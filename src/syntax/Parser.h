/******************************************************************************
 * (1) 版权信息 : 编译原理课程设计 - 华南师范大学计算机学院
 * (2) 文件名称 : Parser.h
 *     标识符   : Parser
 *     摘要     : 表驱动的 LALR(1) 语法分析器（第二项任务 2.6）。
 *                按"状态栈 + 符号栈"的经典算法逐符号推进，分为
 *                  · 逐行输出分析过程（栈内容 / 剩余输入 / 当前动作），
 *                    并把"每条语句"作为一次过程分段；
 *                  · 规约时执行 semantics/SemanticsAction 给出的建树动作，
 *                    直接产出语法树（因此不需要额外的"再造一棵树"步骤）；
 *                  · 遇到错误时报出行号、出错符号与**期望符号集合**
 *                    （由分析表当前状态的可用动作反推），并支持跳过恢复
 *                    以便一次报出多个错误。
 *                本文件只依赖 QtCore。
 * (3) 版本号   : 1.0.0       作者/修改者 : <学号 姓名>   完成日期 : 2026-XX-XX
 * (4) 版本历史 : 1.0.0  2026-XX-XX  首次实现（阶段 6）
 *****************************************************************************/

#pragma once

#include <QString>
#include <QStringList>
#include <QVector>

#include "common/Types.h"
#include "syntax/ParseTable.h"
#include "syntax/SemanticsAction.h"

namespace ccd {

/**
 * @brief 分析过程的一步（供界面逐行展示）。
 */
struct ParseStep {
    int index = 0;              ///< 步号（从 1 起）
    QVector<int> stateStack;    ///< 该步**执行前**的状态栈
    QStringList symbolStack;    ///< 该步执行前的符号栈
    int inputPos = 0;           ///< 当前输入指针（待读入符号的下标）
    QString lookahead;          ///< 当前向前看符号
    QString action;             ///< 执行的动作（`移进 s3` / `规约 r2: exp->exp PLUS term` / 接受 / 出错）
    int line = 0;               ///< 向前看符号所在源程序行号
    int statement = 1;          ///< 该步属于第几条顶层语句（用于"按语句分段"）
    bool isError = false;       ///< 该步是否为出错步

    /** @brief 该步的单行文本（界面与导出共用） */
    QString toText() const;
};

/**
 * @brief 一条语法错误记录。
 */
struct SyntaxError {
    int line = 0;              ///< 出错行号
    int inputPos = 0;          ///< 出错处的输入下标
    QString found;             ///< 实际遇到的符号
    QStringList expected;      ///< 该状态下可接受的符号集合
    QString message;           ///< 说明

    /** @brief 单行文本 */
    QString toText() const;
};

/**
 * @brief 一次语法分析的完整结果。
 */
struct ParseResult {
    bool success = false;              ///< 是否全程无错
    SyntaxTreeData tree;               ///< 语法树
    QVector<ParseStep> steps;          ///< 全过程（默认只保留前若干步，见 maxSteps）
    QVector<SyntaxError> errors;       ///< 错误列表
    QStringList statements;            ///< 按顶层语句切出的输入片段（用于分段展示）
    int statementsParsed = 0;          ///< 成功归约出的顶层语句条数
    int acceptedState = -1;            ///< 接受时的状态号

    /**
     * @brief 本次分析使用的完整符号序列（含末尾的 `$`）。
     * @note 过程记录里只存了"当前向前看符号"与输入下标，界面要显示"剩余输入"
     *       就得有原始序列。在这里存一份，比让界面再读一遍 `.lex` 更可靠，
     *       也能保证过程表与语法树确实出自同一次分析。
     */
    QStringList inputSymbols;

    /** @brief 前 maxSteps 步的过程文本 */
    QString stepsText(int maxSteps = 200) const;
    /** @brief 错误明细文本 */
    QString errorsText() const;
    /** @brief 一句话摘要 */
    QString summary() const;
};

/**
 * @brief 表驱动 LALR(1) 语法分析器。
 *
 * 调用注意：
 *  1. 使用流程为 `setTable()` → `setSemantics()` → `parse(symbols, lines, values)`；
 *  2. 分析表里已经含增广产生式 `S' -> S`（`ParseTableBuilder` 会自行增广），
 *     因此传入的文法无需再增广；接受动作就是"在 `$` 上规约增广产生式"或显式
 *     `Accept` 表项，两者都处理；
 *  3. 建树是**在规约时顺带完成**的：从符号栈弹出 n 项的树根，按语义动作组
 *     新建结点再压回。这样分析结束得到的树与"分析过程"天然一致，
 *     不存在两套逻辑对不上的风险；
 *  4. `$` 由调用方（`LexReader::symbols()`）追加，本类不追加。
 */
class Parser
{
public:
    Parser() = default;

    /** @brief 设置分析表 */
    void setTable(const ParseTable &table);

    /** @brief 设置语义动作（决定语法树形状） */
    void setSemantics(const SemanticsActionSet &semantics);

    /**
     * @brief 设置"顶层语句边界符号"，用于把分析过程按语句分段。
     *
     * 默认取 `SEMI`（TINY 与 mini-C 的语句都以分号结尾，而文法里的终结符
     * 用的是**符号名** `SEMI` 而不是字面量 `;`）。每次规约到"右部含该符号的
     * 那条产生式"时计一条语句。
     *
     * 注意：这里必须用文法中的符号名。若写成 `;`，`prod.right.contains()`
     * 永远为假，结果就是"分析成功但一条语句都没识别出来" —— 这是实际踩过的坑。
     */
    void setStatementBoundary(const QString &symbol);

    /** @brief 当前的语句边界符号 */
    QString statementBoundary() const { return m_statementBoundary; }

    /**
     * @brief 执行分析。
     * @param symbols 符号序列（末尾应为 `$`）。
     * @param lines   与 symbols 一一对应的源程序行号；可为空。
     * @param values  与 symbols 一一对应的单词值；可为空。
     * @param maxSteps 最多保留多少步过程记录（0 表示不限）。
     * @return 分析结果。
     */
    ParseResult parse(const QStringList &symbols, const QVector<int> &lines = {},
                      const QStringList &values = {}, int maxSteps = 2000) const;

    /** @brief 最近一次错误信息（配置类错误，与语法错误区分开） */
    QString error() const { return m_error; }

    /** @brief 分析表是否已就绪 */
    bool isReady() const { return m_hasTable; }

    /**
     * @brief 分析表所用文法相对"用户原文法"的产生式编号偏移量。
     * @return 增广产生式插在最前时为 1；未增广时为 0。
     *
     * @note 公开出来是为了让界面/测试能核对"语义动作文件第几行对应哪条产生式"。
     *       语义动作文件按**用户写的、未增广的**文法行序编写，而分析表里的
     *       产生式编号已因增广整体后移，查表时必须减去本偏移。
     */
    int semanticsOffset() const;

private:
    /**
     * @brief 执行语义动作，构造本次规约的结点。
     * @param prod       被规约的产生式。
     * @param rhsNodes   右部各符号对应的树根下标（-1 表示该位置没有树根）。
     * @param tree       输入输出：结点池。
     * @return 规约结果对应的树根下标（-1 表示没有结点，例如纯 ε 产生式且无建结点组）。
     *
     * @note 产生式编号对齐（本阶段踩到的真实缺陷，务必留意）：
     *       `ParseTableBuilder` 内部会对文法调用 `augment()`，把增广产生式
     *       `S' -> S` **插在下标 0**，使原有产生式的编号整体后移一位。
     *       而语义动作文件是按**用户写的、未增广的**文法行序编写的。
     *       因此按 `prod.id` 查语义动作时必须先减去这个偏移
     *       （见 `semanticsOffset()`），否则每条产生式都会取到邻居的语义，
     *       症状是"整棵树错位、只有根结点可达、其余结点全部成为孤岛"。
     */
    int applySemantics(const Production &prod, const QVector<int> &rhsNodes,
                       SyntaxTreeData &tree) const;

    /**
     * @brief 从一个状态出发，列出 ACTION 表上所有可用动作的符号（即"期望符号集"）。
     * @param state 状态号。
     * @param limit 最多列出多少个（0 表示不限）。
     * @return 排序后的符号列表。
     */
    QStringList expectedAt(int state, int limit = 12) const;

    ParseTable m_table;                    ///< 分析表
    SemanticsActionSet m_semantics;        ///< 语义动作
    bool m_hasTable = false;               ///< 是否已设置分析表
    QString m_statementBoundary = QStringLiteral("SEMI");  ///< 语句边界符号（文法里的符号名）
    QString m_error;                       ///< 配置错误
};

}  // namespace ccd
