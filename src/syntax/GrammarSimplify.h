/******************************************************************************
 * (1) 版权信息 : 编译原理课程设计 - 华南师范大学计算机学院
 * (2) 文件名称 : GrammarSimplify.h
 *     标识符   : GrammarSimplify
 *     摘要     : 文法化简（第二项任务 2.2）。
 *                按讲稿的两步法删除无用产生式：
 *                  第一步 —— 删除"不可推导"（无法推出终结符串）的非终结符及其产生式；
 *                  第二步 —— 删除"不可达"（从开始符号出发走不到）的非终结符及其产生式。
 *                两步都必须执行，且**顺序不能颠倒**：先删不可推导的，才能让
 *                第二步发现那些"只经由死符号才可达"的符号。
 *                本文件只依赖 QtCore，可脱离 GUI 单元测试。
 * (3) 版本号   : 1.0.0       作者/修改者 : <学号 姓名>   完成日期 : 2026-XX-XX
 * (4) 版本历史 : 1.0.0  2026-XX-XX  首次实现（阶段 4）
 *****************************************************************************/

#pragma once

#include <QSet>
#include <QString>
#include <QStringList>

#include "syntax/Grammar.h"

namespace ccd {

/**
 * @brief 文法化简结果。
 *
 * 存储结构：kept 是化简后的文法（**不再增广**，保持与输入同形），
 *          便于与原文法逐条对照；三张名单用于在界面上交代"删了什么、为什么"。
 */
struct SimplifyResult {
    Grammar original;              ///< 化简前的文法（含终结符/非终结符列表）
    Grammar kept;                  ///< 化简后的文法

    QStringList unreachableRemoved;   ///< 被判定为"不可达"而非终结符
    QStringList nonProductiveRemoved; ///< 被判定为"不可推导"的非终结符
    QVector<int> removedProductionIds;///< 被删除的产生式在原文法中的序号

    QStringList steps;             ///< 逐步说明（供界面"化简过程"区显示）

    /** @brief 是否删除了任何东西 */
    bool changed() const
    {
        return !unreachableRemoved.isEmpty() || !nonProductiveRemoved.isEmpty();
    }

    /** @brief 一句话摘要 */
    QString summary() const;
};

/**
 * @brief 文法化简器。
 *
 * 调用注意：
 *  1. simplify() 不修改传入的文法，结果全部放在返回值里；
 *  2. 开始符号恒被保留（即使它推不出终结符串）—— 否则化简后的文法为空、
 *     后续所有分析都无从谈起；这种情况下会在 steps 里明确给出警告；
 *  3. 化简后的文法**不做增广**，增广由调用方在化简之后按需执行
 *     （顺序：载入 → 化简 → 增广 → First/Follow → LR 构造）。
 */
class GrammarSimplify
{
public:
    GrammarSimplify() = default;

    /**
     * @brief 执行化简。
     * @param grammar 待化简的文法（不被修改）。
     * @return 化简结果（含化简后的文法与删除明细）。
     */
    SimplifyResult simplify(const Grammar &grammar) const;

    /** @brief 最近一次错误信息 */
    QString error() const { return m_error; }

private:
    /**
     * @brief 求"可推导出终结符串"的非终结符集合（含逐步推导过程）。
     * @param grammar 文法。
     * @param steps   输出：每一轮新增了哪些非终结符。
     * @return 可推导的非终结符集合。
     */
    QSet<QString> productiveNonterminals(const Grammar &grammar, QStringList &steps) const;

    /**
     * @brief 求"从开始符号可达"的非终结符集合。
     * @param grammar 文法。
     * @param productive 已求出的可推导集合（用于第二轮：只沿可推导符号前进）。
     * @param steps   输出：每一轮新增了哪些非终结符。
     * @return 可达的非终结符集合。
     */
    QSet<QString> reachableNonterminals(const Grammar &grammar, const QSet<QString> &productive,
                                        QStringList &steps) const;

    mutable QString m_error;  ///< 最近一次错误信息
};

}  // namespace ccd
