/******************************************************************************
 * (1) 版权信息 : 编译原理课程设计 - 华南师范大学计算机学院
 * (2) 文件名称 : DfaMinimize.h
 *     标识符   : DfaMinimize
 *     摘要     : DFA 最小化（划分法 / Moore 算法）。算法步骤：
 *                  （1）初始划分 Π0：把状态分成"非接受态"与"接受态"两组，
 *                       其中接受态再按**输出编码**细分（不同单词的接受态
 *                       绝不能合并，否则词法分析会输出错误编码）；
 *                  （2）反复扫描每个组：若组内两个状态在某个输入符号上的
 *                       转移落入了不同的组，则把该组拆开；
 *                  （3）重复至划分不再变化，每个组即为一个最小化后的状态；
 *                  （4）按"原状态编号最小者"作为代表重建状态转换表，
 *                       并按从初态出发的广度优先顺序重新编号。
 *                本文件只依赖 QtCore。
 * (3) 版本号   : 1.0.0       作者/修改者 : <学号 姓名>   完成日期 : 2026-XX-XX
 * (4) 版本历史 : 1.0.0  2026-XX-XX  首次实现
 *****************************************************************************/

#pragma once

#include <QString>
#include <QVector>

#include "common/Types.h"

namespace ccd {

/**
 * @brief DFA 最小化器。
 *
 * 调用注意：
 *  1. 输入 DFA 应已由 NfaToDfa 构造完成（含 alphabet / trans / acceptCode）；
 *  2. 输出 DFA 的状态编号为"从初态出发的广度优先顺序"，初态固定为 0；
 *  3. 会同时输出"最小化后状态 → 原状态集合"的对照，供界面展示合并情况。
 */
class DfaMinimize
{
public:
    DfaMinimize() = default;

    /**
     * @brief 执行最小化。
     * @param dfa 输入 DFA。
     * @param out 输出最小化 DFA。
     * @return 成功为 true；失败时 error() 给出原因。
     */
    bool minimize(const Dfa &dfa, Dfa &out);

    /** @brief 最近一次错误信息 */
    QString error() const { return m_error; }

    /**
     * @brief 取最近一次最小化的"新状态 → 原状态集合"对照。
     * @return 对照表；下标为最小化后的状态编号。
     */
    const QVector<QVector<int>> &mergedGroups() const { return m_mergedGroups; }

    /**
     * @brief 取最近一次最小化的划分过程记录（每轮一个快照）。
     * @return 每轮划分的文本描述，供界面展示"划分法如何逐步细化"。
     */
    const QStringList &partitionTrace() const { return m_trace; }

private:
    QString m_error;                        ///< 错误信息
    QVector<QVector<int>> m_mergedGroups;   ///< 新状态 → 原状态集合
    QStringList m_trace;                    ///< 划分过程记录
};

}  // namespace ccd
