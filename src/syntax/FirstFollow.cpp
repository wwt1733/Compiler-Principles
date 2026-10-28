/******************************************************************************
 * (1) 版权信息 : 编译原理课程设计 - 华南师范大学计算机学院
 * (2) 文件名称 : FirstFollow.cpp
 *     标识符   : FirstFollow
 *     摘要     : First 集与 Follow 集计算的实现（第二项任务 2.3）。
 * (3) 版本号   : 1.0.0       作者/修改者 : <学号 姓名>   完成日期 : 2026-XX-XX
 * (4) 版本历史 : 1.0.0  2026-XX-XX  首次实现（阶段 4）
 *****************************************************************************/

#include "syntax/FirstFollow.h"

#include <QStringList>

namespace ccd {

const QString kEndMarker = QStringLiteral("$");

/**
 * 功能：求一个符号序列的 First 集。
 * 输入：sequence —— 符号序列。
 * 输出：First 集（不含 ε）。
 * 注意（与讲稿推导式一致）：
 *  从第一个符号开始，逐个把它的 First 并入结果；
 *  一旦遇到"不可空"的符号就停止 —— 因为该符号必然消耗掉一个终结符，
 *  它后面的符号不可能再成为首符号。
 *
 *  **关键细节**：结束符 `$`（kEndMarker）不是文法里的终结符，因此
 *  `grammar.isTerminal("$")` 为 false。但它在 FIRST(βa) 里起着终结符的作用，
 *  必须显式当作终结符处理，否则 FIRST(β$) 会算出空集，
 *  进而使 LR(1) 闭包一个项目都展开不出来（整个分析表退化成 2 个状态）。
 *  这是本模块一个真实踩过的坑，自测 T14.3f/T14.3g 专门钉住。
 */
QSet<QString> FirstFollowResult::firstSetOfSequence(const QStringList &sequence) const
{
    QSet<QString> result;
    for (const QString &s : sequence) {
        if (s == kEndMarker || grammar.isTerminal(s)) {
            result.insert(s);
            // 终结符不可空：必须停下
            break;
        }
        const QSet<QString> &f = first.value(s);
        result.unite(f);
        if (!nullable.contains(s))
            break;
    }
    return result;
}

/**
 * 功能：判断符号序列是否可推出空串。
 * 输入：sequence —— 符号序列。
 * 输出：可空为 true。
 * 注意：空序列视为可空（这是归纳基例，也让 followSet 的计算自然处理
 *       "A 位于产生式末尾"的情形）。
 *       `$` 与终结符一样不可空，必须返回 false —— 与 firstSetOfSequence
 *       保持同一套"什么算终结符"的口径，否则两处判断会不一致。
 */
bool FirstFollowResult::sequenceNullable(const QStringList &sequence) const
{
    for (const QString &s : sequence) {
        if (s == kEndMarker || grammar.isTerminal(s))
            return false;
        if (!nullable.contains(s))
            return false;
    }
    return true;
}

/**
 * 功能：把集合格式化为可读文本。
 * 输入：set —— 符号集合。
 * 输出：排序后用顿号连接的文本，形如 "{ LT、LTEQ、RT }"；空集为 "{ }"。
 * 注意：必须**排序**后再输出。QSet 的遍历顺序依赖内部哈希，不稳定，
 *       若直接输出会导致同一个文法两次计算的显示结果不同，
 *       测试断言与截图都无法固定下来。
 */
QString FirstFollowResult::formatSet(const QSet<QString> &set)
{
    QStringList items(set.constBegin(), set.constEnd());
    items.sort();
    if (items.isEmpty())
        return QStringLiteral("{ }");
    return QStringLiteral("{ %1 }").arg(items.join(QStringLiteral("、")));
}

/** 功能：一句话摘要。输入：无。输出：摘要文本。 */
QString FirstFollowResult::summary() const
{
    return QStringLiteral("非终结符 %1 个；可空 %2 个；First 集合计 %3 个元素；"
                          "Follow 集合计 %4 个元素")
        .arg(grammar.nonterminals().size())
        .arg(nullable.size())
        .arg([this] {
            int n = 0;
            for (auto it = first.constBegin(); it != first.constEnd(); ++it)
                n += it.value().size();
            return n;
        }())
        .arg([this] {
            int n = 0;
            for (auto it = follow.constBegin(); it != follow.constEnd(); ++it)
                n += it.value().size();
            return n;
        }());
}

/**
 * 功能：求可推出空串的非终结符集合。
 * 输入：grammar —— 文法；steps —— 输出逐步说明。
 * 输出：可空非终结符集合。
 * 注意（不动点迭代）：
 *  1. ε 产生式（右部为空）⇒ 左部直接可空；
 *  2. 产生式 A -> X1…Xn 中，若每个 Xi 都是"已判定的可空非终结符"，则 A 可空；
 *  3. 反复扫描至不再变化；一条链 `A -> B`、`B -> C`、`C -> #` 需要三轮。
 */
QSet<QString> FirstFollow::computeNullable(const Grammar &grammar, QStringList &steps) const
{
    QSet<QString> nullable;

    int round = 0;
    bool changed = true;
    while (changed) {
        changed = false;
        ++round;
        QStringList added;

        for (const Production &p : grammar.productions()) {
            if (nullable.contains(p.left))
                continue;
            if (p.isEpsilon()) {
                nullable.insert(p.left);
                added.append(p.left);
                changed = true;
                continue;
            }
            bool allNullable = true;
            for (const QString &s : p.right) {
                if (!grammar.isNonterminal(s) || !nullable.contains(s)) {
                    allNullable = false;
                    break;
                }
            }
            if (allNullable) {
                nullable.insert(p.left);
                added.append(p.left);
                changed = true;
            }
        }
        if (!added.isEmpty()) {
            steps << QStringLiteral("可空迭代第 %1 轮新增：%2")
                         .arg(round)
                         .arg(added.join(QStringLiteral("、")));
        }
        if (round > grammar.nonterminals().size() + 2)
            break;
    }

    return nullable;
}

/**
 * 功能：求全部非终结符的 First 集。
 * 输入：grammar —— 文法；nullable —— 可空集合；steps —— 输出逐步说明。
 * 输出：非终结符 → First 集。
 * 注意（不动点迭代，与讲稿的推导式逐条对应）：
 *  对每条产生式 A -> X1 X2 … Xn：
 *    ① 把 First(X1) 并入 First(A)；
 *    ② 若 X1 可空，继续把 First(X2) 并入，依此类推；
 *    ③ 直到遇到不可空的符号为止；
 *    ④ 若全部 Xi 都可空，A 可空（已由 computeNullable 处理），
 *       First(A) 不额外加入 ε（本实现约定 First 集不含 ε）。
 */
QMap<QString, QSet<QString>> FirstFollow::computeFirst(const Grammar &grammar,
                                                       const QSet<QString> &nullable,
                                                       QStringList &steps) const
{
    QMap<QString, QSet<QString>> first;
    for (const QString &nt : grammar.nonterminals())
        first.insert(nt, QSet<QString>());

    int round = 0;
    bool changed = true;
    while (changed) {
        changed = false;
        ++round;
        QStringList added;

        for (const Production &p : grammar.productions()) {
            QSet<QString> &target = first[p.left];
            const int before = target.size();

            for (const QString &s : p.right) {
                if (grammar.isTerminal(s)) {
                    target.insert(s);
                    // 终结符不可空，后面的符号不可能是首符号
                    break;
                }
                target.unite(first.value(s));
                if (!nullable.contains(s))
                    break;
            }

            if (target.size() != before) {
                added.append(p.left);
                changed = true;
            }
        }
        if (!added.isEmpty()) {
            // 去重后报告本轮哪些左部发生了变化
            QStringList unique = added;
            unique.removeDuplicates();
            steps << QStringLiteral("First 迭代第 %1 轮变化：%2")
                         .arg(round)
                         .arg(unique.join(QStringLiteral("、")));
        }
        if (round > grammar.nonterminals().size() * 2 + 4)
            break;
    }

    return first;
}

/**
 * 功能：求全部非终结符的 Follow 集。
 * 输入：grammar / nullable / first / steps。
 * 输出：非终结符 → Follow 集（含结束符 $）。
 * 注意（不动点迭代，与讲稿的推导式逐条对应）：
 *  1. 开始符号的 Follow 补上 `$`（若已增广，`S'` 是开始符号，
 *     `$` 会经由 `S' -> S` 自然传给 S；这里的兜底是为了"未增广也能用"）；
 *  2. 对每条产生式 A -> X1…Xn，对每个 Xi（非终结符），令 β = Xi+1…Xn：
 *     ① 把 First(β) 并入 Follow(Xi)；
 *     ② 若 β 可空（含 β 为空的情形），把 Follow(A) 并入 Follow(Xi)；
 *  3. 反复扫描至不再变化。
 */
QMap<QString, QSet<QString>> FirstFollow::computeFollow(const Grammar &grammar,
                                                        const QSet<QString> &nullable,
                                                        const QMap<QString, QSet<QString>> &first,
                                                        QStringList &steps) const
{
    QMap<QString, QSet<QString>> follow;
    for (const QString &nt : grammar.nonterminals())
        follow.insert(nt, QSet<QString>());

    // 开始符号的 Follow 含结束符
    if (!grammar.startSymbol().isEmpty())
        follow[grammar.startSymbol()].insert(kEndMarker);

    int round = 0;
    bool changed = true;
    while (changed) {
        changed = false;
        ++round;
        QStringList added;

        for (const Production &p : grammar.productions()) {
            for (int i = 0; i < p.right.size(); ++i) {
                const QString &sym = p.right.at(i);
                if (!grammar.isNonterminal(sym))
                    continue;

                const QStringList beta = p.right.mid(i + 1);

                // ① First(β) 并入 Follow(sym)
                const int before = follow[sym].size();
                for (const QString &t : beta) {
                    if (grammar.isTerminal(t)) {
                        follow[sym].insert(t);
                        break;
                    }
                    follow[sym].unite(first.value(t));
                    if (!nullable.contains(t))
                        break;
                }

                // ② β 可空 ⇒ Follow(A) 并入 Follow(sym)
                if (beta.isEmpty() || [&] {
                        for (const QString &t : beta) {
                            if (grammar.isTerminal(t) || !nullable.contains(t))
                                return false;
                        }
                        return true;
                    }()) {
                    follow[sym].unite(follow.value(p.left));
                }

                if (follow[sym].size() != before) {
                    added.append(sym);
                    changed = true;
                }
            }
        }
        if (!added.isEmpty()) {
            QStringList unique = added;
            unique.removeDuplicates();
            steps << QStringLiteral("Follow 迭代第 %1 轮变化：%2")
                         .arg(round)
                         .arg(unique.join(QStringLiteral("、")));
        }
        if (round > grammar.nonterminals().size() * 2 + 4)
            break;
    }

    return follow;
}

/**
 * 功能：计算 First、Follow 与可空集合。
 * 输入：grammar —— 文法（建议已增广）。
 * 输出：计算结果。
 * 注意：三步有严格依赖顺序 —— 先求可空，再求 First（依赖可空），
 *       最后求 Follow（依赖可空与 First）。不能并行，也不可换序。
 */
FirstFollowResult FirstFollow::compute(const Grammar &grammar) const
{
    m_error.clear();
    FirstFollowResult result;
    result.grammar = grammar;

    if (grammar.productionCount() == 0) {
        m_error = QStringLiteral("文法为空，无法计算 First/Follow。");
        return result;
    }

    result.steps << QStringLiteral("---- 第一步：求可空（nullable）非终结符 ----");
    result.nullable = computeNullable(grammar, result.steps);
    result.steps << QStringLiteral("可空集合（%1 个）：%2")
                        .arg(result.nullable.size())
                        .arg(result.nullable.isEmpty()
                                 ? QStringLiteral("{ }")
                                 : QStringList(result.nullable.constBegin(),
                                               result.nullable.constEnd())
                                       .join(QStringLiteral("、")));

    result.steps << QString();
    result.steps << QStringLiteral("---- 第二步：求 First 集 ----");
    result.first = computeFirst(grammar, result.nullable, result.steps);

    result.steps << QString();
    result.steps << QStringLiteral("---- 第三步：求 Follow 集 ----");
    result.follow = computeFollow(grammar, result.nullable, result.first, result.steps);

    return result;
}

}  // namespace ccd
