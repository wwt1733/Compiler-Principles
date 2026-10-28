/******************************************************************************
 * (1) 版权信息 : 编译原理课程设计 - 华南师范大学计算机学院
 * (2) 文件名称 : TokenSpec.h
 *     标识符   : TokenSpec
 *     摘要     : 单词规格文件（"某一高级程序设计语言的所有单词对应的正则
 *                表达式"文本文件）的解析器。把文件解析为一组 RegexDef，
 *                并完成三件关键工作：
 *                  （1）识别命名中的下划线 + 数字编码，以及后缀 L（Launch）；
 *                  （2）区分宏定义、普通单词、注释、关键字四类；
 *                  （3）把宏定义（如 digit=[0-9]）预解析为语法树，供后续
 *                       正则式做命名展开。
 *                文件格式见 AGENT.md 4.1 节与 data/ 下各 words 文件的头部说明。
 *                本文件只依赖 QtCore。
 * (3) 版本号   : 1.0.0       作者/修改者 : <学号 姓名>   完成日期 : 2026-XX-XX
 * (4) 版本历史 : 1.0.0  2026-XX-XX  首次实现
 *****************************************************************************/

#pragma once

#include <QMap>
#include <QString>
#include <QStringList>
#include <QVector>

#include "common/Types.h"
#include "lex/RegexAst.h"

namespace ccd {

/**
 * @brief 单词规格文件解析器。
 *
 * 调用注意：
 *  1. 使用流程为 构造函数 → parse() → 读取 defs()/macros()/errors()；
 *  2. parse() 不会抛异常；遇到单行错误时记录到 errors() 并跳过该行，
 *     以便一次把所有格式问题都报给用户；
 *  3. 宏定义必须在被引用之前出现（自顶向下单遍解析），这与任务书样例文件的
 *     书写顺序一致（digit、letter 在最前面）。
 */
class TokenSpec
{
public:
    TokenSpec() = default;

    /**
     * @brief 解析规格文件全文。
     * @param text 文件内容（任意换行风格，内部会归一化）。
     * @return 全部行都没有致命错误时为 true；只要存在错误行即为 false，
     *         但已成功解析的定义仍然保留（便于界面展示部分结果）。
     */
    bool parse(const QString &text);

    /** @brief 全部单词规格定义（按文件中出现顺序，含关键字/注释/宏） */
    const QVector<RegexDef> &defs() const { return m_defs; }

    /** @brief 宏表：名字 → 已解析的正则语法树 */
    const QMap<QString, RegexAst> &macros() const { return m_macros; }

    /** @brief 解析过程中的错误信息列表（每行一条） */
    const QStringList &errors() const { return m_errors; }

    /** @brief 需要构造自动机的定义（普通单词 + 注释），按声明顺序 */
    QVector<RegexDef> automatonDefs() const;

    /** @brief 关键字定义（不参与自动机，只登记编码） */
    QVector<RegexDef> keywordDefs() const;

    /**
     * @brief 取"字面单词 → 编码"的常量表（供生成的词法程序做查表判定）。
     * @return 有序映射；键为字面单词（不区分大小写的语言已统一为小写），值为编码。
     * @note 包含关键字与专用符号。生成词法程序时：
     *        1. 关键字表用于"识别出标识符后改判为关键字"；
     *        2. 符号表用于"识别出运算符后查出其编码"。
     */
    QMap<QString, int> literalCodeTable() const;

    /**
     * @brief 取关键字表（单词 → 编码）。
     * @return 有序映射；键已按语言约定处理大小写。
     */
    QMap<QString, int> keywordTable() const;

    /**
     * @brief 取专用符号表（符号原文 → 编码）。
     * @return 有序映射。
     */
    QMap<QString, int> symbolTable() const;

    /**
     * @brief 生成便于界面显示的规格汇总表。
     * @param headers 输出：列标题。
     * @param rows    输出：每行文本。
     * @note 列依次为 序号、名称、编码范围、类别、正则式（原始）、逆波兰式。
     */
    void buildSummaryTable(QStringList &headers, QList<QStringList> &rows) const;

private:
    /**
     * @brief 解析一条定义行。
     * @param line 已去除首尾空白的定义行。
     * @param order 该行在文件中的序号。
     * @return 成功为 true；失败时把原因追加到 m_errors。
     * @note 行格式为 "名字=正则式"；名字中可含下划线加数字编码，编码后可带 L。
     */
    bool parseDefinitionLine(const QString &line, int order);

    QVector<RegexDef> m_defs;              ///< 全部定义
    QMap<QString, RegexAst> m_macros;      ///< 宏表
    QStringList m_errors;                  ///< 错误列表
};

}  // namespace ccd
