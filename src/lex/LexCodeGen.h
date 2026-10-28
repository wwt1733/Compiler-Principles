/******************************************************************************
 * (1) 版权信息 : 编译原理课程设计 - 华南师范大学计算机学院
 * (2) 文件名称 : LexCodeGen.h
 *     标识符   : LexCodeGen
 *     摘要     : 由最小化 DFA 生成 C/C++ 词法分析源程序（任务书要求 1.7）。
 *                支持两种转换方法（任务书 p.6~p.7 的"转换方法一/方法二"）：
 *                  方法一 Judgechar/GetNext/GetToken：
 *                    用 nextState() 函数按 (状态, 字符) 查表推进状态，
 *                    以 switch 分派"当前状态该做什么"；
 *                  方法二 switch(state)：
 *                    直接生成 switch (state) { case N: switch(ch) {...} }，
 *                    转移关系全部编码进 switch 分支，不查表。
 *                生成的程序包含：最长匹配（maximal munch）与回退、
 *                关键字查表改判、注释跳过（不输出编码）、
 *                以及按 AGENT.md 4.3 节格式输出 .lex 文件。
 *                本文件只依赖 QtCore。
 * (3) 版本号   : 1.0.0       作者/修改者 : <学号 姓名>   完成日期 : 2026-XX-XX
 * (4) 版本历史 : 1.0.0  2026-XX-XX  首次实现
 *****************************************************************************/

#pragma once

#include <QMap>
#include <QString>
#include <QStringList>

#include "common/Types.h"

namespace ccd {

/**
 * @brief 词法分析源程序生成器。
 *
 * 调用注意：
 *  1. 输入必须是**最小化后**的 DFA（任务书要求 1.7 明确"由最小化 DFA 生成"）；
 *  2. 除 DFA 外还需要"字面单词 → 编码"表（关键字与专用符号）：关键字表用于
 *     识别出标识符后改判，符号表用于普通单词的收尾判定；
 *  3. 生成结果是一份完整的、可被 g++ 独立编译的 C++ 源文件，
 *     运行时从命令行第 1 个参数读入源程序、第 2 个参数写出 .lex 文件。
 */
class LexCodeGen
{
public:
    /** 转换方法（任务书 p.6~p.7） */
    enum class Method {
        JudgeChar,  ///< 方法一：Judgechar / GetNext / GetToken 三函数式
        SwitchState ///< 方法二：switch(state) 内嵌 switch(ch)
    };

    /** 输出排版 */
    enum class Layout {
        OnePerLine,  ///< 每行一个单词（默认，便于第二项任务逐行分析）
        TaskbookSample  ///< 任务书 p.3 示例排版：全部单词拼成一行
    };

    LexCodeGen() = default;

    /**
     * @brief 生成词法分析源程序的完整文本。
     * @param dfa        最小化 DFA。
     * @param literals   字面单词表（关键字 + 专用符号）→ 编码。
     * @param keywords   关键字表（单词 → 编码），用于 ID 的查表改判。
     * @param idCode     ID 的编码（用于判断"是否需要查关键字表"）。
     * @param method     转换方法。
     * @param layout     输出排版。
     * @param programName 源程序语言名（写进注释，如 "TINY"）。
     * @return 生成的 C++ 源程序文本；失败时返回空串并设置 error()。
     */
    QString generate(const Dfa &dfa, const QMap<QString, int> &literals,
                     const QMap<QString, int> &keywords, int idCode, Method method,
                     Layout layout, const QString &programName);

    /** @brief 最近一次错误信息 */
    QString error() const { return m_error; }

    /**
     * @brief 生成"程序使用说明"文本（供界面⑤显示）。
     * @param dfa 已生成的 DFA（用于统计信息）。
     * @return 说明文本。
     */
    static QString usageNotes(const Dfa &dfa);

private:
    /** @brief 生成方法一（Judgechar/GetNext/GetToken）的状态分派代码 */
    QString emitMethodOne(const Dfa &dfa, int idCode) const;

    /** @brief 生成方法二（switch(state)）的状态分派代码 */
    QString emitMethodTwo(const Dfa &dfa, int idCode) const;

    /** @brief 生成"字面单词常量表"代码 */
    QString emitLiteralTables(const QMap<QString, int> &literals,
                              const QMap<QString, int> &keywords) const;

    QString m_error;  ///< 错误信息
};

}  // namespace ccd
