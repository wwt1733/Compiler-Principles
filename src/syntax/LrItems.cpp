/******************************************************************************
 * (1) 版权信息 : 编译原理课程设计 - 华南师范大学计算机学院
 * (2) 文件名称 : LrItems.cpp
 *     标识符   : LrItems
 *     摘要     : LR(0) 项目集规范族构造的实现（第二项任务 2.4）。
 * (3) 版本号   : 1.0.0       作者/修改者 : <学号 姓名>   完成日期 : 2026-XX-XX
 * (4) 版本历史 : 1.0.0  2026-XX-XX  首次实现（阶段 5）
 *****************************************************************************/

#include "syntax/LrItems.h"

#include <QQueue>

#include <algorithm>

namespace ccd {

namespace {

/**
 * 功能：把项目格式化为可读文本，形如 "exp -> simple-exp · comparison-op simple-exp"。
 * 输入：grammar —— 文法；item —— 项目。
 * 输出：文本。
 * 注意：圆点位置用 `·`（U+00B7）而非 `.` —— 小数点容易与产生式里的符号混淆，
 *       而且实验报告里也是这个习惯写法。
 */
QString formatItem(const Grammar &grammar, const LrItem &item)
{
    const Production &p = grammar.production(item.prodId);
    if (p.left.isEmpty())
        return QStringLiteral("(无效项目 r%1)").arg(item.prodId);

    QStringList parts;
    for (int i = 0; i < p.right.size(); ++i) {
        if (i == item.dot)
            parts << QStringLiteral("·");
        parts << p.right.at(i);
    }
    if (item.dot >= p.right.size())
        parts << QStringLiteral("·");

    return QStringLiteral("%1 -> %2").arg(p.left, parts.join(QLatin1Char(' ')));
}

/**
 * 功能：项目排序比较器（按产生式号，再按圆点位置）。
 * 输入：a、b —— 两个项目。
 * 输出：a 排在 b 前面为 true。
 * 注意：必须提供**全序**才能让项目集有唯一表示，这是状态去重的前提。
 *       LrItem 已自带 operator<，此处直接复用即可，无需另写。
 */

/** @brief 把项目集排序去重（就地） */
void sortUnique(QVector<LrItem> &items)
{
    std::sort(items.begin(), items.end());
    items.erase(std::unique(items.begin(), items.end()), items.end());
}

}  // namespace

// ---------------------------------------------------------------------------
// Lr0State
// ---------------------------------------------------------------------------

/** 功能：判断状态是否含增广开始项目的完成态。输入：grammar。输出：是/否。 */
bool Lr0State::isAccepting(const Grammar &grammar) const
{
    for (const LrItem &it : items) {
        const Production &p = grammar.production(it.prodId);
        if (p.left == grammar.augmentedStart() && it.dot == p.right.size())
            return true;
    }
    return false;
}

/** 功能：判断状态是否含圆点在增广产生式最左端的项目。输入：grammar。输出：是/否。 */
bool Lr0State::containsAugmentedStart(const Grammar &grammar) const
{
    for (const LrItem &it : items) {
        const Production &p = grammar.production(it.prodId);
        if (p.left == grammar.augmentedStart() && it.dot == 0)
            return true;
    }
    return false;
}

/** 功能：项目集文本。输入：grammar。输出：一行一个项目的文本。 */
QString Lr0State::toText(const Grammar &grammar) const
{
    QStringList lines;
    for (const LrItem &it : items)
        lines << formatItem(grammar, it);
    return lines.join(QLatin1Char('\n'));
}

// ---------------------------------------------------------------------------
// Lr0Automaton
// ---------------------------------------------------------------------------

/** 功能：一句话摘要。输入：无。输出：摘要文本。 */
QString Lr0Automaton::summary() const
{
    int edgeCount = 0;
    for (const Lr0State &s : states)
        edgeCount += s.transitions.size();
    return QStringLiteral("LR(0) 项目集规范族：%1 个状态、%2 条转移；文法产生式 %3 条")
        .arg(states.size())
        .arg(edgeCount)
        .arg(grammar.productionCount());
}

/** 功能：取转移目标。输入：state、symbol。输出：目标状态号（不存在为 -1）。 */
int Lr0Automaton::goTo(int state, const QString &symbol) const
{
    if (state < 0 || state >= states.size())
        return -1;
    return states.at(state).transitions.value(symbol, -1);
}

/** 功能：全部文法符号（终结符在前）。输入：无。输出：符号列表。 */
QStringList Lr0Automaton::allSymbols() const
{
    QStringList result = grammar.terminals();
    result += grammar.nonterminals();
    return result;
}

/** 功能：状态项目集文本。输入：state。输出：文本（越界返回空串）。 */
QString Lr0Automaton::stateItemsText(int state) const
{
    if (state < 0 || state >= states.size())
        return QString();
    return states.at(state).toText(grammar);
}

// ---------------------------------------------------------------------------
// LrItems
// ---------------------------------------------------------------------------

/**
 * 功能：求项目集的闭包 closure(I)。
 * 输入：grammar —— 文法；items —— 种子项目集。
 * 输出：闭包（已排序去重）。
 * 注意（算法要点）：
 *  1. 对 I 中每个项目 A -> α·Bβ（圆点后是非终结符 B），把 B 的**每条**
 *     产生式 B -> γ 以圆点在 0 位的项目 B -> ·γ 加入 I；
 *  2. 新加入的项目同样要处理 —— 例如 A -> ·B、B -> ·C 会连锁展开到 C，
 *     所以必须用工作列表循环到不再新增，而不是只扫一遍种子；
 *  3. ε 产生式的项目形如 A -> ·（圆点在末尾且右部为空），也要正常加入。
 */
QVector<LrItem> LrItems::closure(const Grammar &grammar, const QVector<LrItem> &items)
{
    QVector<LrItem> result = items;
    sortUnique(result);

    // 已处理集合：避免同一个项目反复展开（否则 ε 产生式会死循环）
    QSet<qint64> seen;
    for (const LrItem &it : result)
        seen.insert((static_cast<qint64>(it.prodId) << 32) | static_cast<quint32>(it.dot));

    // 用下标遍历即可：result 只会增长，新元素会被后续循环处理到
    for (int i = 0; i < result.size(); ++i) {
        const LrItem item = result.at(i);
        const Production &p = grammar.production(item.prodId);

        // 圆点已到末尾：没有可展开的符号
        if (item.dot >= p.right.size())
            continue;

        const QString &sym = p.right.at(item.dot);
        if (!grammar.isNonterminal(sym))
            continue;

        for (int prodId : grammar.productionsOf(sym)) {
            LrItem fresh;
            fresh.prodId = prodId;
            fresh.dot = 0;
            const qint64 key = (static_cast<qint64>(fresh.prodId) << 32);
            if (seen.contains(key))
                continue;
            seen.insert(key);
            result.append(fresh);
        }
    }

    sortUnique(result);
    return result;
}

/**
 * 功能：求 goto(I, X)。
 * 输入：grammar —— 文法；items —— 项目集 I；symbol —— 文法符号 X。
 * 输出：goto(I, X) 的闭包（空集时返回空列表）。
 * 注意（算法要点）：
 *  1. 先对 I 取闭包（若调用方已传闭包，这一步是幂等的，不额外付代价）；
 *  2. 对每个形如 A -> α·Xβ 的项目，产生项目 A -> αX·β（圆点右移一位）；
 *  3. 对得到的**内核**再取一次闭包，才是 goto 的结果。
 *     漏掉第 3 步是 LR 构造最常见的错误：那样 goto 得到的项目集
 *     缺少由新圆点后非终结符展开出来的项目，后续状态会整体缺失。
 */
QVector<LrItem> LrItems::goTo(const Grammar &grammar, const QVector<LrItem> &items,
                              const QString &symbol)
{
    const QVector<LrItem> closed = closure(grammar, items);

    QVector<LrItem> kernel;
    for (const LrItem &it : closed) {
        const Production &p = grammar.production(it.prodId);
        if (it.dot >= p.right.size())
            continue;
        if (p.right.at(it.dot) != symbol)
            continue;
        LrItem advanced;
        advanced.prodId = it.prodId;
        advanced.dot = it.dot + 1;
        kernel.append(advanced);
    }

    if (kernel.isEmpty())
        return QVector<LrItem>();

    return closure(grammar, kernel);
}

/**
 * 功能：构造 LR(0) 项目集规范族。
 * 输入：grammar —— 文法（建议已增广）。
 * 输出：LR(0) 自动机。
 * 注意（算法要点，标准子集构造法）：
 *  1. 初态 I0 = closure({S' -> ·S})；
 *  2. 用队列做广度优先：对每个未处理状态 I，对**每个文法符号** X 求 goto(I,X)，
 *     非空则找已存在的同项目集状态（有则连边，无则新建状态入队）；
 *  3. 项目集比较用**排序去重后的列表逐项相等**，不用哈希 ——
 *     文法规模小（状态数几十到几百），逐项比较足够快且不会有哈希碰撞风险；
 *  4. 增广产生式的项目 S' -> S · 所在状态即"接受状态"，后续分析表在此填 acc。
 */
Lr0Automaton LrItems::build(const Grammar &grammar) const
{
    m_error.clear();
    Lr0Automaton result;
    result.grammar = grammar;

    if (grammar.productionCount() == 0) {
        m_error = QStringLiteral("文法为空，无法构造 LR(0) 项目集。");
        return result;
    }

    // 未增广时自行增广（推荐调用方显式增广，见头文件说明）
    if (!result.grammar.augmented())
        result.grammar.augment();

    const Grammar &g = result.grammar;
    const QString start = g.augmentedStart();
    if (start.isEmpty()) {
        m_error = QStringLiteral("文法缺少增广开始符号，无法确定初态。");
        return result;
    }

    // 找到增广产生式 S' -> S
    int augProdId = -1;
    for (const Production &p : g.productions()) {
        if (p.left == start) {
            augProdId = p.id;
            break;
        }
    }
    if (augProdId < 0) {
        m_error = QStringLiteral("文法中找不到增广产生式 %1 -> ...。").arg(start);
        return result;
    }

    // ---- 初态 ----
    LrItem seed;
    seed.prodId = augProdId;
    seed.dot = 0;

    QVector<LrItem> initial = closure(g, QVector<LrItem>{seed});

    Lr0State s0;
    s0.items = initial;
    s0.id = 0;
    result.states.append(s0);
    result.steps << QStringLiteral("I0 = closure({ %1 -> ·%2 }) —— 共 %3 个项目")
                        .arg(start, g.startSymbol())
                        .arg(initial.size());

    // ---- 广度优先构造 ----
    const QStringList symbols = result.allSymbols();

    for (int index = 0; index < result.states.size(); ++index) {
        const QVector<LrItem> current = result.states.at(index).items;

        for (const QString &sym : symbols) {
            const QVector<LrItem> target = goTo(g, current, sym);
            if (target.isEmpty())
                continue;

            // 在已有状态里找同一个项目集
            int found = -1;
            for (int k = 0; k < result.states.size(); ++k) {
                if (result.states.at(k).items == target) {
                    found = k;
                    break;
                }
            }

            if (found < 0) {
                Lr0State fresh;
                fresh.items = target;
                fresh.id = result.states.size();
                found = fresh.id;
                result.states.append(fresh);
                result.steps << QStringLiteral("goto(I%1, %2) = I%3 —— 新状态，%4 个项目")
                                    .arg(index)
                                    .arg(sym)
                                    .arg(found)
                                    .arg(target.size());
            }

            result.states[index].transitions.insert(sym, found);
        }
    }

    result.steps << QString();
    result.steps << result.summary();

    return result;
}

}  // namespace ccd
