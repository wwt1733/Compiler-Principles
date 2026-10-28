/******************************************************************************
 * (1) 版权信息 : 编译原理课程设计 - 华南师范大学计算机学院
 * (2) 文件名称 : NfaToDfa.h
 *     标识符   : NfaToDfa
 *     摘要     : 不确定的有穷自动机 → 确定的有穷自动机（子集构造法）。
 *                算法步骤：
 *                  （1）D0 = ε-closure({NFA 开始状态})；
 *                  （2）对每个未处理的 DFA 状态 Di 与每个输入符号 a，
 *                       求 Dj = ε-closure(move(Di, a))；
 *                  （3）若 Dj 是新集合则分配新编号并入队，否则复用已有编号；
 *                  （4）重复至无新状态产生。
 *                同时登记"DFA 状态 → 对应 NFA 状态集合"的对照关系，供界面显示。
 *                本文件只依赖 QtCore。
 * (3) 版本号   : 1.0.0       作者/修改者 : <学号 姓名>   完成日期 : 2026-XX-XX
 * (4) 版本历史 : 1.0.0  2026-XX-XX  首次实现
 *****************************************************************************/

#pragma once

#include <QString>
#include <functional>

#include "common/Types.h"

namespace ccd {

/**
 * @brief 子集构造法：NFA → DFA。
 *
 * 调用注意：
 *  1. 输入 NFA 必须已填充 stateCount / edges / start / accepts / alphabet；
 *  2. 输出 DFA 的状态编号按"发现顺序"（即广度优先顺序）分配，编号 0 为初态，
 *     这样状态转换表读起来与教材一致；
 *  3. 接受状态的编码归属遵循"先声明者优先、非注释优先于注释"的规则，
 *     具体见 pickAcceptTag()。
 */
class NfaToDfa
{
public:
    /** 状态数上限：防止子集爆炸导致内存失控 */
    static const int kMaxStates = 20000;

    NfaToDfa() = default;

    /**
     * @brief 执行子集构造。
     * @param nfa 输入 NFA。
     * @param out 输出 DFA。
     * @param onProgress 进度回调（可空）：形参是"已构造的 DFA 状态数"。
     * @return 成功为 true；失败时 error() 给出原因。
     *
     * @note 为什么需要进度回调：TINY 规模下一次子集构造实测约 9 秒，
     *       界面在此期间完全不响应（Windows 超过 5 秒就把窗口标成"未响应"）。
     *       回调**每 32 个状态**触发一次，界面层借机 `processEvents()` 并刷新
     *       状态栏，窗口就不会被判为卡死。
     *       回调里**不要**做重活：它是在算法主循环里被同步调用的。
     */
    bool construct(const Nfa &nfa, Dfa &out,
                   const std::function<void(int)> &onProgress = {});

    /** @brief 最近一次错误信息 */
    QString error() const { return m_error; }

    /**
     * @brief 在多个候选标签中挑选该 DFA 状态最终采用的标签。
     * @param tags 同一 NFA 状态（或状态集合）上的多个候选标签。
     * @return 选中的标签；tags 为空时返回 code 为 -1 的空标签。
     * @note 裁定规则（与 AGENT.md 4.1 节的"最长匹配平局裁定"一致）：
     *        1. 非注释标签优先于注释标签（关键字/单词优先于注释）；
     *        2. 同类之间按标签在标签列表中的先后（即定义在规格文件中的声明顺序）。
     */
    static AcceptTag pickAcceptTag(const QVector<AcceptTag> &tags);

private:
    /** @brief 判断某个 NFA 状态集合是否为接受集合，并给出标签 */
    static bool isAcceptSet(const Nfa &nfa, const QSet<int> &states, AcceptTag &tag);

    QString m_error;  ///< 错误信息
};

}  // namespace ccd
