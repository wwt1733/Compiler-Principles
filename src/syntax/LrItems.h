/******************************************************************************
 * (1) 版权信息 : 编译原理课程设计 - 华南师范大学计算机学院
 * (2) 文件名称 : LrItems.h
 *     标识符   : LrItems
 *     摘要     : LR(0) 项目集规范族的构造（第二项任务 2.4）。
 *                实现 closure(I)、goto(I, X) 与项目集去重，
 *                得到 LR(0) 自动机（DFA）的全部状态与转移。
 *                本文件只依赖 QtCore，可脱离 GUI 单元测试。
 * (3) 版本号   : 1.0.0       作者/修改者 : <学号 姓名>   完成日期 : 2026-XX-XX
 * (4) 版本历史 : 1.0.0  2026-XX-XX  首次实现（阶段 5）
 *****************************************************************************/

#pragma once

#include <QMap>
#include <QSet>
#include <QString>
#include <QStringList>
#include <QVector>

#include "syntax/Grammar.h"

namespace ccd {

/**
 * @brief 一个 LR(0) 项目集的索引化表示。
 *
 * 存储结构：
 *  - items    : **排序去重后**的项目列表。排序是为了让"同一项目集"的表示唯一，
 *               从而可以用它作为去重键（否则 {A,B} 与 {B,A} 会被当成两个状态）；
 *  - gotoTable: 文法符号 → 目标状态号。
 *  - id       : 状态号（0 为初态，即 closure({S' -> ·S})）。
 */
struct Lr0State {
    QVector<LrItem> items;         ///< 项目集（排序去重）
    QMap<QString, int> transitions;///< 该状态经各符号转移到的状态号
    int id = -1;                   ///< 状态号

    /** @brief 该状态是否为"内核"（是否含圆点在产生式最左端的增广项目） */
    bool containsAugmentedStart(const Grammar &grammar) const;

    /** @brief 项目集的规格化文本（一行一个项目） */
    QString toText(const Grammar &grammar) const;

    /** @brief 该状态是否是"接受状态"（含 S' -> S ·） */
    bool isAccepting(const Grammar &grammar) const;
};

/**
 * @brief LR(0) 项目集规范族（LR(0) 自动机）。
 *
 * 调用注意：
 *  1. build() 要求文法**已增广**（`S' -> S` 为最后一条产生式）；未增广时
 *     本类会自行调用 augment()，但那样 `S'` 的编号会由本类决定，
 *     调用方若已持有产生式序号可能对不上，因此推荐显式先增广；
 *  2. 状态顺序为"发现顺序"（BFS），因此状态 0 恒为初态。
 *     顺序稳定是可回归测试的前提 —— 界面上分析表的行序依赖它。
 */
struct Lr0Automaton {
    Grammar grammar;              ///< 所用文法（含增广产生式）
    QVector<Lr0State> states;     ///< 全部状态
    QStringList steps;            ///< 构造过程说明（界面"构造过程"区显示）

    /** @brief 一句话摘要 */
    QString summary() const;

    /** @brief 某状态某符号的转移目标（不存在返回 -1） */
    int goTo(int state, const QString &symbol) const;

    /** @brief 全部文法符号（终结符在前、非终结符在后，供界面列头使用） */
    QStringList allSymbols() const;

    /** @brief 状态的项目集文本（供界面表格显示） */
    QString stateItemsText(int state) const;
};

/**
 * @brief LR(0) 项目集规范族的构造器。
 *
 * 调用注意：build() 不修改传入的文法（内部会复制一份再增广）。
 */
class LrItems
{
public:
    LrItems() = default;

    /**
     * @brief 构造 LR(0) 项目集规范族。
     * @param grammar 文法（建议已增广）。
     * @return LR(0) 自动机。
     */
    Lr0Automaton build(const Grammar &grammar) const;

    /** @brief 最近一次错误信息 */
    QString error() const { return m_error; }

    /**
     * @brief 求项目集的闭包 closure(I)。
     * @param grammar 文法。
     * @param items   种子项目集（顺序任意）。
     * @return 闭包（**已排序去重**）。
     * @note 公开为静态方法是为了让自测可以直接验证"闭包"这一步 ——
     *       这是 LR 构造里最容易写错的地方（漏掉新增项目的再展开）。
     */
    static QVector<LrItem> closure(const Grammar &grammar, const QVector<LrItem> &items);

    /**
     * @brief 求 goto(I, X)。
     * @param grammar 文法。
     * @param items   项目集 I（不必是闭包，函数内部会先取闭包）。
     * @param symbol  文法符号 X。
     * @return goto(I, X) 的**闭包**；若为空集则返回空列表。
     */
    static QVector<LrItem> goTo(const Grammar &grammar, const QVector<LrItem> &items,
                                const QString &symbol);

private:
    mutable QString m_error;  ///< 最近一次错误信息
};

}  // namespace ccd
