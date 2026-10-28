/******************************************************************************
 * (1) 版权信息 : 编译原理课程设计 - 华南师范大学计算机学院
 * (2) 文件名称 : Types.cpp
 *     标识符   : types
 *     摘要     : 公共数据结构中需要算法实现的部分：NFA 的 ε 闭包与 move、
 *                DFA 状态表重建、产生式与动作的规格化文本、LR 项目的哈希、
 *                语法树结点管理。
 * (3) 版本号   : 1.0.0       作者/修改者 : <学号 姓名>   完成日期 : 2026-XX-XX
 * (4) 版本历史 : 1.0.0  2026-XX-XX  首次实现
 *****************************************************************************/

#include "common/Types.h"

namespace ccd {

// ---------------------------------------------------------------------------
// 全局常量
// ---------------------------------------------------------------------------

/** 空串在内部统一使用的符号名；任务书 p.3 用 '#'，p.8 说明 mini-C 输入用 '@' */
const QString kEpsilon = QStringLiteral("#");

// ---------------------------------------------------------------------------
// RegexDef
// ---------------------------------------------------------------------------

QVector<int> RegexDef::codeList() const
{
    QVector<int> result;
    if (code < 0)
        return result;  // 宏定义无编码

    if (!launch) {
        result.append(code);
        return result;
    }
    // 带 L（Launch）后缀：从起始编码起，按关键字个数连续分配编码
    for (int i = 0; i < keywordList.size(); ++i)
        result.append(code + i);
    return result;
}

// ---------------------------------------------------------------------------
// CharClassAlphabet
// ---------------------------------------------------------------------------

/**
 * 功能：字节值 → 类下标。
 * 输入：byte —— 0~255 的字节值（超出范围按低 8 位处理）。
 * 输出：classes 中的下标；不属于任何类时返回 -1。
 * 注意：byteClass 恒为 256 项，但仍做长度检查，避免结构被半填充时越界。
 */
int CharClassAlphabet::indexOfByte(int byte) const
{
    const int b = byte & 0xFF;
    if (b >= byteClass.size())
        return -1;
    return byteClass.at(b);
}

/** 功能：字节值 → 符号键。输入：字节值。输出：符号键（无类时为空串）。 */
QString CharClassAlphabet::symbolOfByte(int byte) const
{
    const int index = indexOfByte(byte);
    if (index < 0 || index >= classes.size())
        return QString();
    return classes.at(index).key;
}

/** 功能：字节值 → 显示名。输入：字节值。输出：显示名（无类时为空串）。 */
QString CharClassAlphabet::nameOfByte(int byte) const
{
    const int index = indexOfByte(byte);
    if (index < 0 || index >= classes.size())
        return QString();
    return classes.at(index).name;
}

/**
 * 功能：符号键 → 显示名。
 * 输入：key —— 类的符号键。
 * 输出：显示名；未找到时返回空串。
 * 注意：类数只有二十上下，线性查找足够，不必再维护一张哈希表。
 */
QString CharClassAlphabet::nameOfSymbol(const QString &key) const
{
    for (const CharClassInfo &info : classes) {
        if (info.key == key)
            return info.name;
    }
    return QString();
}

/**
 * 功能：生成"名字 = 字节集"的逐行图例。
 * 输入：无（使用本对象）。
 * 输出：每行形如 "letter = [A-Za-z]" 的文本。
 * 注意：内容全部是可见 ASCII，可安全写进 DOT 或日志。
 */
QString CharClassAlphabet::legendText() const
{
    QStringList lines;
    lines.reserve(classes.size());
    for (const CharClassInfo &info : classes)
        lines << QStringLiteral("%1 = %2").arg(info.name, info.setText);
    return lines.join(QLatin1Char('\n'));
}

// ---------------------------------------------------------------------------
// Nfa：ε 闭包与 move
// ---------------------------------------------------------------------------

/**
 * 功能：求 NFA 状态集合的 ε 闭包。
 * 输入：s —— 初始状态集合。
 * 输出：包含 s 且对 ε 边封闭的最大状态集合。
 * 注意：使用显式工作栈（不用递归），避免深正则式导致栈溢出。
 */
QSet<int> Nfa::epsilonClosure(const QSet<int> &s) const
{
    QSet<int> result = s;
    QVector<int> stack;
    stack.reserve(s.size());

    // 建立"源状态 → ε 边目的状态列表"的索引，避免每次遍历全部边
    QMap<int, QVector<int>> epsilonEdges;
    for (const Transition &e : edges) {
        if (e.isEpsilon)
            epsilonEdges[e.from].append(e.to);
    }

    for (int st : s)
        stack.append(st);

    while (!stack.isEmpty()) {
        const int cur = stack.takeLast();
        const auto it = epsilonEdges.constFind(cur);
        if (it == epsilonEdges.constEnd())
            continue;
        for (int next : it.value()) {
            if (!result.contains(next)) {
                result.insert(next);
                stack.append(next);
            }
        }
    }
    return result;
}

/**
 * 功能：求状态集合经单个输入符号转移后的状态集合（不取 ε 闭包）。
 * 输入：s —— 起始状态集合；ch —— 输入符号（单字符）。
 * 输出：move(s, ch)。
 * 注意：调用者若需要完整转移，应再对结果求一次 ε 闭包。
 */
QSet<int> Nfa::move(const QSet<int> &s, const QString &ch) const
{
    QSet<int> result;
    for (const Transition &e : edges) {
        if (e.isEpsilon || e.symbol != ch)
            continue;
        if (s.contains(e.from))
            result.insert(e.to);
    }
    return result;
}

/**
 * 功能：收集 NFA 中出现的全部输入符号（不含 ε）。
 * 输入：无（使用本对象的 edges）。
 * 输出：去重并按字符编码排序的符号列表。
 * 注意：返回顺序稳定，保证多次运行结果一致，便于测试用例逐字节比对。
 */
QStringList Nfa::collectAlphabet() const
{
    QSet<QString> set;
    for (const Transition &e : edges) {
        if (!e.isEpsilon && !e.symbol.isEmpty())
            set.insert(e.symbol);
    }
    QStringList list = set.values();
    list.sort();
    return list;
}

// ---------------------------------------------------------------------------
// Dfa
// ---------------------------------------------------------------------------

/**
 * 功能：重新设定 DFA 的状态数并清空全部状态表。
 * 输入：n —— 新的状态数。
 * 输出：无（就地修改）。
 * 注意：会同时重置 accepting / acceptCode / subsets / acceptTag，调用后需重新填充。
 */
void Dfa::resize(int n)
{
    stateCount = n;
    trans.clear();
    trans.resize(n);
    accepting.fill(false, n);
    acceptCode.fill(-1, n);
    acceptIsComment.fill(false, n);
    subsets.clear();
    subsets.resize(n);
    acceptTag.clear();
    acceptTag.resize(n);
}

// ---------------------------------------------------------------------------
// Production
// ---------------------------------------------------------------------------

/**
 * 功能：产生式的规格化文本。
 * 输入：无（使用本对象）。
 * 输出：形如 "exp -> exp addop term"；空产生式输出 "A -> #"。
 * 注意：仅供界面显示与测试比对，不参与算法。
 */
QString Production::toString() const
{
    if (right.isEmpty())
        return left + QStringLiteral(" -> ") + kEpsilon;
    return left + QStringLiteral(" -> ") + right.join(QLatin1Char(' '));
}

// ---------------------------------------------------------------------------
// LR 项目的哈希
// ---------------------------------------------------------------------------

/** 功能：LrItem 的哈希函数（供 QSet 去重）。输入：项目与随机种子。输出：哈希值。 */
uint qHash(const LrItem &key, uint seed)
{
    return ::qHash(key.prodId, seed) ^ ::qHash(key.dot, seed + 0x9e3779b9u);
}

/** 功能：Lr1Item 的哈希函数。输入：项目与随机种子。输出：哈希值。 */
uint qHash(const Lr1Item &key, uint seed)
{
    uint h = ::qHash(key.prodId, seed);
    h ^= ::qHash(key.dot, seed + 0x9e3779b9u);
    h ^= ::qHash(key.lookahead, seed + 0x85ebca6bu);
    return h;
}

// ---------------------------------------------------------------------------
// ParseAction
// ---------------------------------------------------------------------------

/**
 * 功能：分析表动作的规格化文本。
 * 输入：无（使用本对象）。
 * 输出："s3"（移进到状态 3）、"r2"（按产生式 2 规约）、"acc"、"error"。
 * 注意：与实验报告中的分析表书写习惯一致。
 */
QString ParseAction::toString() const
{
    switch (type) {
    case ActionType::Shift:
        return QStringLiteral("s%1").arg(value);
    case ActionType::Reduce:
        return QStringLiteral("r%1").arg(value);
    case ActionType::Accept:
        return QStringLiteral("acc");
    case ActionType::Error:
    default:
        return QStringLiteral("error");
    }
}

// ---------------------------------------------------------------------------
// SyntaxTreeData
// ---------------------------------------------------------------------------

/**
 * 功能：新建一个语法树结点。
 * 输入：label —— 结点标签；isTerminal —— 是否叶结点；line —— 源程序行号。
 * 输出：新结点的下标。
 * 注意：结点以数组形式集中存放，下标即结点编号，不使用裸指针。
 */
int SyntaxTreeData::addNode(const QString &label, bool isTerminal, int line)
{
    SyntaxNode node;
    node.label = label;
    node.isTerminal = isTerminal;
    node.line = line;
    nodes.append(node);
    return nodes.size() - 1;
}

/**
 * 功能：统计语法树的叶结点个数。
 * 输入：无（使用本对象）。
 * 输出：叶结点数量。
 * 注意：用于自测断言"叶结点数 = 参与分析的单词数"。
 */
int SyntaxTreeData::leafCount() const
{
    int count = 0;
    for (const SyntaxNode &n : nodes) {
        if (n.isTerminal || n.children.isEmpty())
            ++count;
    }
    return count;
}

/**
 * 功能：按 parent 字段重建每个结点的 children 列表。
 * 输入：无（使用本对象）。
 * 输出：无。
 * 注意：1. 父子关系统一记在 parent 上（规约自底向上，建子结点时父结点还不存在，
 *          没法往 children 里追加；而 parent 可以随时回填）；
 *       2. 重建时跳过非法下标、自环，以及会在树上形成环的父子关系 ——
 *          这样语义动作即使写错，最多出现"结点不可达"，绝不会让遍历死循环。
 */
void SyntaxTreeData::rebuildChildren()
{
    for (SyntaxNode &n : nodes)
        n.children.clear();

    const int n = nodes.size();
    for (int i = 0; i < n; ++i) {
        const int p = nodes.at(i).parent;
        if (p < 0 || p >= n || p == i)
            continue;

        // 环检测：沿 parent 链上溯，若走回自己说明这条边会成环，丢弃
        int cur = p;
        bool cycle = false;
        for (int guard = 0; guard <= n && cur >= 0 && cur < n; ++guard) {
            if (cur == i) {
                cycle = true;
                break;
            }
            cur = nodes.at(cur).parent;
        }
        if (cycle)
            continue;

        nodes[p].children.append(i);
    }
}

/** 功能：从根出发能走到的结点个数。输入：无。输出：可达结点数。 */
int SyntaxTreeData::reachableCount() const
{
    if (root < 0 || root >= nodes.size())
        return 0;

    QVector<bool> seen(nodes.size(), false);
    QVector<int> stack;
    stack.append(root);
    int count = 0;
    while (!stack.isEmpty()) {
        const int v = stack.takeLast();
        if (v < 0 || v >= nodes.size() || seen.at(v))
            continue;
        seen[v] = true;
        ++count;
        for (int c : nodes.at(v).children)
            stack.append(c);
    }
    return count;
}

/**
 * 功能：丢弃从根不可达的结点，并重新编号。
 * 输入：无（使用本对象）。
 * 输出：无。
 * 注意：1. 必须在 rebuildChildren() 之后调用 —— 要先有 children 才能沿树标记可达；
 *       2. 重编号时同步修正 root 与每个结点的 parent，避免下标错位；
 *       3. 空树或根下标非法时直接清空，不留半截数据。
 */
void SyntaxTreeData::pruneUnreachable()
{
    if (root < 0 || root >= nodes.size()) {
        nodes.clear();
        root = -1;
        return;
    }

    // 标记可达
    QVector<bool> keep(nodes.size(), false);
    QVector<int> stack;
    stack.append(root);
    while (!stack.isEmpty()) {
        const int v = stack.takeLast();
        if (v < 0 || v >= nodes.size() || keep.at(v))
            continue;
        keep[v] = true;
        for (int c : nodes.at(v).children)
            stack.append(c);
    }

    // 按原顺序保留可达结点，并建立"旧下标 -> 新下标"映射
    QVector<int> remap(nodes.size(), -1);
    QVector<SyntaxNode> kept;
    kept.reserve(nodes.size());
    for (int i = 0; i < nodes.size(); ++i) {
        if (!keep.at(i))
            continue;
        remap[i] = kept.size();
        kept.append(nodes.at(i));
    }

    for (SyntaxNode &n : kept) {
        const int p = n.parent;
        n.parent = (p >= 0 && p < remap.size() && remap.at(p) >= 0) ? remap.at(p) : -1;
    }

    nodes = kept;
    root = remap.at(root);
    rebuildChildren();
}

}  // namespace ccd
