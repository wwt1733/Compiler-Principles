/******************************************************************************
 * (1) 版权信息 : 编译原理课程设计 - 华南师范大学计算机学院
 * (2) 文件名称 : DfaMinimize.cpp
 *     标识符   : DfaMinimize
 *     摘要     : DFA 最小化（划分法）的实现。核心是反复计算每个状态的
 *                "签名"（是否为接受态、输出编码、各输入符号上的转移目标所在
 *                的组号），把签名相同的状态归为一组，直到分组稳定；随后按
 *                广度优先顺序重建状态转换表。
 * (3) 版本号   : 1.0.0       作者/修改者 : <学号 姓名>   完成日期 : 2026-XX-XX
 * (4) 版本历史 : 1.0.0  2026-XX-XX  首次实现
 *****************************************************************************/

#include "lex/DfaMinimize.h"

#include <QQueue>
#include <QSet>

#include <algorithm>

#include "common/Logger.h"

namespace ccd {

namespace {

/**
 * 功能：计算 DFA 中转移边的总数。
 * 输入：dfa —— DFA。
 * 输出：转移边数。
 * 注意：仅用于日志与界面统计。
 */
int countTransitions(const Dfa &dfa)
{
    int count = 0;
    for (const QMap<QString, int> &row : dfa.trans)
        count += row.size();
    return count;
}

/**
 * 功能：把一个组（状态集合）格式化为可读文本。
 * 输入：group —— 状态集合。
 * 输出：形如 "{0,1,2}" 的文本。
 * 注意：仅用于划分过程记录。
 */
QString groupText(const QVector<int> &group)
{
    QStringList parts;
    for (int state : group)
        parts << QString::number(state);
    return QStringLiteral("{") + parts.join(QLatin1Char(',')) + QStringLiteral("}");
}

}  // namespace

/**
 * 功能：执行 DFA 最小化。
 * 输入：dfa —— 输入 DFA；out —— 输出最小化 DFA。
 * 输出：成功为 true。
 * 注意：
 *  1. **接受态必须按输出编码细分**：不同单词的接受态一旦合并，词法分析就会
 *     输出错误编码，因此初始划分中把 (是否接受, 编码, 是否注释) 作为第一层分类键；
 *  2. 状态转换表中缺失的转移用 -1 表示（不引入死状态），参与签名计算时
 *     以 -1 参与，保证"有转移"与"无转移"不会被视为等价；
 *  3. 结果状态按从初态出发的广度优先顺序重新编号，使表格利于阅读。
 */
bool DfaMinimize::minimize(const Dfa &dfa, Dfa &out)
{
    m_error.clear();
    m_mergedGroups.clear();
    m_trace.clear();
    out = Dfa();

    if (dfa.stateCount <= 0) {
        m_error = QStringLiteral("输入的 DFA 为空（状态数为 0）。");
        return false;
    }

    const QStringList alphabet = dfa.alphabet;
    if (alphabet.isEmpty()) {
        m_error = QStringLiteral("DFA 的字母表为空，无法最小化。");
        return false;
    }

    const int n = dfa.stateCount;

    // ------------------------------------------------------------------
    // 第 1 步：初始划分
    //   先按"是否接受 + 输出编码 + 是否注释"分组，再按转移关系细化。
    //   这样做的必要性：不同单词的接受态即使转移完全相同也不能合并。
    // ------------------------------------------------------------------
    QHash<QString, QVector<int>> initialBuckets;
    QStringList bucketOrder;  // 保持稳定顺序，便于结果可复现

    for (int state = 0; state < n; ++state) {
        // 注意：判定"是否接受"必须用 Dfa::accepting 标志，不能用 acceptCode >= 0。
        //       因为"接受但不输出编码"（如教材中的单正则式 NFA、注释类规则）
        //       其编码恰为 -1，若用编码判断会把它误当成非接受态。
        const bool accepting = (state < dfa.accepting.size() && dfa.accepting.at(state));
        const bool comment = (state < dfa.acceptIsComment.size() && dfa.acceptIsComment.at(state));
        const int code = accepting ? dfa.acceptCode.value(state, -1) : -1;

        // 分类键：接受态按编码细分；非接受态统一为一类
        const QString key = accepting ? QStringLiteral("A%1/%2").arg(code).arg(comment ? 1 : 0)
                                      : QStringLiteral("N");

        if (!initialBuckets.contains(key))
            bucketOrder.append(key);
        initialBuckets[key].append(state);
    }

    // 把初始划分整理为"组列表"
    QVector<QVector<int>> groups;
    for (const QString &key : bucketOrder) {
        QVector<int> group = initialBuckets.value(key);
        std::sort(group.begin(), group.end());
        groups.append(group);
    }
    m_trace.append(QStringLiteral("初始划分（按“是否接受 + 编码”分类）：共 %1 组")
                       .arg(groups.size()));
    for (const QVector<int> &group : groups)
        m_trace.append(QStringLiteral("    %1").arg(groupText(group)));

    // ------------------------------------------------------------------
    // 第 2 步：反复细化划分
    //   每轮为每个状态计算签名；同一组内签名不同的状态被拆到不同新组。
    //   循环直到组数不再增加（划分稳定）。
    // ------------------------------------------------------------------
    int round = 0;
    while (true) {
        ++round;

        // 状态 → 所属组号。
        //
        // 重要（曾经踩坑）：这一步必须在**每轮开始时**重新计算。曾经把它写在
        // 轮内但在拆分之后，导致同轮内新拆出的组号没能反映到签名里，
        // 于是等价判定出错、甚至把不等价的状态合并（TINY 的 239 个状态被
        // 错误压缩到 9 个）。每轮开头重算可保证签名基于**上一次稳定**的划分。
        QVector<int> groupOf(n, -1);
        for (int g = 0; g < groups.size(); ++g) {
            for (int state : groups.at(g))
                groupOf[state] = g;
        }

        QVector<QVector<int>> newGroups;
        bool changed = false;

        for (const QVector<int> &group : groups) {
            // 组内按签名再分组：QHash 的键即签名字符串
            QHash<QString, QVector<int>> bySignature;
            QStringList signatureOrder;

            for (int state : group) {
                // 签名只由"各输入符号上的转移目标所在组号"决定。
                //
                // 重要（曾经踩坑）：**绝不能把状态自身的编号放进签名**。
                // 否则每个状态的签名都互不相同，划分永远无法合并任何状态，
                // 最小化会退化成"原样输出"，而且表面上不会报任何错误。
                QString signature;
                for (const QString &symbol : alphabet) {
                    const auto it = dfa.trans.at(state).constFind(symbol);
                    if (it == dfa.trans.at(state).constEnd())
                        signature += QStringLiteral("-1,");
                    else
                        signature += QString::number(groupOf.at(it.value())) + QLatin1Char(',');
                }

                if (!bySignature.contains(signature))
                    signatureOrder.append(signature);
                bySignature[signature].append(state);
            }

            if (signatureOrder.size() > 1)
                changed = true;

            for (const QString &signature : signatureOrder) {
                QVector<int> sub = bySignature.value(signature);
                std::sort(sub.begin(), sub.end());
                newGroups.append(sub);
            }
        }

        groups = newGroups;

        // 记录本轮划分结果（只记录组数与分组情况，避免报告过长）
        m_trace.append(QStringLiteral("第 %1 轮细化后：共 %2 组").arg(round).arg(groups.size()));
        for (const QVector<int> &group : groups)
            m_trace.append(QStringLiteral("    %1").arg(groupText(group)));

        if (!changed)
            break;  // 划分稳定

        // 安全阀：划分的组数不会超过状态总数，超过即为实现缺陷
        if (groups.size() > n) {
            m_error = QStringLiteral("内部错误：划分组数 %1 超过状态总数 %2。")
                          .arg(groups.size())
                          .arg(n);
            return false;
        }
    }

    // ------------------------------------------------------------------
    // 第 3 步：按从初态出发的广度优先顺序重排各组的编号
    //   目的：让"最小化 DFA 状态 0"就是初态，且编号尽量连续可读。
    // ------------------------------------------------------------------
    QVector<int> groupOf(n, -1);
    for (int g = 0; g < groups.size(); ++g) {
        for (int state : groups.at(g))
            groupOf[state] = g;
    }

    // 组之间的转移关系（组 → 组）
    // 注意：同一组内的状态在同一符号上的转移必然落在同一组，这是划分稳定的
    //       充要条件，因此可以用任意代表元计算。
    QVector<QMap<QString, int>> groupTrans(groups.size());
    for (int g = 0; g < groups.size(); ++g) {
        const int representative = groups.at(g).first();
        for (const QString &symbol : alphabet) {
            const auto it = dfa.trans.at(representative).constFind(symbol);
            if (it != dfa.trans.at(representative).constEnd())
                groupTrans[g].insert(symbol, groupOf.at(it.value()));
        }
    }

    const int startGroup = groupOf.at(dfa.start);
    QVector<int> bfsOrder;          // 组号的广度优先顺序
    QVector<int> newIndexOf(groups.size(), -1);
    QQueue<int> pending;

    newIndexOf[startGroup] = 0;
    bfsOrder.append(startGroup);
    pending.enqueue(startGroup);

    while (!pending.isEmpty()) {
        const int g = pending.dequeue();
        // 按字母表顺序遍历，保证结果可复现
        for (const QString &symbol : alphabet) {
            const auto it = groupTrans.at(g).constFind(symbol);
            if (it == groupTrans.at(g).constEnd())
                continue;
            const int target = it.value();
            if (newIndexOf.at(target) < 0) {
                newIndexOf[target] = bfsOrder.size();
                bfsOrder.append(target);
                pending.enqueue(target);
            }
        }
    }
    // 理论上所有组都从初态可达（子集构造只产生可达状态）；保险起见把遗漏的补上
    for (int g = 0; g < groups.size(); ++g) {
        if (newIndexOf.at(g) < 0) {
            newIndexOf[g] = bfsOrder.size();
            bfsOrder.append(g);
        }
    }

    // ------------------------------------------------------------------
    // 第 4 步：构建输出 DFA
    // ------------------------------------------------------------------
    out.stateCount = groups.size();
    out.start = 0;
    out.alphabet = alphabet;
    // 字符类字母表要跟着一起带过去：最小化只合并状态、不改字母表，
    // 漏掉这一行最小化 DFA 页签就会退化成"按字节"显示（符号键变成 @0 这样的裸键）。
    out.alphabetInfo = dfa.alphabetInfo;
    out.trans.clear();
    out.trans.resize(out.stateCount);
    out.accepting.fill(false, out.stateCount);
    out.acceptCode.fill(-1, out.stateCount);
    out.acceptIsComment.fill(false, out.stateCount);
    out.acceptTag.clear();
    out.acceptTag.resize(out.stateCount);
    out.subsets.clear();
    out.subsets.resize(out.stateCount);

    m_mergedGroups.clear();
    m_mergedGroups.resize(out.stateCount);

    for (int newIndex = 0; newIndex < bfsOrder.size(); ++newIndex) {
        const int g = bfsOrder.at(newIndex);
        const QVector<int> &members = groups.at(g);

        // 合并来源记录（供界面显示"哪些原状态被合并了"）
        for (int state : members)
            m_mergedGroups[newIndex].append(state);
        // 同时写入 subsets，使状态转换表控件能显示"最小化后状态 ← 原状态集合"
        for (int state : members)
            out.subsets[newIndex].insert(state);

        // 接受信息取自代表元（组内所有成员的接受信息必然相同，否则不会被分到一组）
        const int representative = members.first();
        if (representative < dfa.accepting.size())
            out.accepting[newIndex] = dfa.accepting.at(representative);
        if (representative < dfa.acceptCode.size())
            out.acceptCode[newIndex] = dfa.acceptCode.at(representative);
        if (representative < dfa.acceptIsComment.size())
            out.acceptIsComment[newIndex] = dfa.acceptIsComment.at(representative);
        if (representative < dfa.acceptTag.size())
            out.acceptTag[newIndex] = dfa.acceptTag.at(representative);

        // 转移表
        for (const QString &symbol : alphabet) {
            const auto it = groupTrans.at(g).constFind(symbol);
            if (it == groupTrans.at(g).constEnd())
                continue;
            out.trans[newIndex].insert(symbol, newIndexOf.at(it.value()));
        }
    }

    LOG_INFO(QStringLiteral("DFA 最小化完成（划分法，共 %1 轮细化）：%2 → %3 个状态，转移 %4 → %5 条。")
                 .arg(round)
                 .arg(dfa.stateCount)
                 .arg(out.stateCount)
                 .arg(countTransitions(dfa))
                 .arg(countTransitions(out)));
    return true;
}

}  // namespace ccd
