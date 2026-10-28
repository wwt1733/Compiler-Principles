/******************************************************************************
 * (1) 版权信息 : 编译原理课程设计 - 华南师范大学计算机学院
 * (2) 文件名称 : GrammarSimplify.cpp
 *     标识符   : GrammarSimplify
 *     摘要     : 文法化简的实现（第二项任务 2.2）。
 * (3) 版本号   : 1.0.0       作者/修改者 : <学号 姓名>   完成日期 : 2026-XX-XX
 * (4) 版本历史 : 1.0.0  2026-XX-XX  首次实现（阶段 4）
 *****************************************************************************/

#include "syntax/GrammarSimplify.h"

#include <QSet>

namespace ccd {

/**
 * 功能：一句话摘要。
 * 输入：无。
 * 输出：形如"删除非终结符 2 个（不可推导 1、不可达 1）、产生式 3 条；剩余 31 条"。
 */
QString SimplifyResult::summary() const
{
    return QStringLiteral("删除非终结符 %1 个（不可推导 %2、不可达 %3）、产生式 %4 条；"
                          "化简后产生式 %5 条")
        .arg(nonProductiveRemoved.size() + unreachableRemoved.size())
        .arg(nonProductiveRemoved.size())
        .arg(unreachableRemoved.size())
        .arg(removedProductionIds.size())
        .arg(kept.productionCount());
}

/**
 * 功能：求"可推导出终结符串"的非终结符集合。
 * 输入：grammar —— 文法；steps —— 输出逐步说明。
 * 输出：可推导的非终结符集合。
 * 注意（算法要点）：
 *  1. 判定条件：存在一条产生式 A -> X1 X2 … Xn，使得每个 Xi 要么是终结符，
 *     要么是"已被判定为可推导的非终结符"。ε 产生式（右部为空）天然满足，
 *     因为空串是终结符串；
 *  2. 用**不动点迭代**：反复扫描全部产生式，只要本轮有新符号被判定，
 *     就再扫一轮，直到不再变化。不能只扫一遍 —— 一条链
 *     `A -> B`、`B -> C`、`C -> x` 需要三轮才能把 A 判定出来；
 *  3. 每轮把新增的符号记进 steps，界面据此展示"第 k 轮推导出哪些"，
 *     这正是测试报告里解释化简过程所需的材料。
 */
QSet<QString> GrammarSimplify::productiveNonterminals(const Grammar &grammar,
                                                      QStringList &steps) const
{
    QSet<QString> productive;

    int round = 0;
    bool changed = true;
    while (changed) {
        changed = false;
        ++round;
        QStringList added;

        for (const Production &p : grammar.productions()) {
            if (productive.contains(p.left))
                continue;

            // ε 产生式：右部为空 ⇒ 可推出空串（属于终结符串）
            if (p.isEpsilon()) {
                productive.insert(p.left);
                added.append(p.left);
                changed = true;
                continue;
            }

            // 右部每个符号都必须是终结符或已判定的可推导非终结符
            bool allOk = true;
            for (const QString &s : p.right) {
                if (grammar.isTerminal(s))
                    continue;
                if (productive.contains(s))
                    continue;
                allOk = false;
                break;
            }
            if (allOk) {
                productive.insert(p.left);
                added.append(p.left);
                changed = true;
            }
        }

        if (!added.isEmpty()) {
            steps << QStringLiteral("第 %1 轮判定可推导：%2")
                         .arg(round)
                         .arg(added.join(QStringLiteral("、")));
        }
        // 防止病态文法导致轮数失控（正常文法轮数不超过非终结符个数）
        if (round > grammar.nonterminals().size() + 2)
            break;
    }

    return productive;
}

/**
 * 功能：求"从开始符号可达"的非终结符集合。
 * 输入：grammar —— 文法；productive —— 可推导集合；steps —— 输出逐步说明。
 * 输出：可达的非终结符集合。
 * 注意（算法要点）：
 *  1. 从开始符号出发做**广度优先**遍历：展开 A 的每条产生式，
 *     把右部出现的非终结符加入队列（终结符不进队列）；
 *  2. 只沿**可推导**的符号前进：若 A 不可推导，它本身就该被删，
 *     再去探索它的右部只会把"死符号"连带保留下来。
 *     这正是"先删不可推导、再删不可达"顺序不可颠倒的原因；
 *  3. 可达集合恒包含开始符号（起点即入队）。
 */
QSet<QString> GrammarSimplify::reachableNonterminals(const Grammar &grammar,
                                                     const QSet<QString> &productive,
                                                     QStringList &steps) const
{
    QSet<QString> reachable;
    QStringList frontier;
    QStringList visitedOrder;

    const QString start = grammar.startSymbol();
    if (start.isEmpty())
        return reachable;

    reachable.insert(start);
    frontier.append(start);

    int round = 0;
    while (!frontier.isEmpty()) {
        ++round;
        QStringList next;
        for (const QString &nt : frontier) {
            for (int id : grammar.productionsOf(nt)) {
                const Production &p = grammar.production(id);
                for (const QString &s : p.right) {
                    if (!grammar.isNonterminal(s))
                        continue;
                    // 不可推导的符号不再前进（它会被删掉）
                    if (!productive.contains(s))
                        continue;
                    if (reachable.contains(s))
                        continue;
                    reachable.insert(s);
                    next.append(s);
                    visitedOrder.append(s);
                }
            }
        }
        if (!next.isEmpty()) {
            steps << QStringLiteral("第 %1 层可达：%2")
                         .arg(round)
                         .arg(next.join(QStringLiteral("、")));
        }
        frontier = next;
        // 防御：层数不可能超过非终结符个数
        if (round > grammar.nonterminals().size() + 2)
            break;
    }

    return reachable;
}

/**
 * 功能：执行文法化简。
 * 输入：grammar —— 待化简文法（不被修改）。
 * 输出：化简结果。
 * 注意（关键决策）：
 *  1. 两步的删除对象**只针对非终结符**，通过"该符号不在保留集合里"来
 *     连带删除所有以它为左部的产生式；
 *  2. 从未作为左部的符号是终结符，永远不会被删；
 *  3. 开始符号恒保留（见头文件说明）；若开始符号本身不可推导，
 *     在 steps 里给出明确警告，而不是静默产出空文法；
 *  4. 化简结果**不做增广** —— 增广由调用方在化简后执行。
 */
SimplifyResult GrammarSimplify::simplify(const Grammar &grammar) const
{
    m_error.clear();
    SimplifyResult result;
    result.original = grammar;

    if (grammar.productionCount() == 0) {
        m_error = QStringLiteral("文法为空，无法化简。");
        result.kept = grammar;
        return result;
    }

    const QString start = grammar.startSymbol();

    // ---- 第一步：删除不可推导的非终结符 ----
    QStringList productiveSteps;
    const QSet<QString> productive = productiveNonterminals(grammar, productiveSteps);

    for (const QString &nt : grammar.nonterminals()) {
        if (!productive.contains(nt) && nt != start)
            result.nonProductiveRemoved.append(nt);
    }
    if (!productive.contains(start)) {
        result.steps << QStringLiteral("警告：开始符号 %1 推不出任何终结符串，"
                                       "按约定保留它，但该文法本身有缺陷。")
                            .arg(start);
    }
    result.steps << QStringLiteral("---- 第一步：求可推导非终结符 ----");
    result.steps += productiveSteps;
    result.steps << QStringLiteral("可推导集合（%1 个）：%2")
                        .arg(productive.size())
                        .arg(QStringList(productive.constBegin(), productive.constEnd())
                                 .join(QStringLiteral("、")));

    // 保留集合 = 可推导 ∧ (是开始符号)
    QSet<QString> keep;
    for (const QString &nt : grammar.nonterminals()) {
        if (productive.contains(nt) || nt == start)
            keep.insert(nt);
    }

    // ---- 第二步：在"已删过不可推导符号"的文法上求可达 ----
    //
    // 注意：这里必须**先构造一个中间文法**（只含 keep 里的左部），
    // 再在其上求可达。若直接在原文法上求可达，那些"只经由死符号才可达"
    // 的符号会被误判为可达。
    //
    // 实现方式：把要保留的产生式逐条拼成 BNF 文本再载入。
    // 这样得到的中间文法与"手工装配符号表"等价，但能复用 loadFromText()
    // 里已经过测试的符号表重建逻辑，不必在这里重复一遍。
    Grammar interim;
    {
        QString interimText;
        for (const Production &p : grammar.productions()) {
            if (!keep.contains(p.left))
                continue;
            interimText += p.isEpsilon()
                               ? QStringLiteral("%1 -> #\n").arg(p.left)
                               : QStringLiteral("%1 -> %2\n").arg(p.left,
                                                                  p.right.join(QLatin1Char(' ')));
        }
        QString loadError;
        if (!interim.loadFromText(interimText, loadError)) {
            m_error = QStringLiteral("化简中间步骤构造失败：%1").arg(loadError);
            result.kept = grammar;
            return result;
        }
        interim.setTerminalNames(grammar.terminalNames());
    }

    QStringList reachableSteps;
    const QSet<QString> reachable = reachableNonterminals(interim, productive, reachableSteps);

    for (const QString &nt : grammar.nonterminals()) {
        if (!keep.contains(nt))
            continue;  // 已在第一步被判定为不可推导
        if (!reachable.contains(nt))
            result.unreachableRemoved.append(nt);
    }
    result.steps << QString();
    result.steps << QStringLiteral("---- 第二步：求从 %1 可达的非终结符 ----").arg(start);
    result.steps += reachableSteps;
    result.steps << QStringLiteral("可达集合（%1 个）：%2")
                        .arg(reachable.size())
                        .arg(QStringList(reachable.constBegin(), reachable.constEnd())
                                 .join(QStringLiteral("、")));

    // ---- 汇总保留集合 ----
    QSet<QString> finalKeep;
    for (const QString &nt : grammar.nonterminals()) {
        if (!keep.contains(nt))
            continue;
        if (!reachable.contains(nt))
            continue;
        finalKeep.insert(nt);
    }
    // 开始符号必须保留
    finalKeep.insert(start);

    // ---- 装配化简后的文法 ----
    QString text;
    for (const Production &p : grammar.productions()) {
        if (!finalKeep.contains(p.left)) {
            result.removedProductionIds.append(p.id);
            continue;
        }
        text += p.isEpsilon()
                    ? QStringLiteral("%1 -> #\n").arg(p.left)
                    : QStringLiteral("%1 -> %2\n").arg(p.left, p.right.join(QLatin1Char(' ')));
    }

    Grammar simplified;
    QString loadError;
    if (!simplified.loadFromText(text, loadError)) {
        m_error = QStringLiteral("化简结果构造失败：%1").arg(loadError);
        result.kept = grammar;
        return result;
    }
    // 编码表属于语言配置，必须带过去，否则后续解析器无法翻译 .lex 编码
    simplified.setTerminalNames(grammar.terminalNames());
    result.kept = simplified;

    if (result.nonProductiveRemoved.isEmpty() && result.unreachableRemoved.isEmpty()) {
        result.steps << QString();
        result.steps << QStringLiteral("该文法没有无用的非终结符，化简前后完全一致。");
    } else {
        result.steps << QString();
        result.steps << QStringLiteral("删除汇总：不可推导 [%1]、不可达 [%2]，"
                                       "共删除产生式 %3 条。")
                            .arg(result.nonProductiveRemoved.join(QStringLiteral("、")),
                                 result.unreachableRemoved.join(QStringLiteral("、")))
                            .arg(result.removedProductionIds.size());
    }

    return result;
}

}  // namespace ccd
