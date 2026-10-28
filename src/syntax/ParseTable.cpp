/******************************************************************************
 * (1) 版权信息 : 编译原理课程设计 - 华南师范大学计算机学院
 * (2) 文件名称 : ParseTable.cpp
 *     标识符   : ParseTable
 *     摘要     : ACTION / GOTO 分析表构造与冲突检测的实现（第二项任务 2.5）。
 * (3) 版本号   : 1.0.0       作者/修改者 : <学号 姓名>   完成日期 : 2026-XX-XX
 * (4) 版本历史 : 1.0.0  2026-XX-XX  首次实现（阶段 5）
 *****************************************************************************/

#include "syntax/ParseTable.h"

#include <QSet>

#include "syntax/GrammarSimplify.h"
#include "syntax/LrItems.h"

namespace ccd {

/** 功能：表种类转中文名。输入：kind。输出：名称。 */
QString parseTableKindName(ParseTableKind kind)
{
    switch (kind) {
    case ParseTableKind::Lalr1:
        return QStringLiteral("LALR(1)");
    case ParseTableKind::Slr1:
        return QStringLiteral("SLR(1)");
    case ParseTableKind::Lr0:
        return QStringLiteral("LR(0)");
    }
    return QStringLiteral("未知");
}

// ---------------------------------------------------------------------------
// ParseTable
// ---------------------------------------------------------------------------

/** 功能：取动作。输入：state、terminal。输出：动作（无表项为 Error）。 */
ParseAction ParseTable::actionAt(int state, const QString &terminal) const
{
    if (state < 0 || state >= action.size())
        return ParseAction{};
    return action.at(state).value(terminal, ParseAction{});
}

/** 功能：取 goto 目标。输入：state、nonterminal。输出：状态号（无表项为 -1）。 */
int ParseTable::gotoAt(int state, const QString &nonterminal) const
{
    if (state < 0 || state >= gotoTable.size())
        return -1;
    return gotoTable.at(state).value(nonterminal, -1);
}

/** 功能：一句话摘要。输入：无。输出：摘要文本。 */
QString ParseTable::summary() const
{
    int actionEntries = 0;
    for (const QMap<QString, ParseAction> &row : action)
        actionEntries += row.size();
    int gotoEntries = 0;
    for (const QMap<QString, int> &row : gotoTable)
        gotoEntries += row.size();

    return QStringLiteral("%1 分析表：%2 个状态；ACTION 表项 %3 个、GOTO 表项 %4 个；%5")
        .arg(parseTableKindName(kind))
        .arg(action.size())
        .arg(actionEntries)
        .arg(gotoEntries)
        .arg(conflicts.isEmpty() ? QStringLiteral("无冲突")
                                 : QStringLiteral("冲突 %1 处").arg(conflicts.size()));
}

/**
 * 功能：冲突明细文本。
 * 输入：无。
 * 输出：冲突列表文本；无冲突时给出说明与已被消解的情况。
 * 注意：即使当前无冲突，也把"冲突消解规则"写出来 —— 报告里需要交代
 *       用的是哪种策略，否则"为什么悬空 else 归到最近的 if"无从解释。
 */
QString ParseTable::conflictsText() const
{
    if (conflicts.isEmpty()) {
        return QStringLiteral("该文法在 %1 下无移进-规约冲突、无规约-规约冲突，"
                              "分析表是确定的。")
            .arg(parseTableKindName(kind));
    }

    QStringList lines;
    lines << QStringLiteral("共 %1 处冲突（已按下述规则消解，分析表仍可运行，"
                            "但文法严格来说不是 %2 文法）：")
                 .arg(conflicts.size())
                 .arg(parseTableKindName(kind));
    lines << QString();
    for (int i = 0; i < conflicts.size(); ++i) {
        const TableConflict &c = conflicts.at(i);
        lines << QStringLiteral("%1. 状态 I%2 遇符号 %3：%4 → %5")
                     .arg(i + 1)
                     .arg(c.state)
                     .arg(c.symbol)
                     .arg(c.oldAction, c.newAction);
        if (!c.description.isEmpty())
            lines << QStringLiteral("   说明：%1").arg(c.description);
    }
    return lines.join(QLatin1Char('\n'));
}

/**
 * 功能：分析表文本形式。
 * 输入：无。
 * 输出：以状态为行的表格文本（列 = 全部终结符 + GOTO 的非终结符）。
 * 注意：列头用**终结符原文**（如 LT、SEMI），不附加编码 ——
 *       编码在符号对照表里已单独列出，这里再带一遍会让表格过宽而难读。
 */
QString ParseTable::toText() const
{
    const QStringList &terms = terminals();
    const QStringList &nonts = nonterminals();

    QStringList header;
    header << QStringLiteral("状态");
    for (const QString &t : terms)
        header << t;
    for (const QString &n : nonts)
        header << n;

    QStringList lines;
    lines << header.join(QLatin1Char('\t'));

    for (int s = 0; s < action.size(); ++s) {
        QStringList row;
        row << QStringLiteral("I%1").arg(s);
        for (const QString &t : terms) {
            const ParseAction a = actionAt(s, t);
            row << (a.type == ActionType::Error ? QStringLiteral("") : a.toString());
        }
        for (const QString &n : nonts) {
            const int target = gotoAt(s, n);
            row << (target < 0 ? QStringLiteral("") : QString::number(target));
        }
        lines << row.join(QLatin1Char('\t'));
    }

    return lines.join(QLatin1Char('\n'));
}

// ---------------------------------------------------------------------------
// ParseTableBuilder
// ---------------------------------------------------------------------------

/**
 * 功能：构造分析表。
 * 输入：grammar —— 文法；kind —— 表种类。
 * 输出：分析表。
 * 注意（关键实现要点）：
 *  1. **先化简再增广**（顺序不可颠倒）：化简会删除"从开始符号不可达"的符号，
 *     而增广后的 S' 恰好是这样一个符号，先增广会被误删；
 *  2. 三种表共用同一套填表骨架，差别只在"规约动作用的向前看符号从哪来"：
 *       LALR(1) —— 用 LALR 项目集里各项目的 lookahead；
 *       SLR(1)  —— 用 Follow(左部)；
 *       LR(0)   —— 用**全部**终结符（即不看向前看符号，冲突最多）；
 *     这样对比实验只需切换一个枚举，不必维护三份填表代码；
 *  3. 冲突一律记录并按下述规则消解，**不中断构造**：
 *       移进-规约 → 移进优先（与 yacc/bison 默认一致，可自动消解悬空 else）；
 *       规约-规约 → 产生式序号小者优先（保证结果确定、可复现）。
 *     每次"表项被覆盖"都记一条冲突，因此冲突数就是被覆盖的次数，
 *     这比"只记第一次冲突"更能反映文法的真实问题密度。
 */
ParseTable ParseTableBuilder::build(const Grammar &grammar, ParseTableKind kind) const
{
    m_error.clear();
    ParseTable table;
    table.kind = kind;

    if (grammar.productionCount() == 0) {
        m_error = QStringLiteral("文法为空，无法构造分析表。");
        return table;
    }

    // ---- 化简（在增广之前）----
    GrammarSimplify simplifier;
    const SimplifyResult simplified = simplifier.simplify(grammar);
    Grammar work = simplified.kept;
    work.setTerminalNames(grammar.terminalNames());

    table.steps << QStringLiteral("文法化简：%1").arg(simplified.summary());

    // ---- 增广 ----
    work.augment();
    table.steps << QStringLiteral("增广文法：追加 %1 -> %2")
                       .arg(work.augmentedStart(), work.startSymbol());

    // ---- First/Follow ----
    FirstFollow ff;
    const FirstFollowResult follow = ff.compute(work);
    table.steps << QStringLiteral("First/Follow：%1").arg(follow.summary());

    // ---- 项目集 ----
    Lr1Items lr1;
    LalrAutomaton automaton;
    if (kind == ParseTableKind::Lalr1) {
        automaton = lr1.compute(work, follow);
        if (!lr1.error().isEmpty()) {
            m_error = lr1.error();
            return table;
        }
        table.steps << automaton.summary();
    } else {
        // SLR(1) 与 LR(0) 都不需要向前看符号，直接用 LR(0) 规范族，
        // 少构造一遍 LR(1) 族，也让界面上"合并前后状态数"的对比更有说服力。
        LrItems lr0;
        const Lr0Automaton lr0Auto = lr0.build(work);
        if (!lr0.error().isEmpty()) {
            m_error = lr0.error();
            return table;
        }
        // 把 LR(0) 自动机包装成 LalrAutomaton 以便共用填表代码：
        // 内核 = 全部项目，向前看集合在填表时按 kind 另行给出（故此处留空）。
        automaton.grammar = lr0Auto.grammar;
        automaton.lr1StateCount = lr0Auto.states.size();
        automaton.mergedGroupCount = 0;
        automaton.steps = lr0Auto.steps;
        automaton.states.resize(lr0Auto.states.size());
        for (int i = 0; i < lr0Auto.states.size(); ++i) {
            LalrState &st = automaton.states[i];
            st.id = i;
            st.core = lr0Auto.states.at(i).items;
            st.lookahead.resize(st.core.size());
            st.transitions = lr0Auto.states.at(i).transitions;
        }
        table.steps << lr0Auto.summary();
    }

    table.grammar = automaton.grammar;
    table.automaton = automaton;

    const Grammar &g = table.grammar;
    const int stateCount = automaton.states.size();

    // SLR(1) 与 LR(0) 的规约向前看符号在此处预先确定，填表时直接取用
    const QSet<QString> allTerminals(g.terminals().constBegin(), g.terminals().constEnd());

    table.action.resize(stateCount);
    table.gotoTable.resize(stateCount);

    // ---- 填表 ----
    for (int s = 0; s < stateCount; ++s) {
        const LalrState &st = automaton.states.at(s);

        // ① GOTO 与"圆点后是终结符"的移进：都由转移边直接给出
        for (auto it = st.transitions.constBegin(); it != st.transitions.constEnd(); ++it) {
            const QString &sym = it.key();
            const int target = it.value();
            if (g.isTerminal(sym)) {
                ParseAction a;
                a.type = ActionType::Shift;
                a.value = target;
                const auto old = table.action[s].constFind(sym);
                if (old != table.action[s].constEnd() && old.value().type != ActionType::Shift) {
                    TableConflict c;
                    c.state = s;
                    c.symbol = sym;
                    c.oldAction = old.value().toString();
                    c.newAction = a.toString();
                    c.description = QStringLiteral("移进-规约冲突：%1 状态下遇 %2 既已决定规约，"
                                                   "又可按转移边移进。按移进优先消解。")
                                        .arg(QStringLiteral("I%1").arg(s), sym);
                    table.conflicts.append(c);
                }
                table.action[s].insert(sym, a);
            } else if (g.isNonterminal(sym)) {
                table.gotoTable[s].insert(sym, target);
            }
            // 增广开始符号 S' 既不是终结符也不是非终结符（它只出现在左部），
            // 这里不必处理：它的项目只用于判定接受。
        }

        // ② 规约与接受：遍历项目
        for (int ci = 0; ci < st.core.size(); ++ci) {
            const LrItem &item = st.core.at(ci);
            const Production &p = g.production(item.prodId);
            if (item.dot < p.right.size())
                continue;  // 圆点不在末尾，无规约动作

            // 接受：S' -> S ·
            if (p.left == g.augmentedStart()) {
                ParseAction a;
                a.type = ActionType::Accept;
                table.action[s].insert(kEndMarker, a);
                continue;
            }

            // 确定规约的向前看符号集合
            QSet<QString> lookaheads;
            switch (kind) {
            case ParseTableKind::Lalr1:
                lookaheads = st.lookahead.value(ci);
                break;
            case ParseTableKind::Slr1:
                lookaheads = follow.follow.value(p.left);
                break;
            case ParseTableKind::Lr0:
                lookaheads = allTerminals;
                break;
            }

            ParseAction a;
            a.type = ActionType::Reduce;
            a.value = p.id;

            for (const QString &t : lookaheads) {
                const auto old = table.action[s].constFind(t);
                if (old != table.action[s].constEnd()) {
                    // 冲突：已有动作与本次规约争夺同一格子
                    TableConflict c;
                    c.state = s;
                    c.symbol = t;
                    c.oldAction = old.value().toString();
                    c.newAction = a.toString();

                    if (old.value().type == ActionType::Shift) {
                        c.description = m_policy.shiftWinsOverReduce
                                            ? QStringLiteral("移进-规约冲突：候选规约是 %1"
                                                             "（产生式 %2）。按移进优先保留移进。")
                                                  .arg(p.toString())
                                                  .arg(p.id)
                                            : QStringLiteral("移进-规约冲突：候选规约是 %1"
                                                             "（产生式 %2）。按规约优先改为规约。")
                                                  .arg(p.toString())
                                                  .arg(p.id);
                        if (m_policy.shiftWinsOverReduce)
                            { table.conflicts.append(c); continue; }
                    } else if (old.value().type == ActionType::Reduce) {
                        const int oldProd = old.value().value;
                        c.description =
                            QStringLiteral("规约-规约冲突：已选产生式 %1，本次候选产生式 %2。%3")
                                .arg(oldProd)
                                .arg(p.id)
                                .arg(m_policy.lowerProductionWins
                                         ? QStringLiteral("按序号小的优先消解。")
                                         : QStringLiteral("按后出现的优先消解。"));
                        if (m_policy.lowerProductionWins && oldProd <= p.id)
                            { table.conflicts.append(c); continue; }
                    } else if (old.value().type == ActionType::Accept) {
                        c.description = QStringLiteral("接受-规约冲突：该状态已可接受输入，"
                                                       "本次候选规约为 %1。保留接受。")
                                            .arg(p.toString());
                        table.conflicts.append(c);
                        continue;
                    }
                    table.conflicts.append(c);
                }
                table.action[s].insert(t, a);
            }
        }
    }

    table.steps << QString();
    table.steps << table.summary();
    if (!table.conflicts.isEmpty()) {
        table.steps << QStringLiteral("检测到 %1 处冲突，已按默认规则消解"
                                      "（移进优先 / 序号小的产生式优先）。")
                           .arg(table.conflicts.size());
    }

    return table;
}

}  // namespace ccd
