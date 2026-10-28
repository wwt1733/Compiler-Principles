/******************************************************************************
 * (1) 版权信息 : 编译原理课程设计 - 华南师范大学计算机学院
 * (2) 文件名称 : LexReader.h
 *     标识符   : LexReader
 *     摘要     : 单词编码文件（.lex）的读取与"数字编码 → 文法终结符"翻译。
 *                这是第一项任务与第二项任务的**接合点**：
 *                  词法阶段输出的是数字编码（如 206）；
 *                  语法阶段的文法写的是符号名（如 LT）；
 *                  本模块负责把前者翻成后者，并给出词法/语法两侧一致性检查。
 *                文件格式见 AGENT.md §4.3。
 *                本文件只依赖 QtCore。
 * (3) 版本号   : 1.0.0       作者/修改者 : <学号 姓名>   完成日期 : 2026-XX-XX
 * (4) 版本历史 : 1.0.0  2026-XX-XX  首次实现（阶段 6）
 *****************************************************************************/

#pragma once

#include <QMap>
#include <QString>
#include <QStringList>
#include <QVector>

#include "common/Types.h"
#include "syntax/Grammar.h"

namespace ccd {

/**
 * @brief 从 .lex 读出的一个记号（在 Token 之上补出"语法分析用的符号名"）。
 */
struct LexItem {
    int code = -1;        ///< 单词编码
    QString value;        ///< 单词值（ID 为原文、NUM 为数值、关键字/符号为空）
    int line = 1;         ///< 源程序行号
    QString symbol;       ///< 翻译后的文法符号名；无法对应时为空

    /** @brief 便于界面显示：`206 <LT> 第3行` */
    QString toDisplayString() const;
};

/**
 * @brief .lex 文件读取器。
 *
 * 调用注意：
 *  1. 读取前必须先 `setGrammar()`，因为"编码 → 符号名"的翻译要用到
 *     `Grammar::terminalNames()`（由 `LanguageConfigRegistry::terminalNames()`
 *     注入）。没有注入编码表时，符号名一律留空并记录到 warnings()，
 *     分析会因"输入符号不在文法终结符集内"而无法进行 —— 这是需要报给
 *     使用者的真实问题，不是可以静默兜底的；
 *  2. 输入文法既可以是未增广的（用户写的），也可以是增广后的。`$` 作为
 *     输入结束标记由本类**自动追加**，不需要 .lex 文件里写；
 *  3. `EOF` 行会被识别并丢弃（词法程序的正常结束标志，不是文法符号）。
 */
class LexReader
{
public:
    LexReader() = default;

    /** @brief 指定文法（用于把编码翻译成文法符号名） */
    void setGrammar(const Grammar &grammar);

    /**
     * @brief 解析 .lex 文本。
     * @param text 文件内容。
     * @return 无致命错误时为 true。
     * @note 无法翻译编码的行会进 warnings() 并**跳过**，不影响其余行。
     */
    bool parse(const QString &text);

    /**
     * @brief 从文件读取并解析。
     * @param path  文件路径。
     * @param error 输出：失败原因。
     * @return 成功为 true。
     */
    bool loadFromFile(const QString &path, QString &error);

    /** @brief 读出的全部记号（不含自动追加的 `$`） */
    const QVector<LexItem> &items() const { return m_items; }

    /** @brief 语法分析用的符号序列（末尾已追加 `$`） */
    QStringList symbols() const;

    /** @brief 与 symbols() 一一对应的源程序行号（`$` 用最后一行） */
    QVector<int> symbolLines() const;

    /** @brief 与 symbols() 一一对应的单词值（`$` 为空；终结符值为原始文本） */
    QStringList symbolValues() const;

    /** @brief 警告列表（无法翻译的编码、格式异常行等） */
    const QStringList &warnings() const { return m_warnings; }

    /** @brief 无法翻译的编码集合（去重后，便于一次性报给用户） */
    QList<int> unknownCodes() const;

    /** @brief 源程序总行数（取所有记号行号的最大值） */
    int sourceLineCount() const;

    /** @brief 一句话摘要 */
    QString summary() const;

    /** @brief 清空 */
    void clear();

    /** @brief 输入结束标记（与 FirstFollow 的 kEndMarker 保持一致） */
    static QString endMarker();

private:
    /**
     * @brief 解析一行 .lex。
     * @param line 已 trim 的行。
     * @param lineNo 行号（.lex 文件内的行号，仅用于报错）。
     * @return 成功为 true。
     */
    bool parseLine(const QString &line, int lineNo);

    Grammar m_grammar;      ///< 用于翻译的文法
    QVector<LexItem> m_items;  ///< 读出的记号
    QStringList m_warnings;    ///< 警告
    bool m_hasGrammar = false; ///< 是否已注入文法
};

}  // namespace ccd
