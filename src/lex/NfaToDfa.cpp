/******************************************************************************
 * (1) 版权信息 : 编译原理课程设计 - 华南师范大学计算机学院
 * (2) 文件名称 : NfaToDfa.cpp
 *     标识符   : NfaToDfa
 *     摘要     : 子集构造法的实现。用"状态集合 → DFA 状态编号"的哈希表做
 *                去重，用显式队列做广度优先扩展；每个 DFA 状态都记录其对应的
 *                NFA 状态集合，便于在界面上展示对照关系。
 * (3) 版本号   : 1.0.0       作者/修改者 : <学号 姓名>   完成日期 : 2026-XX-XX
 * (4) 版本历史 : 1.0.0  2026-XX-XX  首次实现
 *****************************************************************************/

#include "lex/NfaToDfa.h"

#include <QQueue>

#include <algorithm>

#include "common/Logger.h"

namespace ccd {

namespace {

/**
 * 功能：把状态集合转换为可比较的排序列表。
 * 输入：states —— 状态集合。
 * 输出：升序排列的状态号列表。
 * 注意：QSet 的遍历顺序不稳定，必须先排序再用于哈希键与界面显示，
 *       否则同一输入多次运行可能得到不同的 DFA 状态编号，导致测试无法复现。
 */
QVector<int> sortedStates(const QSet<int> &states)
{
    QVector<int> result;
    result.reserve(states.size());
    for (int state : states)
        result.append(state);
    std::sort(result.begin(), result.end());
    return result;
}

/**
 * 功能：把状态集合编码为哈希表的键。
 * 输入：states —— 已排序的状态号列表。
 * 输出：形如 "1,3,7" 的键字符串。
 * 注意：以逗号分隔并用首尾逗号包裹，避免 "1,3" 与 "13" 之类的歧义。
 */
QString setKey(const QVector<int> &sorted)
{
    QString key = QStringLiteral(",");
    for (int state : sorted) {
        key += QString::number(state);
        key += QLatin1Char(',');
    }
    return key;
}

}  // namespace

/**
 * 功能：在多个候选标签中挑选最终采用的标签。
 * 输入：tags —— 候选标签列表。
 * 输出：选中的标签。
 * 注意：
 *  1. 非注释标签优先于注释标签；
 *  2. 同类之间取列表中第一个（即规格文件中先声明者），
 *     这与"多个正则式同时匹配时按声明顺序取编码"的约定一致。
 */
AcceptTag NfaToDfa::pickAcceptTag(const QVector<AcceptTag> &tags)
{
    AcceptTag empty;
    empty.code = -1;
    if (tags.isEmpty())
        return empty;

    // 第一轮：优先找非注释标签
    for (const AcceptTag &tag : tags) {
        if (!tag.isComment)
            return tag;
    }
    // 第二轮：全部是注释标签，取第一个
    return tags.first();
}

/**
 * 功能：判断某个 NFA 状态集合是否为接受集合。
 * 输入：nfa —— NFA；states —— 待判断的状态集合；tag —— 输出选中的标签。
 * 输出：集合中至少有一个接受状态时为 true。
 * 注意：多个接受状态并存时，把所有标签汇总后交给 pickAcceptTag() 统一裁定。
 */
bool NfaToDfa::isAcceptSet(const Nfa &nfa, const QSet<int> &states, AcceptTag &tag)
{
    QVector<AcceptTag> candidates;
    for (int state : states) {
        const auto it = nfa.accepts.constFind(state);
        if (it != nfa.accepts.constEnd())
            candidates.append(it.value());
    }
    if (candidates.isEmpty())
        return false;
    tag = pickAcceptTag(candidates);
    return true;
}

/**
 * 功能：执行子集构造法，把 NFA 转换为 DFA。
 * 输入：nfa —— 输入 NFA；out —— 输出 DFA。
 * 输出：成功为 true。
 * 注意：
 *  1. 字母表取自 NFA 中实际出现的全部输入符号（已排序，保证可复现）；
 *  2. DFA 状态编号按广度优先的发现顺序分配，初态固定为 0；
 *  3. 若某个 DFA 状态在某符号上没有转移，"状态转换表"中该格留空
 *     （不引入"死状态"，因为词法分析器靠最长匹配回退，不需要死状态收尾）。
 */
bool NfaToDfa::construct(const Nfa &nfa, Dfa &out,
                           const std::function<void(int)> &onProgress)
{
    m_error.clear();
    out = Dfa();

    if (nfa.stateCount <= 0) {
        m_error = QStringLiteral("输入的 NFA 为空（状态数为 0）。");
        return false;
    }
    if (nfa.start < 0 || nfa.start >= nfa.stateCount) {
        m_error = QStringLiteral("NFA 的开始状态编号非法：%1。").arg(nfa.start);
        return false;
    }

    const QStringList alphabet = nfa.alphabet.isEmpty() ? nfa.collectAlphabet() : nfa.alphabet;
    if (alphabet.isEmpty()) {
        m_error = QStringLiteral("NFA 的字母表为空，无法进行子集构造。");
        return false;
    }

    out.alphabet = alphabet;
    // 字符类字母表必须随 DFA 一起传下去：界面要靠它把符号键（@0）显示成类名
    // （letter），代码生成要靠它把符号键展开成字节区间判定。
    // 注意本函数开头已经 `out = Dfa()`，所以这里必须重新赋值。
    out.alphabetInfo = nfa.alphabetInfo;

    // ------------------------------------------------------------------
    // 转移索引：把"按符号找边"和"按状态找 ε 边"预先把好。
    //
    // 为什么必须这么做：内层循环是「DFA 状态数 × 字母表大小」次
    // `move` + `epsilonClosure`。TINY 是 400 × 约 256 ≈ 10 万次，
    // 而 `Nfa::move` 每调用一次都要**线性扫描全部边**、并逐条做
    // `QString` 比较（`e.symbol != ch`），`epsilonClosure` 更是每次
    // 都重新建一遍 QMap 索引。实测这一处让整条词法流水线耗时
    // **31 秒**，界面在此期间完全无响应（Windows 超过 5 秒就显示"未响应"）。
    //
    // 改成预建索引后，每次 move 只遍历"该符号上的那几条边"，
    // ε 闭包也只沿 ε 邻接表走，整体从千万级线性扫描降到百万级以内。
    // 注意 `Nfa::move` / `Nfa::epsilonClosure` 本身保持原样不动 ——
    // 它们是清晰的定义式实现，仍然被自测当作"参考实现"直接调用。
    // ------------------------------------------------------------------
    QHash<QString, QVector<QPair<int, int>>> bySymbol;   ///< 符号 → (from, to) 列表
    QHash<int, QVector<int>> epsilonFrom;                ///< 状态 → ε 后继列表
    QHash<int, QStringList> symbolsFrom;                 ///< 状态 → 该状态出发用到的符号
    bySymbol.reserve(alphabet.size() * 4);
    for (const Transition &e : nfa.edges) {
        if (e.isEpsilon) {
            epsilonFrom[e.from].append(e.to);
        } else if (!e.symbol.isEmpty()) {
            bySymbol[e.symbol].append(qMakePair(e.from, e.to));
            QStringList &list = symbolsFrom[e.from];
            if (!list.contains(e.symbol))
                list.append(e.symbol);
        }
    }

    // 候选符号：只试"当前状态集合里某些状态确实有出边"的符号。
    // 这一步很关键 —— 整套词法规格的字母表约 256 个符号（取反集合覆盖全部
    // 字节值），但任意一个子集状态的出边只涉及其中十几个。若老老实实遍历
    // 整个字母表，就会对 400 × 256 ≈ 10 万组做"move + ε 闭包 + 排序 + 拼键"，
    // 其中绝大多数结果为空、白算一遍。实测改成候选符号后再降一个数量级。
    // 结果与遍历整个字母表**完全等价**：被跳过的符号其 move 必为空，
    // 原代码遇到空 move 也是 `continue`，不会写入转移表。
    QSet<QString> candidateCache;

    /** @brief move(s, ch)：只遍历该符号上的边（语义与 `Nfa::move` 完全一致） */
    const auto moveOf = [&bySymbol](const QSet<int> &s, const QString &ch) {
        QSet<int> result;
        const auto it = bySymbol.constFind(ch);
        if (it == bySymbol.constEnd())
            return result;
        for (const QPair<int, int> &pr : it.value()) {
            if (s.contains(pr.first))
                result.insert(pr.second);
        }
        return result;
    };

    /** @brief ε 闭包：沿 ε 邻接表走（语义与 `Nfa::epsilonClosure` 完全一致） */
    const auto closureOf = [&epsilonFrom](const QSet<int> &s) {
        QSet<int> result = s;
        QVector<int> stack;
        stack.reserve(s.size());
        for (int st : s)
            stack.append(st);
        while (!stack.isEmpty()) {
            const int cur = stack.takeLast();
            const auto it = epsilonFrom.constFind(cur);
            if (it == epsilonFrom.constEnd())
                continue;
            for (int next : it.value()) {
                if (!result.contains(next)) {
                    result.insert(next);
                    stack.append(next);
                }
            }
        }
        return result;
    };

    // 状态集合 → DFA 状态编号 的哈希表
    QHash<QString, int> setToIndex;
    // 待扩展的 DFA 状态队列（广度优先）
    QQueue<int> pending;

    // ---- 初始状态：ε-closure({nfa.start}) ----
    QSet<int> startSet;
    startSet.insert(nfa.start);
    startSet = closureOf(startSet);

    const int startIndex = 0;
    setToIndex.insert(setKey(sortedStates(startSet)), startIndex);
    out.subsets.append(startSet);
    out.accepting.append(false);
    out.acceptCode.append(-1);
    out.acceptIsComment.append(false);
    out.acceptTag.append(AcceptTag());
    out.trans.append(QMap<QString, int>());
    out.stateCount = 1;
    out.start = startIndex;

    // 初态自身也可能是接受态（例如正则式允许匹配空串），必须一并判断
    {
        AcceptTag startTag;
        if (isAcceptSet(nfa, startSet, startTag)) {
            out.accepting[startIndex] = true;
            out.acceptCode[startIndex] = startTag.code;
            out.acceptIsComment[startIndex] = startTag.isComment;
            out.acceptTag[startIndex] = startTag;
        }
    }

    pending.enqueue(startIndex);

    // ---- 广度优先扩展 ----
    int progressed = 0;
    while (!pending.isEmpty()) {
        // 每 32 个状态汇报一次进度。界面层借此 processEvents() 并刷新状态栏，
        // 窗口就不会被 Windows 判成「未响应」（TINY 规模下本函数要跑约 9 秒）。
        if (onProgress && (progressed++ % 32) == 0)
            onProgress(out.stateCount);

        const int current = pending.dequeue();
        const QSet<int> currentSet = out.subsets.at(current);

        // 只遍历"本状态集合真正可能用到的符号"，而不是整个字母表
        candidateCache.clear();
        for (int st : currentSet) {
            const auto sit = symbolsFrom.constFind(st);
            if (sit == symbolsFrom.constEnd())
                continue;
            for (const QString &sym : sit.value())
                candidateCache.insert(sym);
        }

        for (const QString &symbol : candidateCache) {
            // move(currentSet, symbol) 后再取 ε 闭包（用预建索引，见上方说明）
            QSet<int> moved = moveOf(currentSet, symbol);
            if (moved.isEmpty())
                continue;
            moved = closureOf(moved);

            const QVector<int> sorted = sortedStates(moved);
            const QString key = setKey(sorted);

            int targetIndex = setToIndex.value(key, -1);
            if (targetIndex < 0) {
                // 新子集：分配新编号
                targetIndex = out.stateCount;
                if (targetIndex >= kMaxStates) {
                    m_error = QStringLiteral("DFA 状态数超过上限 %1，可能存在子集爆炸。")
                                  .arg(kMaxStates);
                    return false;
                }
                setToIndex.insert(key, targetIndex);

                AcceptTag tag;
                const bool accepting = isAcceptSet(nfa, moved, tag);

                out.subsets.append(moved);
                out.accepting.append(accepting);
                out.acceptCode.append(accepting ? tag.code : -1);
                out.acceptIsComment.append(accepting ? tag.isComment : false);
                out.acceptTag.append(accepting ? tag : AcceptTag());
                out.trans.append(QMap<QString, int>());
                out.stateCount += 1;
                pending.enqueue(targetIndex);
            }

            out.trans[current].insert(symbol, targetIndex);
        }
    }

    LOG_INFO(QStringLiteral("DFA 构造完成（子集构造法）：%1 个状态、%2 个转移、字母表 %3 个%4。")
                 .arg(out.stateCount)
                 .arg([&out] {
                     int count = 0;
                     for (const QMap<QString, int> &row : out.trans)
                         count += row.size();
                     return count;
                 }())
                 .arg(out.alphabet.size())
                 .arg(out.alphabetInfo.isEmpty() ? QStringLiteral("符号")
                                                 : QStringLiteral("字符类")));
    return true;
}

}  // namespace ccd
