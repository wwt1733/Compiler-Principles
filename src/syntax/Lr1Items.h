/******************************************************************************
 * (1) 版权信息 : 编译原理课程设计 - 华南师范大学计算机学院
 * (2) 文件名称 : Lr1Items.h
 *     标识符   : Lr1Items
 *     摘要     : LALR(1) 项目集与向前看符号的求解（第二项任务 2.5 的核心）。
 *                采用"先构造 LR(1) 规范族、再把**同核心**的状态合并"的路线：
 *                合并后的状态天然带有 LALR(1) 向前看符号集，
 *                且不需要额外的传播算法即可保证正确。
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

#include "syntax/FirstFollow.h"
#include "syntax/Grammar.h"
#include "syntax/LrItems.h"

namespace ccd {

/**
 * @brief 一个 LALR(1) 状态（同核心的 LR(1) 状态合并结果）。
 *
 * 存储结构：
 *  - core     : LR(0) 内核项目（**排序去重**）—— 合并的判据就是它相同；
 *  - lookahead: 每个内核项目的向前看符号集合，键为项目在 core 中的下标；
 *  - transitions: 文法符号 → 目标 LALR 状态号。
 */
struct LalrState {
    QVector<LrItem> core;                  ///< LR(0) 内核（同核心即合并）
    QVector<QSet<QString>> lookahead;      ///< 与 core 一一对应的向前看集合
    QMap<QString, int> transitions;        ///< 符号 → 目标状态号
    int id = -1;                           ///< 状态号

    /** @brief 某项目的向前看集合（项目不在内核中时返回空集） */
    QSet<QString> lookaheadOf(const LrItem &item) const;

    /** @brief 该状态是否是接受状态（内核含 S' -> S ·） */
    bool isAccepting(const Grammar &grammar) const;
};

/**
 * @brief LALR(1) 自动机。
 */
struct LalrAutomaton {
    Grammar grammar;                 ///< 所用文法（含增广产生式）
    QVector<LalrState> states;       ///< 全部 LALR(1) 状态
    int lr1StateCount = 0;           ///< 合并前的 LR(1) 状态数（用于对比说明）
    int mergedGroupCount = 0;        ///< 实际发生合并的核心组数
    QStringList steps;               ///< 构造过程说明

    /** @brief 一句话摘要 */
    QString summary() const;

    /** @brief 某状态某符号的转移目标（不存在返回 -1） */
    int goTo(int state, const QString &symbol) const;
};

/**
 * @brief LALR(1) 项目集构造器。
 *
 * 调用注意：
 *  1. compute() 要求文法**已增广**；未增广时会自行增广；
 *  2. 时间与空间复杂度关于 LR(1) 规范族的规模。对本项目的 TINY / mini-C
 *     （产生式数十条）完全够用；若将来遇到 LR(1) 状态爆炸的文法，
 *     应改用 DeRemer-Pennello 的向前看传播法（只构造 LR(0) 族）。
 *     本实现的取舍是**正确性与可读性优先**，并在界面上如实报告合并前后的状态数。
 */
class Lr1Items
{
public:
    Lr1Items() = default;

    /**
     * @brief 构造 LALR(1) 自动机。
     * @param grammar  文法（建议已增广）。
     * @param follow   已算好的 First/Follow（用于 LR(1) 闭包的向前看计算）。
     * @return LALR(1) 自动机。
     */
    LalrAutomaton compute(const Grammar &grammar, const FirstFollowResult &follow) const;

    /** @brief 最近一次错误信息 */
    QString error() const { return m_error; }

    /**
     * @brief 求 LR(1) 项目集的闭包。
     * @param grammar 文法。
     * @param items   种子 LR(1) 项目集。
     * @param follow  First/Follow 结果（求 FIRST(βa) 用）。
     * @return 闭包（已按 (产生式, 圆点, 向前看) 排序去重）。
     * @note 公开为静态方法便于自测直接验证这一步。
     *       核心公式：对 A -> α·Bβ, a，对 B 的每条产生式 B -> γ 与
     *       FIRST(βa) 中的每个终结符 b，加入 [B -> ·γ, b]。
     */
    static QVector<Lr1Item> closureLr1(const Grammar &grammar, const QVector<Lr1Item> &items,
                                       const FirstFollowResult &follow);

    /**
     * @brief 求 LR(1) 的 goto(I, X)。
     * @param grammar 文法。
     * @param items   项目集 I。
     * @param symbol  文法符号 X。
     * @param follow  First/Follow 结果。
     * @return goto(I, X) 的闭包；空集时返回空列表。
     */
    static QVector<Lr1Item> goToLr1(const Grammar &grammar, const QVector<Lr1Item> &items,
                                    const QString &symbol, const FirstFollowResult &follow);

    /**
     * @brief 把 LR(1) 项目集投影为它的 LR(0) 内核（判据：产生式 + 圆点）。
     * @param items LR(1) 项目集。
     * @return 排序去重后的内核项目列表。
     * @note 这里刻意取**全部**项目而非"圆点不在最左端的项目"。有些教材把
     *       后者叫内核，但同核心合并的判据必须用同一套口径；用全体项目作判据
     *       同样正确（LR(0) 项目集由其全体项目唯一确定），且实现更简单不易错。
     */
    static QVector<LrItem> coreOf(const QVector<Lr1Item> &items);

private:
    mutable QString m_error;  ///< 最近一次错误信息
};

}  // namespace ccd
