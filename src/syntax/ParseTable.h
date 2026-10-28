/******************************************************************************
 * (1) 版权信息 : 编译原理课程设计 - 华南师范大学计算机学院
 * (2) 文件名称 : ParseTable.h
 *     标识符   : ParseTable
 *     摘要     : ACTION / GOTO 分析表的构造与冲突检测（第二项任务 2.5）。
 *                基于 LALR(1) 自动机填表：移进填 s<n>、规约填 r<n>、
 *                接受填 acc；同时检测移进-规约与规约-规约冲突并如实记录，
 *                供界面报告"该文法不是 LALR(1)"的具体原因。
 *                本文件只依赖 QtCore，可脱离 GUI 单元测试。
 * (3) 版本号   : 1.0.0       作者/修改者 : <学号 姓名>   完成日期 : 2026-XX-XX
 * (4) 版本历史 : 1.0.0  2026-XX-XX  首次实现（阶段 5）
 *****************************************************************************/

#pragma once

#include <QMap>
#include <QString>
#include <QStringList>
#include <QVector>

#include "common/Types.h"
#include "syntax/FirstFollow.h"
#include "syntax/Grammar.h"
#include "syntax/Lr1Items.h"

namespace ccd {

/** 分析表种类（界面可切换对比，实验报告里做对比实验用） */
enum class ParseTableKind {
    Lalr1,  ///< LALR(1)：LR(1) 规范族合并同核心状态（本项目主线）
    Slr1,   ///< SLR(1)：用 Follow 集填规约动作（用于对比，规模小但能力弱）
    Lr0     ///< LR(0)：不看向前看符号，冲突最多（用于对比说明 LALR 的必要性）
};

/** @brief 表种类转中文名 */
QString parseTableKindName(ParseTableKind kind);

/**
 * @brief 完整的分析表。
 *
 * 存储结构：
 *  - action[state][terminal] : 移进/规约/接受动作；
 *  - gotoTable[state][nonterminal] : 转移目标状态号。
 *  两者都用 QMap 而非二维数组，因为终结符集合在填表时才知道，
 *  且大部分格子是空的（用 Map 天然只存非空项，界面遍历也方便）。
 */
struct ParseTable {
    Grammar grammar;                          ///< 所用文法（含增广产生式）
    LalrAutomaton automaton;                  ///< 对应的 LALR(1) 自动机
    ParseTableKind kind = ParseTableKind::Lalr1;  ///< 表种类

    QVector<QMap<QString, ParseAction>> action;   ///< ACTION 表
    QVector<QMap<QString, int>> gotoTable;        ///< GOTO 表

    QVector<TableConflict> conflicts;         ///< 冲突记录（空表示是合法表）
    QStringList steps;                        ///< 填表过程说明

    /** @brief 是否为无冲突的合法分析表 */
    bool isClean() const { return conflicts.isEmpty(); }

    /** @brief 取动作（无表项返回 Error 动作） */
    ParseAction actionAt(int state, const QString &terminal) const;

    /** @brief 取 goto 目标（无表项返回 -1） */
    int gotoAt(int state, const QString &nonterminal) const;

    /** @brief 终结符列表（ACTION 表的列头，按文法顺序） */
    const QStringList &terminals() const { return grammar.terminals(); }
    /** @brief 非终结符列表（GOTO 表的列头） */
    const QStringList &nonterminals() const { return grammar.nonterminals(); }

    /** @brief 状态数 */
    int stateCount() const { return action.size(); }

    /** @brief 一句话摘要（含状态数、表项数、冲突数） */
    QString summary() const;

    /** @brief 冲突明细文本（供界面"冲突报告"区显示；无冲突时给出说明） */
    QString conflictsText() const;

    /** @brief 分析表文本形式（供导出与报告引用，行=状态，列=终结符） */
    QString toText() const;
};

/**
 * @brief 分析表构造器。
 *
 * 调用注意：
 *  1. build() 内部会依次完成：增广 → 化简后重算 First/Follow →
 *     构造 LALR(1) 项目集 → 填表。调用方只需给文法与表种类；
 *  2. **冲突不视为失败**：本类总是返回填好的表（冲突格子按"移进优先"
 *     或"先出现的规约优先"取舍），并把每次覆盖记进 conflicts。
 *     这样界面既能展示"文法不是 LALR(1)"的具体位置，也能让用户看到
 *     冲突是怎么被解决的 —— 这比直接报错更有教学价值。
 */
class ParseTableBuilder
{
public:
    ParseTableBuilder() = default;

    /**
     * @brief 构造分析表。
     * @param grammar 文法（可未增广；本函数会自行增广）。
     * @param kind    表种类。
     * @return 分析表。
     */
    ParseTable build(const Grammar &grammar, ParseTableKind kind = ParseTableKind::Lalr1) const;

    /** @brief 最近一次错误信息 */
    QString error() const { return m_error; }

    /**
     * @brief 冲突消解规则（可配置，便于在报告里说明取舍依据）。
     *
     * 默认策略：**移进优先**（shift-reduce）与**先出现的产生式优先**
     * （reduce-reduce）。理由：
     *  - 移进优先是 yacc/bison 的默认，能自动消解悬空 else 这类经典冲突，
     *    且不需要给文法加优先级声明；
     *  - 规约-规约冲突按产生式序号取小者，保证结果**确定且可复现**
     *    （否则同一文法两次运行可能给出不同表，无法回归测试）。
     */
    struct ConflictPolicy {
        bool shiftWinsOverReduce = true;   ///< 移进-规约冲突时移进优先
        bool lowerProductionWins = true;   ///< 规约-规约冲突时序号小的优先
    };

    /** @brief 设置冲突消解规则（影响后续 build()） */
    void setConflictPolicy(const ConflictPolicy &policy) { m_policy = policy; }
    /** @brief 取当前冲突消解规则 */
    const ConflictPolicy &conflictPolicy() const { return m_policy; }

private:
    mutable QString m_error;       ///< 最近一次错误信息
    ConflictPolicy m_policy;       ///< 冲突消解规则
};

}  // namespace ccd
