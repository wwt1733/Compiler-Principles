/******************************************************************************
 * (1) 版权信息 : 编译原理课程设计 - 华南师范大学计算机学院
 * (2) 文件名称 : SemanticsAction.h
 *     标识符   : SemanticsAction
 *     摘要     : 语法树结构（语义动作）文件的解析与查询（第二项任务 2.7）。
 *                用户在数据文件里为**每一条产生式**给出一行，规定该产生式
 *                规约时应如何构造语法树结点。本模块只负责"读 + 查"，
 *                具体的建树动作由 syntax/SyntaxTree 在规约时执行。
 *                文件格式契约见 AGENT.md §10。
 *                本文件只依赖 QtCore。
 * (3) 版本号   : 1.0.0       作者/修改者 : <学号 姓名>   完成日期 : 2026-XX-XX
 * (4) 版本历史 : 1.0.0  2026-XX-XX  首次实现（阶段 6）
 *****************************************************************************/

#pragma once

#include <QString>
#include <QStringList>
#include <QVector>

#include "syntax/Grammar.h"

namespace ccd {

/**
 * @brief 一个"建结点"动作组。
 *
 * 一行的文本被拆成若干**以分号分隔的组**；每组建一个树结点，
 * **最后一个组**建出的结点就是这次规约的结果。组有三种写法：
 *
 * | 写法 | 含义 |
 * |---|---|
 * | `L` 或 `L c1 c2 …` | **新建**结点，标签取**产生式左部符号名**；后面的数字依次是"取右部第几项当子结点"（从 0 计数） |
 * | `R n` 或 `R n c1 c2 …` | **复用**右部第 n 项已有的结点作为本组结点（该位置没有结点时才新建，标签取右部第 n 个符号名）；n 之后是子结点引用 |
 * | `-1 n` | 不建结点，直接把右部第 n 项的树根传递上去（单链产生式用它最简洁） |
 *
 * `R n` 的"复用"是表达"操作符结点带两侧操作数"的关键。例如
 * `exp -> simple-exp comparison-op simple-exp`（右部三项，下标 0/1/2）应写成：
 *
 *     R1 0 2
 *
 *   → 取右部第 1 项那个结点（即 `comparison-op -> LT` 建出的 `<` 结点），
 *     把右部第 0 项（左侧 simple-exp）与第 2 项（右侧 simple-exp）
 *     挂成它的子结点；本次规约的结果就是 `<` 结点本身。
 *   ⇒ 与老师参考树同形（参考树里 `<` 结点直接带两个操作数）。
 *   若这里改成"新建一个结点"，比较运算符就会多出一个空壳结点，
 *   真正的 `<` 反而变成不可达的孤岛。
 */
struct SemanticsGroup {
    /** 结点构造方式 */
    enum Kind {
        BuildLhs,    ///< `L`：新建结点，标签取产生式左部符号名
        BuildSymbol, ///< `R n`：复用右部第 n 项的结点（没有则新建，标签取右部第 n 个符号）
        PassThrough  ///< `-1 n`：不建结点，传递右部第 n 个符号的树根
    };

    Kind kind = BuildLhs;      ///< 构造方式
    int symbolIndex = -1;      ///< BuildSymbol 时：复用/取标签的右部符号下标
    QVector<int> children;     ///< 子结点引用：右部符号下标（按书写顺序）
    int sourceLine = 0;        ///< 该组在语义文件中的行号（1 起，便于报错）

    /** @brief 便于界面显示的一句话描述 */
    QString toString() const;

    /** @brief 还原成文件里的原始写法（供界面"原始动作"列显示） */
    QString toRawText() const;
};

/**
 * @brief 语义动作文件解析器。
 *
 * 调用注意：
 *  1. 使用流程为 构造函数 → loadFromText()/loadFromFile() → actionAt(产生式号)；
 *  2. **行序 = 产生式出现顺序**。`Grammar::loadFromText` 赋的产生式 id 从 0 起
 *     按出现顺序递增，且**增广产生式尚未加入**，因此本类按"未增广"的产生式
 *     序号索引；增广产生式（`S' -> S`）没有语义动作，`actionAt()` 对它返回
 *     默认动作（建左部结点），这正好让整棵树有唯一的根；
 *  3. 文件行数少于产生式条数时，缺失的行按默认动作处理（**不报错**），
 *     这样任何文法都能出树，符合"用户定义的语法树结构可以为空"的开放要求；
 *  4. 行内格式异常（如 `R` 后面没给序号）时报错并指出行号，但**不中断**解析，
 *     已成功的行仍然保留。
 */
class SemanticsActionSet
{
public:
    SemanticsActionSet() = default;

    /**
     * @brief 从文本载入语义动作。
     * @param text 文件内容（任意换行风格）。
     * @return 没有任何错误行为 true；有错误时错误信息见 errors()。
     */
    bool loadFromText(const QString &text);

    /**
     * @brief 从文件载入语义动作。
     * @param path  文件路径。
     * @param error 输出：失败原因。
     * @return 成功为 true。
     */
    bool loadFromFile(const QString &path, QString &error);

    /** @brief 全部动作，下标 = 产生式号（顺序与文法文件一致） */
    const QVector<QVector<SemanticsGroup>> &actions() const { return m_actions; }

    /** @brief 有语义动作行的产生式条数（= 文件中有效行数） */
    int count() const { return m_actions.size(); }

    /**
     * @brief 取第 index 条产生式的动作组列表。
     * @param index 产生式号（从 0 起）。
     * @return 动作组列表；越界（文件里没有这一行）时返回**默认动作**
     *         （单个 `BuildLhs` 组、无子结点），保证任何文法都能出树。
     */
    QVector<SemanticsGroup> actionAt(int index) const;

    /** @brief 是否为默认动作（文件里没有给出这一行） */
    bool isDefaultAt(int index) const { return index < 0 || index >= m_actions.size(); }

    /** @brief 解析错误列表（每行一条，含行号） */
    const QStringList &errors() const { return m_errors; }

    /** @brief 一句话摘要 */
    QString summary() const;

    /** @brief 把解析结果整理成便于界面显示的表（行=产生式号） */
    void buildSummaryTable(const Grammar &grammar, QStringList &headers,
                           QList<QStringList> &rows) const;

    /** @brief 清空 */
    void clear();

private:
    /** 每条产生式对应一组动作组；下标 = 产生式号，值 = 该产生式的动作组列表 */
    QVector<QVector<SemanticsGroup>> m_actions;
    QStringList m_errors;  ///< 错误列表

    /** @brief 解析一行，解析结果追加到 m_actions */
    bool parseLine(const QString &line, int lineNo);
};

}  // namespace ccd
