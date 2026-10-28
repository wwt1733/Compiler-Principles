/******************************************************************************
 * (1) 版权信息 : 编译原理课程设计 - 华南师范大学计算机学院
 * (2) 文件名称 : LexReader.cpp
 *     标识符   : LexReader
 *     摘要     : 单词编码文件读取与编码翻译的实现（第一、二项任务的接口）。
 * (3) 版本号   : 1.0.0       作者/修改者 : <学号 姓名>   完成日期 : 2026-XX-XX
 * (4) 版本历史 : 1.0.0  2026-XX-XX  首次实现（阶段 6）
 *****************************************************************************/

#include "syntax/LexReader.h"

#include <QSet>

#include <algorithm>

#include "common/FileUtil.h"

namespace ccd {

/** 功能：渲染为一行显示文本。输入：无。输出：文本。 */
QString LexItem::toDisplayString() const
{
    return QStringLiteral("%1 <%2>%3  第%4行")
        .arg(code, 4)
        .arg(symbol.isEmpty() ? QStringLiteral("?") : symbol)
        .arg(value.isEmpty() ? QString() : QStringLiteral(" 值=\"%1\"").arg(value))
        .arg(line);
}

/** 功能：输入结束标记。输入：无。输出："$"。 */
QString LexReader::endMarker()
{
    return QStringLiteral("$");
}

/** 功能：指定文法。输入：grammar。输出：无。 */
void LexReader::setGrammar(const Grammar &grammar)
{
    m_grammar = grammar;
    m_hasGrammar = true;
}

/**
 * 功能：解析 .lex 文本。
 * 输入：text。输出：无致命错误为 true。
 * 注意：读出的记号在最后会由 symbols() 自动追加 `$`，本函数不追加，
 *       这样 items() 对界面展示而言是"文件里实际有什么"的忠实反映。
 */
bool LexReader::parse(const QString &text)
{
    clear();

    const QStringList lines = FileUtil::splitMeaningfulLines(text);
    if (lines.isEmpty()) {
        m_warnings << QStringLiteral("单词编码文件为空。");
        return false;
    }

    int lineNo = 0;
    for (const QString &rawLine : lines) {
        ++lineNo;
        const QString line = rawLine.trimmed();
        if (line.isEmpty() || line.startsWith(QLatin1Char('#')))
            continue;
        parseLine(line, lineNo);
    }

    if (m_items.isEmpty()) {
        m_warnings << QStringLiteral("未从单词编码文件中读出任何记号。");
        return false;
    }
    return true;
}

/**
 * 功能：解析一行 .lex。
 * 输入：line、lineNo。输出：成功为 true。
 *
 * 注意 —— 行格式（AGENT.md §4.3）为 `<编码> [值]` 三种形态：
 *   1. `206`            —— 关键字/专用符号：只有编码；
 *   2. `101 "xXx"`      —— 标识符：编码 + **带引号的原文**（原文里可能有空格）；
 *   3. `100 007`        —— 数字：编码 + 字面量（保留前导零）；
 *   4. `EOF`            —— 结束标记，丢弃。
 * 因此解析时**不能**简单按空白切分第二段：ID 的原文要用引号配对来取，
 * 否则 `"a b"` 这种含空格的标识符会被截断。
 */
bool LexReader::parseLine(const QString &line, int lineNo)
{
    // 结束标记
    if (line.compare(QStringLiteral("EOF"), Qt::CaseInsensitive) == 0)
        return true;

    // 取编码
    int pos = 0;
    while (pos < line.size() && !line.at(pos).isSpace())
        ++pos;
    const QString codeText = line.left(pos);
    bool ok = false;
    const int code = codeText.toInt(&ok);
    if (!ok) {
        m_warnings << QStringLiteral(".lex 第 %1 行：首段 \"%2\" 不是数字编码，已跳过。")
                          .arg(lineNo)
                          .arg(codeText);
        return false;
    }

    // 取可选的"值"
    QString value;
    int rest = pos;
    while (rest < line.size() && line.at(rest).isSpace())
        ++rest;
    if (rest < line.size()) {
        if (line.at(rest) == QLatin1Char('"')) {
            // 引号配对：支持原文中的空格
            const int close = line.indexOf(QLatin1Char('"'), rest + 1);
            if (close < 0) {
                m_warnings << QStringLiteral(".lex 第 %1 行：引号未闭合，值按剩余全文处理。")
                                  .arg(lineNo);
                value = line.mid(rest + 1);
            } else {
                value = line.mid(rest + 1, close - rest - 1);
            }
        } else {
            value = line.mid(rest).trimmed();
        }
    }

    LexItem item;
    item.code = code;
    item.value = value;
    item.line = lineNo;

    if (!m_hasGrammar) {
        m_warnings << QStringLiteral("尚未注入文法，无法把编码 %1 翻译成文法符号。").arg(code);
    } else {
        item.symbol = m_grammar.terminalNames().value(code);
        if (item.symbol.isEmpty()) {
            // 编码表里查不到 —— 属于"词法定义的单词没在文法里出现"，
            // 是真实的不一致问题，必须报出来而不是静默丢弃
            m_warnings << QStringLiteral(".lex 第 %1 行：编码 %2 在文法 %3 的终结符表中"
                                         "找不到对应符号。")
                              .arg(lineNo)
                              .arg(code)
                              .arg(m_grammar.startSymbol());
        }
    }

    m_items.append(item);
    return true;
}

/**
 * 功能：从文件读取并解析。
 * 输入：path、error。输出：成功为 true。
 */
bool LexReader::loadFromFile(const QString &path, QString &error)
{
    bool ok = false;
    const QString text = FileUtil::readText(path, &ok);
    if (!ok) {
        error = QStringLiteral("无法读取单词编码文件：%1").arg(path);
        return false;
    }
    return parse(text);
}

/**
 * 功能：语法分析用的符号序列（末尾自动追加 `$`）。
 * 输入：无。输出：符号列表。
 * 注意：符号为空的记号（编码翻译失败）**不并入**序列，否则分析器会在
 *       一个文法里根本不存在的符号上查表，报出难以理解的错误。
 */
QStringList LexReader::symbols() const
{
    QStringList out;
    out.reserve(m_items.size() + 1);
    for (const LexItem &it : m_items) {
        if (!it.symbol.isEmpty())
            out << it.symbol;
    }
    out << endMarker();
    return out;
}

/** 功能：与 symbols() 一一对应的行号。输入：无。输出：行号列表。 */
QVector<int> LexReader::symbolLines() const
{
    QVector<int> out;
    out.reserve(m_items.size() + 1);
    int last = 1;
    for (const LexItem &it : m_items) {
        if (it.symbol.isEmpty())
            continue;
        out.append(it.line);
        last = it.line;
    }
    out.append(last);
    return out;
}

/** 功能：与 symbols() 一一对应的单词值。输入：无。输出：值列表。 */
QStringList LexReader::symbolValues() const
{
    QStringList out;
    out.reserve(m_items.size() + 1);
    for (const LexItem &it : m_items) {
        if (!it.symbol.isEmpty())
            out << it.value;
    }
    out << QString();
    return out;
}

/** 功能：无法翻译的编码集合。输入：无。输出：升序去重列表。 */
QList<int> LexReader::unknownCodes() const
{
    QSet<int> seen;
    for (const LexItem &it : m_items) {
        if (it.symbol.isEmpty())
            seen.insert(it.code);
    }
    QList<int> out = seen.values();
    std::sort(out.begin(), out.end());
    return out;
}

/** 功能：源程序总行数。输入：无。输出：行数。 */
int LexReader::sourceLineCount() const
{
    int last = 0;
    for (const LexItem &it : m_items)
        last = qMax(last, it.line);
    return last;
}

/** 功能：一句话摘要。输入：无。输出：文本。 */
QString LexReader::summary() const
{
    return QStringLiteral("单词编码文件：%1 个记号、%2 行源程序%3")
        .arg(m_items.size())
        .arg(sourceLineCount())
        .arg(m_warnings.isEmpty() ? QString()
                                  : QStringLiteral("；%1 条警告").arg(m_warnings.size()));
}

/** 功能：清空。输入：无。输出：无。 */
void LexReader::clear()
{
    m_items.clear();
    m_warnings.clear();
}

}  // namespace ccd
