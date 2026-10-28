/******************************************************************************
 * (1) 版权信息 : 编译原理课程设计 - 华南师范大学计算机学院
 * (2) 文件名称 : SemanticsAction.cpp
 *     标识符   : SemanticsAction
 *     摘要     : 语法树结构（语义动作）文件解析与查询的实现（第二项任务 2.7）。
 * (3) 版本号   : 1.0.0       作者/修改者 : <学号 姓名>   完成日期 : 2026-XX-XX
 * (4) 版本历史 : 1.0.0  2026-XX-XX  首次实现（阶段 6）
 *****************************************************************************/

#include "syntax/SemanticsAction.h"

#include <QRegularExpression>

#include "common/FileUtil.h"

namespace ccd {

/**
 * 功能：把动作组渲染成一句话描述（供界面表格显示）。
 * 输入：无。输出：文本。
 * 注意：描述里直接写出"标签取谁、子结点取谁"，因为这张表是给使用者读的 ——
 *       光看 `0 -1 1 -1` 这串数字无法判断自己写的语义函数是否符合预期。
 */
QString SemanticsGroup::toString() const
{
    QStringList parts;
    switch (kind) {
    case BuildLhs:
        parts << QStringLiteral("建结点(标签=左部)");
        break;
    case BuildSymbol:
        parts << QStringLiteral("建结点(标签=右部第%1项)").arg(symbolIndex);
        break;
    case PassThrough:
        parts << QStringLiteral("不建结点(传递第%1项)").arg(symbolIndex);
        break;
    }

    if (!children.isEmpty()) {
        QStringList refs;
        for (int c : children)
            refs << QString::number(c);
        parts << QStringLiteral("子结点=[%1]").arg(refs.join(QLatin1Char(',')));
    }
    return parts.join(QLatin1Char(' '));
}

/** 功能：还原成文件里的原始写法。输入：无。输出：文本。 */
QString SemanticsGroup::toRawText() const
{
    QStringList one;
    if (kind == PassThrough)
        one << QStringLiteral("-1") << QString::number(symbolIndex);
    else if (kind == SemanticsGroup::BuildSymbol)
        one << QStringLiteral("R%1").arg(symbolIndex);
    else
        one << QStringLiteral("L");

    for (int c : children)
        one << QString::number(c);
    return one.join(QLatin1Char(' '));
}

/**
 * 功能：从文本载入语义动作。
 * 输入：text。输出：无错误行为 true。
 * 注意：
 *  1. 空行与整行 `#` 注释**跳过且不占产生式序号** —— 否则用户在文件里加一行
 *     注释就会让后面所有行的对应关系整体错位，这种错误极难排查；
 *  2. **行尾注释必须自己剥掉**：`FileUtil::splitMeaningfulLines()` 只过滤
 *     "整行以 # 开头"的情况，不处理行尾注释。而本项目的语义文件每行末尾都
 *     写着 `# 序号 产生式` 便于对照，若不剥掉，`#` 之后的 "0 program -> ..."
 *     会被当成组记号，报出"组首记号 0 不是 L / R"。这是实际踩过的坑。
 */
bool SemanticsActionSet::loadFromText(const QString &text)
{
    clear();

    const QStringList lines = FileUtil::splitMeaningfulLines(text);
    int lineNo = 0;
    for (const QString &rawLine : lines) {
        ++lineNo;

        // 剥掉行尾注释（`#` 之后全部丢弃）
        QString line = rawLine;
        const int hash = line.indexOf(QLatin1Char('#'));
        if (hash >= 0)
            line = line.left(hash);
        line = line.trimmed();

        if (line.isEmpty())
            continue;
        parseLine(line, lineNo);
    }

    return m_errors.isEmpty();
}

/**
 * 功能：从文件载入语义动作。
 * 输入：path、error。输出：成功为 true。
 */
bool SemanticsActionSet::loadFromFile(const QString &path, QString &error)
{
    bool ok = false;
    const QString text = FileUtil::readText(path, &ok);
    if (!ok) {
        error = QStringLiteral("无法读取语义动作文件：%1").arg(path);
        return false;
    }
    return loadFromText(text);
}

/**
 * 功能：解析一行数字为若干动作组。
 * 输入：line —— 已 trim 的行；lineNo —— 行号。
 * 输出：成功为 true（失败时把原因写进 m_errors，但仍会追加动作组以保持行序）。
 *
 * 注意 —— 行格式（本项目对格式契约的最终约定，详见 AGENT.md §10）：
 *   1. 整行先用 `;` 切成若干**组**，每组建一个结点，**最后一组**的结果
 *      就是这次规约的返回值；
 *   2. 组的首字母是 `L` 或 `R`（大小写不敏感）：
 *        `L [c1 c2 ...]`    建结点，标签 = 产生式左部符号名；
 *        `R n [c1 c2 ...]`  建结点，标签 = 右部第 n 项，后面的数字是子结点引用；
 *   3. 整行写成 `-1 n` 表示不建结点、直接把右部第 n 项的树根传上去。
 *
 * 为什么不用"一串纯数字、靠 `-1` 分段"的原始写法：那种写法里数字 0 同时
 * 承担"建左部结点"和"子结点下标 0"两种含义，无法单遍无歧义解析
 * （`0 1 -1 2 -1` 既可读成 4 组、也可读成 1 组带 3 个子结点）。
 * 加一个字母前缀后语义完全确定，且仍然保持"只写数字下标"的紧凑性。
 */
bool SemanticsActionSet::parseLine(const QString &line, int lineNo)
{
    QVector<SemanticsGroup> groups;

    // 整行 `-1 n`：单链传递
    const QString trimmed = line.trimmed();
    if (trimmed.startsWith(QStringLiteral("-1"))) {
        const QStringList tk =
            trimmed.split(QRegularExpression(QStringLiteral("\\s+")), Qt::SkipEmptyParts);
        SemanticsGroup g;
        g.kind = SemanticsGroup::PassThrough;
        g.sourceLine = lineNo;
        if (tk.size() >= 2) {
            bool ok = false;
            g.symbolIndex = tk.at(1).toInt(&ok);
            if (!ok) {
                m_errors << QStringLiteral("第 %1 行：\"-1\" 之后的 \"%2\" 不是右部符号下标。")
                                .arg(lineNo)
                                .arg(tk.at(1));
                g.symbolIndex = 0;
            }
        } else {
            m_errors << QStringLiteral("第 %1 行：\"-1\" 之后缺少要传递的右部符号下标。").arg(lineNo);
        }
        groups.append(g);
        m_actions.append(groups);
        return m_errors.isEmpty();
    }

    // 按 `;` 切组
    const QStringList rawGroups = trimmed.split(QLatin1Char(';'), Qt::SkipEmptyParts);
    if (rawGroups.isEmpty()) {
        m_actions.append(groups);
        return true;
    }

    for (const QString &rawGroup : rawGroups) {
        const QStringList tk =
            rawGroup.split(QRegularExpression(QStringLiteral("\\s+")), Qt::SkipEmptyParts);
        if (tk.isEmpty())
            continue;

        SemanticsGroup g;
        g.sourceLine = lineNo;

        // 组首记号的两种写法都要接受：`L` / `L 1 2`，以及紧凑的 `L1` / `L1 2`；
        // `R` / `R 0 2` 与紧凑的 `R0 2`。
        // 紧凑写法是数据文件的主力写法（`R0 2` 比 `R 0 2` 更贴近"右部第 0 项"的语感），
        // 所以解析时必须先把首字母与紧随其后的数字拆开。
        QString head = tk.first();
        int start = 1;
        int inlineIndex = -1;

        const QChar first = head.isEmpty() ? QChar() : head.at(0).toUpper();

        // `-1 n` 也可以只作为**其中一组**出现（例：`R1 2 &0 ; -1 1`），
        // 表示"这一组不建结点、把右部第 n 项的树根传上去"。
        // 只有整行都是 `-1 n` 时才在上面那段提前返回，这里处理组内出现的情况。
        if (head == QStringLiteral("-1")) {
            g.kind = SemanticsGroup::PassThrough;
            if (tk.size() < 2) {
                m_errors << QStringLiteral("第 %1 行：\"-1\" 之后缺少要传递的右部符号下标。")
                                .arg(lineNo);
                continue;
            }
            bool pOk = false;
            g.symbolIndex = tk.at(1).toInt(&pOk);
            if (!pOk) {
                m_errors << QStringLiteral("第 %1 行：\"-1\" 之后的 \"%2\" 不是右部符号下标。")
                                .arg(lineNo)
                                .arg(tk.at(1));
                continue;
            }
            groups.append(g);
            continue;
        }

        if (first != QLatin1Char('L') && first != QLatin1Char('R')) {
            m_errors << QStringLiteral("第 %1 行：组首记号 \"%2\" 不是 L / R（应为 "
                                       "`L 子结点…` 或 `R 序号 子结点…`）。")
                            .arg(lineNo)
                            .arg(tk.first());
            continue;
        }

        if (head.size() > 1) {
            // 紧凑写法：首字母后面直接跟数字
            bool ok = false;
            inlineIndex = head.mid(1).toInt(&ok);
            if (!ok) {
                m_errors << QStringLiteral("第 %1 行：组首记号 \"%2\" 中首字母之后"
                                           "不是数字下标。")
                                .arg(lineNo)
                                .arg(tk.first());
                continue;
            }
        }

        if (first == QLatin1Char('L')) {
            g.kind = SemanticsGroup::BuildLhs;
            if (inlineIndex >= 0) {
                // `L1 …`：把紧凑写法的数字当作第一个子结点引用，
                // 但要与"L 后面直接跟子结点"区分开，因此用 start=0 表示
                // 首记号本身已消耗掉，后续 tk[1..] 全是子结点
                start = 1;
                inlineIndex = -1;   // L 没有"标签序号"，紧凑写法无意义，忽略
                // 注：`L1 2` 与 `L 1 2` 等价，都由下面的循环解析
            }
        } else {
            g.kind = SemanticsGroup::BuildSymbol;
            if (inlineIndex >= 0) {
                g.symbolIndex = inlineIndex;
                start = 1;          // 序号已从 `R0` 里取到，tk[1..] 全是子结点
            } else {
                if (tk.size() < 2) {
                    m_errors << QStringLiteral("第 %1 行：\"R\" 之后缺少标签所取的"
                                               "右部符号下标。")
                                    .arg(lineNo);
                    continue;
                }
                bool ok = false;
                g.symbolIndex = tk.at(1).toInt(&ok);
                if (!ok) {
                    m_errors << QStringLiteral("第 %1 行：\"R\" 之后的 \"%2\" 不是数字下标。")
                                    .arg(lineNo)
                                    .arg(tk.at(1));
                    continue;
                }
                start = 2;
            }
        }

        bool ok = true;
        for (int i = start; i < tk.size(); ++i) {
            bool numOk = false;
            const int v = tk.at(i).toInt(&numOk);
            if (!numOk) {
                m_errors << QStringLiteral("第 %1 行：子结点引用 \"%2\" 不是数字下标。")
                                .arg(lineNo)
                                .arg(tk.at(i));
                ok = false;
                break;
            }
            g.children.append(v);
        }
        if (!ok)
            continue;

        groups.append(g);
    }

    m_actions.append(groups);
    return m_errors.isEmpty();
}

/**
 * 功能：取第 index 条产生式的动作组列表。
 * 输入：index。输出：动作组列表。
 * 注意：越界（文件里没写这一行）时返回**默认动作**：建左部结点、不带子结点。
 *       这是一个刻意的兜底 —— 用户的语义文件可以只写自己关心的那几条产生式，
 *       没写的部分仍然能出树，"缺失行按默认动作处理"是 AGENT.md §10 的约定。
 */
QVector<SemanticsGroup> SemanticsActionSet::actionAt(int index) const
{
    if (index >= 0 && index < m_actions.size())
        return m_actions.at(index);

    SemanticsGroup g;
    g.kind = SemanticsGroup::BuildLhs;
    return QVector<SemanticsGroup>{g};
}

/** 功能：一句话摘要。输入：无。输出：文本。 */
QString SemanticsActionSet::summary() const
{
    int totalGroups = 0;
    for (const auto &v : m_actions)
        totalGroups += v.size();

    return QStringLiteral("语义动作：%1 条产生式有定义、共 %2 个建结点组%3")
        .arg(m_actions.size())
        .arg(totalGroups)
        .arg(m_errors.isEmpty() ? QString()
                                : QStringLiteral("；%1 处格式问题").arg(m_errors.size()));
}

/**
 * 功能：把解析结果整理成便于界面显示的表。
 * 输入：grammar； headers/rows —— 输出。
 * 输出：无。
 * 注意：列表头依次为 序号、产生式、原始动作、解释。原始数字列必须保留，
 *       因为使用者对照自己写的文件时看的就是那串数字。
 */
void SemanticsActionSet::buildSummaryTable(const Grammar &grammar, QStringList &headers,
                                           QList<QStringList> &rows) const
{
    headers = {QStringLiteral("序号"), QStringLiteral("产生式"), QStringLiteral("原始动作"),
               QStringLiteral("解释")};
    rows.clear();

    const int total = grammar.productionCount();
    for (int i = 0; i < total; ++i) {
        const Production &p = grammar.production(i);

        QStringList raw;
        QStringList explain;
        if (isDefaultAt(i)) {
            raw << QStringLiteral("（未给出，取默认）");
            explain << QStringLiteral("默认：建结点(标签=左部)");
        } else {
            for (const SemanticsGroup &g : m_actions.at(i)) {
                // 还原原始写法（`L …` / `R n …` / `-1 n`），便于与文件逐字对照
                raw << g.toRawText();
                explain << g.toString();
            }
        }

        rows.append({QString::number(i), p.toString(), raw.join(QStringLiteral(" | ")),
                     explain.join(QStringLiteral(" | "))});
    }
}

/** 功能：清空。输入：无。输出：无。 */
void SemanticsActionSet::clear()
{
    m_actions.clear();
    m_errors.clear();
}

}  // namespace ccd
