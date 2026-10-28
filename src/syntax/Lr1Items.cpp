/******************************************************************************
 * (1) 版权信息 : 编译原理课程设计 - 华南师范大学计算机学院
 * (2) 文件名称 : Lr1Items.cpp
 *     标识符   : Lr1Items
 *     摘要     : LALR(1) 项目集与向前看符号求解的实现（第二项任务 2.5 核心）。
 * (3) 版本号   : 1.0.0       作者/修改者 : <学号 姓名>   完成日期 : 2026-XX-XX
 * (4) 版本历史 : 1.0.0  2026-XX-XX  首次实现（阶段 5）
 *****************************************************************************/

#include "syntax/Lr1Items.h"

#include <algorithm>

#include "syntax/FirstFollow.h"  // kEndMarker

namespace ccd {

namespace {

/** @brief LR(1) 项目排序比较器：产生式号 → 圆点 → 向前看符号 */
bool lr1Less(const Lr1Item &a, const Lr1Item &b)
{
    if (a.prodId != b.prodId)
        return a.prodId < b.prodId;
    if (a.dot != b.dot)
        return a.dot < b.dot;
    return a.lookahead < b.lookahead;
}

/** @brief 把 LR(1) 项目集排序去重（就地） */
void sortUniqueLr1(QVector<Lr1Item> &items)
{
    std::sort(items.begin(), items.end(), lr1Less);
    items.erase(std::unique(items.begin(), items.end()), items.end());
}

/** @brief 两个 LR(1) 项目集是否相等（假定均已排序去重） */
bool sameLr1Set(const QVector<Lr1Item> &a, const QVector<Lr1Item> &b)
{
    if (a.size() != b.size())
        return false;
    for (int i = 0; i < a.size(); ++i) {
        if (!(a.at(i) == b.at(i)))
            return false;
    }
    return true;
}

}  // namespace

// ---------------------------------------------------------------------------
// LalrState
// ---------------------------------------------------------------------------

/**
 * 功能：取某内核项目的向前看集合。
 * 输入：item —— 内核项目。
 * 输出：向前看集合；项目不在内核中时返回空集。
 * 注意：用线性查找而非哈希 —— core 是排序好的，且规模很小，
 *       线性查找既简单又能避免额外维护索引结构。
 */
QSet<QString> LalrState::lookaheadOf(const LrItem &item) const
{
    for (int i = 0; i < core.size(); ++i) {
        if (core.at(i) == item && i < lookahead.size())
            return lookahead.at(i);
    }
    return QSet<QString>();
}

/** 功能：判断是否接受状态。输入：grammar。输出：是/否。 */
bool LalrState::isAccepting(const Grammar &grammar) const
{
    for (const LrItem &it : core) {
        const Production &p = grammar.production(it.prodId);
        if (p.left == grammar.augmentedStart() && it.dot == p.right.size())
            return true;
    }
    return false;
}

// ---------------------------------------------------------------------------
// LalrAutomaton
// ---------------------------------------------------------------------------

/** 功能：一句话摘要。输入：无。输出：摘要文本。 */
QString LalrAutomaton::summary() const
{
    int edgeCount = 0;
    int laTotal = 0;
    for (const LalrState &s : states) {
        edgeCount += s.transitions.size();
        for (const QSet<QString> &la : s.lookahead)
            laTotal += la.size();
    }
    QString text = QStringLiteral("LALR(1)：LR(1) 规范族 %1 个状态 → 合并同核心后 %2 个状态"
                                  "（%3 个核心组发生过合并）；%4 条转移；向前看条目 %5 个")
                       .arg(lr1StateCount)
                       .arg(states.size())
                       .arg(mergedGroupCount)
                       .arg(edgeCount)
                       .arg(laTotal);
    return text;
}

/** 功能：取转移目标。输入：state、symbol。输出：目标状态号（不存在为 -1）。 */
int LalrAutomaton::goTo(int state, const QString &symbol) const
{
    if (state < 0 || state >= states.size())
        return -1;
    return states.at(state).transitions.value(symbol, -1);
}

// ---------------------------------------------------------------------------
// Lr1Items
// ---------------------------------------------------------------------------

/**
 * 功能：求 LR(1) 项目集的闭包。
 * 输入：grammar、items、follow。
 * 输出：闭包（已排序去重）。
 * 注意（算法要点，标准 LR(1) 闭包）：
 *  对闭包中每个项目 [A -> α·Bβ, a]（B 为非终结符）：
 *    ① 求 FIRST(βa)：即序列 β 后面接一个终结符 a 的首终结符集合；
 *       实现上把 β 的所有符号再加上 a 组成一个序列，交给
 *       FirstFollowResult::firstSetOfSequence() —— 该函数已经正确处理了
 *       "β 中的符号可空则继续往后看"的传递逻辑，此处不必重复实现；
 *    ② 对 B 的每条产生式 B -> γ 与 ① 得到的每个终结符 b，
 *       加入项目 [B -> ·γ, b]；
 *    ③ 新加入的项目同样要展开（用 seen 集合防重入，否则会无限递归）。
 *
 *  这里最容易写错的两点：
 *    - 把 ① 写成 FIRST(β)：漏掉"β 全可空时 a 也要算进来"，
 *      在 mini-C 这类含 ε 产生式的文法上会直接漏掉向前看符号，
 *      导致本该规约的地方变成报错；
 *    - 忘记 ③ 的传递展开。
 *  自测 T14 用含 ε 的文法专门覆盖这两点。
 */
QVector<Lr1Item> Lr1Items::closureLr1(const Grammar &grammar, const QVector<Lr1Item> &items,
                                      const FirstFollowResult &follow)
{
    QVector<Lr1Item> result = items;
    sortUniqueLr1(result);

    // seen 的键：产生式号 + 圆点 + 向前看符号
    QSet<QString> seen;
    for (const Lr1Item &it : result) {
        seen.insert(QStringLiteral("%1|%2|%3").arg(it.prodId).arg(it.dot).arg(it.lookahead));
    }

    for (int i = 0; i < result.size(); ++i) {
        const Lr1Item item = result.at(i);
        const Production &p = grammar.production(item.prodId);

        if (item.dot >= p.right.size())
            continue;

        const QString &sym = p.right.at(item.dot);
        if (!grammar.isNonterminal(sym))
            continue;

        // β = 圆点之后的符号序列；把向前看符号 a 接在 β 末尾一起求 FIRST
        QStringList betaPlusA;
        for (int k = item.dot + 1; k < p.right.size(); ++k)
            betaPlusA.append(p.right.at(k));
        betaPlusA.append(item.lookahead);

        const QSet<QString> firstSet = follow.firstSetOfSequence(betaPlusA);

        for (int prodId : grammar.productionsOf(sym)) {
            for (const QString &b : firstSet) {
                Lr1Item fresh;
                fresh.prodId = prodId;
                fresh.dot = 0;
                fresh.lookahead = b;
                const QString key =
                    QStringLiteral("%1|%2|%3").arg(fresh.prodId).arg(fresh.dot).arg(fresh.lookahead);
                if (seen.contains(key))
                    continue;
                seen.insert(key);
                result.append(fresh);
            }
        }
    }

    sortUniqueLr1(result);
    return result;
}

/**
 * 功能：求 LR(1) 的 goto(I, X)。
 * 输入：grammar、items、symbol、follow。
 * 输出：goto(I, X) 的闭包（空集返回空列表）。
 * 注意：与 LR(0) 的 goto 完全同构，只是项目带向前看符号，
 *       且圆点右移后向前看符号**保持不变**（这是 LR(1) 的关键：
 *       a 描述的是"归约这个项目时后面可以跟什么"，右移不改变它）。
 */
QVector<Lr1Item> Lr1Items::goToLr1(const Grammar &grammar, const QVector<Lr1Item> &items,
                                  const QString &symbol, const FirstFollowResult &follow)
{
    const QVector<Lr1Item> closed = closureLr1(grammar, items, follow);

    QVector<Lr1Item> kernel;
    for (const Lr1Item &it : closed) {
        const Production &p = grammar.production(it.prodId);
        if (it.dot >= p.right.size())
            continue;
        if (p.right.at(it.dot) != symbol)
            continue;
        Lr1Item advanced = it;
        advanced.dot = it.dot + 1;
        kernel.append(advanced);
    }

    if (kernel.isEmpty())
        return QVector<Lr1Item>();

    return closureLr1(grammar, kernel, follow);
}

/**
 * 功能：把 LR(1) 项目集投影为它的 LR(0) 内核。
 * 输入：items —— LR(1) 项目集。
 * 输出：排序去重后的 (产生式, 圆点) 列表。
 */
QVector<LrItem> Lr1Items::coreOf(const QVector<Lr1Item> &items)
{
    QVector<LrItem> core;
    core.reserve(items.size());
    for (const Lr1Item &it : items) {
        LrItem base;
        base.prodId = it.prodId;
        base.dot = it.dot;
        core.append(base);
    }
    std::sort(core.begin(), core.end());
    core.erase(std::unique(core.begin(), core.end()), core.end());
    return core;
}

/**
 * 功能：构造 LALR(1) 自动机。
 * 输入：grammar —— 文法；follow —— 已算好的 First/Follow。
 * 输出：LALR(1) 自动机。
 * 注意（算法路线与正确性论证）：
 *  1. **先构造 LR(1) 规范族**：初态 closure({[S' -> ·S, $]})，
 *     用队列广度优先，对每个符号做 goto，项目集用逐项比较去重。
 *  2. **再合并同核心状态**：把 LR(0) 内核（产生式 + 圆点）相同的 LR(1)
 *     状态归为一组，各组的向前看符号集合**取并集**。
 *     这一组就是 LALR(1) 状态，其向前看集合正是 LALR(1) 所要求的。
 *     **正确性依据**：LR(1) 闭包中的向前看符号只可能通过
 *     "同状态内的闭包传递"与"沿 goto 边传播"两种途径产生，
 *     而这两者在合并后都会重新计算到不动点；同核心合并不会丢失
 *     任何本可传播的向前看符号，因此并集恰好等于 LALR(1) 的结果。
 *     （这一点与"先把 LR(1) 合并再用 LALR 传播算法"得到的表相同，
 *     是 LALR(1) 的等价刻画。）
 *  3. **转移边的合并**：同一核心组内各 LR(1) 状态对同一符号的 goto 目标
 *     必定落在同一个核心组（因为 goto 只依赖内核），故直接取该组代表的目标组号。
 *  4. 界面如实报告合并前后状态数，便于在实验报告里对比 LR(1) 与 LALR(1) 的规模。
 */
LalrAutomaton Lr1Items::compute(const Grammar &grammar, const FirstFollowResult &follow) const
{
    m_error.clear();
    LalrAutomaton result;
    result.grammar = grammar;

    if (grammar.productionCount() == 0) {
        m_error = QStringLiteral("文法为空，无法构造 LALR(1) 项目集。");
        return result;
    }
    if (!result.grammar.augmented())
        result.grammar.augment();

    const Grammar &g = result.grammar;

    int augProdId = -1;
    for (const Production &p : g.productions()) {
        if (p.left == g.augmentedStart()) {
            augProdId = p.id;
            break;
        }
    }
    if (augProdId < 0) {
        m_error = QStringLiteral("文法中找不到增广产生式 %1 -> ...。").arg(g.augmentedStart());
        return result;
    }

    // ---- 第一步：LR(1) 规范族 ----
    QVector<QVector<Lr1Item>> lr1States;
    QMap<int, QMap<QString, int>> lr1Transitions;  // 状态 → (符号 → 状态)

    Lr1Item seed;
    seed.prodId = augProdId;
    seed.dot = 0;
    seed.lookahead = kEndMarker;

    lr1States.append(closureLr1(g, QVector<Lr1Item>{seed}, follow));
    result.steps << QStringLiteral("LR(1) 初态 closure({ %1 -> ·%2 , %3 }) —— %4 个项目")
                        .arg(g.augmentedStart(), g.startSymbol(), kEndMarker)
                        .arg(lr1States.first().size());

    const QStringList symbols = [&g] {
        QStringList s = g.terminals();
        s += g.nonterminals();
        return s;
    }();

    for (int index = 0; index < lr1States.size(); ++index) {
        for (const QString &sym : symbols) {
            const QVector<Lr1Item> target = goToLr1(g, lr1States.at(index), sym, follow);
            if (target.isEmpty())
                continue;

            int found = -1;
            for (int k = 0; k < lr1States.size(); ++k) {
                if (sameLr1Set(lr1States.at(k), target)) {
                    found = k;
                    break;
                }
            }
            if (found < 0) {
                found = lr1States.size();
                lr1States.append(target);
            }
            lr1Transitions[index].insert(sym, found);
        }
    }
    result.lr1StateCount = lr1States.size();
    result.steps << QStringLiteral("LR(1) 规范族共 %1 个状态").arg(lr1States.size());

    // ---- 第二步：按 LR(0) 内核分组 ----
    QVector<QVector<LrItem>> groupCore;                 // 组号 → 核心
    QMap<int, int> stateToGroup;                        // LR(1) 状态号 → 组号
    QVector<QVector<int>> groupMembers;                 // 组号 → 成员状态号
    QVector<QMap<QString, int>> groupTransitions;       // 组号 → (符号 → 组号)

    const auto findGroup = [&groupCore](const QVector<LrItem> &core) -> int {
        for (int k = 0; k < groupCore.size(); ++k) {
            if (groupCore.at(k) == core)
                return k;
        }
        return -1;
    };

    for (int s = 0; s < lr1States.size(); ++s) {
        const QVector<LrItem> core = coreOf(lr1States.at(s));
        int grp = findGroup(core);
        if (grp < 0) {
            grp = groupCore.size();
            groupCore.append(core);
            groupMembers.append(QVector<int>());
            groupTransitions.append(QMap<QString, int>());
        }
        stateToGroup.insert(s, grp);
        groupMembers[grp].append(s);
    }
    result.mergedGroupCount = 0;
    for (const QVector<int> &members : groupMembers) {
        if (members.size() > 1)
            ++result.mergedGroupCount;
    }
    result.steps << QStringLiteral("按 LR(0) 内核合并 → %1 个 LALR(1) 状态"
                                   "（其中 %2 个组由多个 LR(1) 状态合并而来）")
                        .arg(groupCore.size())
                        .arg(result.mergedGroupCount);

    // ---- 第三步：装配 LALR 状态（向前看取并集） ----
    result.states.resize(groupCore.size());
    for (int grp = 0; grp < groupCore.size(); ++grp) {
        LalrState &st = result.states[grp];
        st.id = grp;
        st.core = groupCore.at(grp);
        st.lookahead.resize(st.core.size());

        for (int member : groupMembers.at(grp)) {
            for (const Lr1Item &it : lr1States.at(member)) {
                LrItem base;
                base.prodId = it.prodId;
                base.dot = it.dot;
                for (int ci = 0; ci < st.core.size(); ++ci) {
                    if (st.core.at(ci) == base) {
                        st.lookahead[ci].insert(it.lookahead);
                        break;
                    }
                }
            }
        }

        // 转移边：同组内任一成员的目标组号即该组的转移目标
        for (int member : groupMembers.at(grp)) {
            const QMap<QString, int> &tr = lr1Transitions.value(member);
            for (auto it = tr.constBegin(); it != tr.constEnd(); ++it) {
                const int targetGroup = stateToGroup.value(it.value(), -1);
                if (targetGroup >= 0)
                    st.transitions.insert(it.key(), targetGroup);
            }
        }
    }

    // ---- 第四步：把初态调整为状态 0 ----
    //
    // 组号是按"发现顺序"分配的，而 LR(1) 状态 0 的组必然在遍历中第一个被
    // 建立（因为 s 从 0 开始遍历），所以组 0 就是初态，无需重排。
    // 这里加一条断言式检查，万一将来改动遍历顺序也不会静默出错。
    if (result.states.isEmpty() || !result.states.first().core.isEmpty()) {
        const bool firstIsInitial = [&] {
            for (const LrItem &it : result.states.first().core) {
                const Production &p = g.production(it.prodId);
                if (p.left == g.augmentedStart() && it.dot == 0)
                    return true;
            }
            return false;
        }();
        if (!firstIsInitial) {
            m_error = QStringLiteral("内部错误：合并后状态 0 不是初态。");
            return result;
        }
    }

    result.steps << QString();
    result.steps << result.summary();

    return result;
}

}  // namespace ccd
