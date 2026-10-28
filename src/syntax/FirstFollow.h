/******************************************************************************
 * (1) 版权信息 : 编译原理课程设计 - 华南师范大学计算机学院
 * (2) 文件名称 : FirstFollow.h
 *     标识符   : FirstFollow
 *     摘要     : First 集与 Follow 集的计算（第二项任务 2.3）。
 *                按讲稿的不动点迭代法求解：
 *                  - First(A)：A 能推出的所有串的首终结符集合；
 *                  - Follow(A)：可能跟在 A 后面的终结符集合（含结束符 $）。
 *                额外求出"可空（nullable）"集合 —— 讲稿的迭代式依赖它，
 *                单独算出来既让实现清晰，也便于在界面上解释计算过程。
 *                本文件只依赖 QtCore，可脱离 GUI 单元测试。
 * (3) 版本号   : 1.0.0       作者/修改者 : <学号 姓名>   完成日期 : 2026-XX-XX
 * (4) 版本历史 : 1.0.0  2026-XX-XX  首次实现（阶段 4）
 *****************************************************************************/

#pragma once

#include <QMap>
#include <QSet>
#include <QString>
#include <QStringList>

#include "syntax/Grammar.h"

namespace ccd {

/** 输入结束标记（Follow 集里代表"后面没有符号了"） */
extern const QString kEndMarker;

/**
 * @brief First / Follow 计算结果。
 *
 * 存储结构说明（与讲稿的书写习惯对齐）：
 *  1. **First 集不含 ε**。若某非终结符可空，只在 nullable 里体现；
 *     这样界面上的 First 表就是"首终结符"，与手算结果逐项可比。
 *     需要判断"串是否可空"时用 nullable 与 firstSetOfSequence() 的返回值组合，
 *     不要指望在 First 集里找 ε；
 *  2. Follow 集**含结束符 `$`**（kEndMarker），因为分析表的 Accept 动作
 *     与"输入读完"的判定都依赖它。
 */
struct FirstFollowResult {
    Grammar grammar;                  ///< 计算所用的文法（通常是增广并化简后的）
    QMap<QString, QSet<QString>> first;   ///< 非终结符 → First 集
    QMap<QString, QSet<QString>> follow;  ///< 非终结符 → Follow 集
    QSet<QString> nullable;           ///< 可推出空串的非终结符

    QStringList steps;                ///< 逐步说明（界面"计算过程"区显示）

    /**
     * @brief 求一个符号序列的 First 集。
     * @param sequence 符号序列。
     * @return 该序列的 First 集（不含 ε）。
     * @note 语义与推导式一致：从左到右累加每个符号的 First，
     *       遇到"不可空"的符号即停止；全部符号都可空时返回空集，
     *       此时调用方应据 sequenceNullable() 判断这是不是"可空"。
     */
    QSet<QString> firstSetOfSequence(const QStringList &sequence) const;

    /**
     * @brief 判断一个符号序列是否可推出空串。
     * @param sequence 符号序列。
     * @return 全部符号都可空（空序列视为可空）时为 true。
     */
    bool sequenceNullable(const QStringList &sequence) const;

    /** @brief 把集合格式化为排序后的顿号分隔文本（供界面与文档显示） */
    static QString formatSet(const QSet<QString> &set);

    /** @brief 一句话摘要 */
    QString summary() const;
};

/**
 * @brief First / Follow 计算器。
 *
 * 调用注意：
 *  1. 建议在**增广文法**上计算：增广产生式 `S' -> S` 使 `$` 自然进入
 *     Follow(S)，无需在代码里为开始符号手工插入 `$`（虽然本实现也做了兜底）；
 *  2. compute() 不修改传入的文法；
 *  3. 若文法未增广，Follow(开始符号) 会额外补上 `$`。
 */
class FirstFollow
{
public:
    FirstFollow() = default;

    /**
     * @brief 计算 First 集、Follow 集与可空集合。
     * @param grammar 文法（建议已增广）。
     * @return 计算结果。
     */
    FirstFollowResult compute(const Grammar &grammar) const;

    /** @brief 最近一次错误信息 */
    QString error() const { return m_error; }

private:
    /**
     * @brief 求可推出空串的非终结符集合（不动点迭代）。
     * @param grammar 文法。
     * @param steps   输出：每一轮新增了哪些非终结符。
     * @return 可空非终结符集合。
     */
    QSet<QString> computeNullable(const Grammar &grammar, QStringList &steps) const;

    /**
     * @brief 求全部非终结符的 First 集（不动点迭代）。
     * @param grammar  文法。
     * @param nullable 可空集合。
     * @param steps    输出：每一轮新增了哪些元素。
     * @return 非终结符 → First 集。
     */
    QMap<QString, QSet<QString>> computeFirst(const Grammar &grammar, const QSet<QString> &nullable,
                                              QStringList &steps) const;

    /**
     * @brief 求全部非终结符的 Follow 集（不动点迭代）。
     * @param grammar  文法。
     * @param nullable 可空集合。
     * @param first    已求出的 First 集。
     * @param steps    输出：每一轮新增了哪些元素。
     * @return 非终结符 → Follow 集。
     */
    QMap<QString, QSet<QString>> computeFollow(const Grammar &grammar, const QSet<QString> &nullable,
                                               const QMap<QString, QSet<QString>> &first,
                                               QStringList &steps) const;

    mutable QString m_error;  ///< 最近一次错误信息
};

}  // namespace ccd
