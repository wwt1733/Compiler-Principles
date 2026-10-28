/******************************************************************************
 * (1) 版权信息 : 编译原理课程设计 - 华南师范大学计算机学院
 * (2) 文件名称 : Grammar.h
 *     标识符   : Grammar
 *     摘要     : BNF 文法的载入与管理（第二项任务 2.1）。
 *                把任务书 p.3 约定的 BNF 文本（`左部 -> 右部1 | 右部2`，
 *                空串用 `#` 表示）解析为产生式表、非终结符集与终结符集，
 *                并提供增广文法、按左部索引等供后续算法使用的操作。
 *                本文件只依赖 QtCore，不含任何界面代码，可脱离 GUI 单元测试。
 * (3) 版本号   : 1.0.0       作者/修改者 : <学号 姓名>   完成日期 : 2026-XX-XX
 * (4) 版本历史 : 1.0.0  2026-XX-XX  首次实现（阶段 4）
 *****************************************************************************/

#pragma once

#include <QMap>
#include <QString>
#include <QStringList>
#include <QVector>

#include "common/Types.h"

namespace ccd {

/**
 * @brief 上下文无关文法（BNF）。
 *
 * 调用注意：
 *  1. 输入格式：一行一条产生式，`左部 -> 右部`，右部用 `|` 分隔候选式；
 *     以 `#` 开头的行是注释，空行忽略；空串必须显式写成 `#`
 *     （既支持 `A -> #`，也支持 `A -> B | #`）；
 *  2. **第一个非终结符即开始符号**（任务书约定），不是第一条产生式的左部
 *     —— 两者通常相同，但 `program -> stmt-sequence` 这类写法下必须以前者为准；
 *  3. 载入时自动增广 `S' -> S`（若用户已写增广产生式则不重复追加）；
 *  4. 终结符 = 出现在右部、且从未作为任何产生式左部的符号，按**首次出现顺序**排列；
 *     非终结符 = 所有左部符号，按**首次出现顺序**排列。
 */
class Grammar
{
public:
    /** 增广文法的开始符号名 */
    static const QString kAugmentedStart;

    Grammar() = default;

    // ---- 载入 ----

    /**
     * @brief 从文本载入文法。
     * @param text  BNF 文本。
     * @param error 输出：失败原因（成功时清空）。
     * @return 成功为 true。
     * @note 载入前会清空原有内容；失败时对象可能处于部分载入状态，
     *       调用方应据返回值决定是否使用。
     */
    bool loadFromText(const QString &text, QString &error);

    /**
     * @brief 从文件载入文法（自动跳过 UTF-8 BOM、统一换行）。
     * @param path  文件路径。
     * @param error 输出：失败原因。
     * @return 成功为 true。
     */
    bool loadFromFile(const QString &path, QString &error);

    // ---- 查询 ----

    /** @brief 全部产生式 */
    const QVector<Production> &productions() const { return m_productions; }
    /** @brief 产生式条数 */
    int productionCount() const { return m_productions.size(); }
    /** @brief 取第 index 条产生式（越界返回空产生式） */
    const Production &production(int index) const;

    /** @brief 非终结符列表（有序） */
    const QStringList &nonterminals() const { return m_nonterminals; }
    /** @brief 终结符列表（有序） */
    const QStringList &terminals() const { return m_terminals; }
    /** @brief 开始符号 */
    QString startSymbol() const { return m_startSymbol; }
    /** @brief 增广开始符号（载入后为 kAugmentedStart） */
    QString augmentedStart() const { return m_augmentedStart; }

    /** @brief 判断是否非终结符 */
    bool isNonterminal(const QString &symbol) const;
    /** @brief 判断是否终结符 */
    bool isTerminal(const QString &symbol) const;
    /** @brief 判断是否为文法符号（终结符或非终结符） */
    bool isSymbol(const QString &symbol) const;

    /** @brief 按左部取产生式序号列表（有序） */
    QVector<int> productionsOf(const QString &lhs) const;

    /**
     * @brief 取某终结符对应的单词编码。
     * @param symbol 终结符名。
     * @return 编码；未登记时返回 -1。
     * @note 载入后可用 setTerminalNames() 注入"编码 → 终结符名"表，
     *       语法分析器据此把 `.lex` 里的数字编码翻译成终结符名。
     */
    int terminalCode(const QString &symbol) const;
    /** @brief 取"终结符名 → 编码"表（setTerminalNames() 的逆表，惰性构建并缓存） */
    const QMap<QString, int> &terminalCodes() const;

    /**
     * @brief 注入"单词编码 → 终结符名"表。
     * @param names 编码到终结符名的映射，直接传
     *              `LanguageConfigRegistry::terminalNames(id)` 即可。
     * @note 参数方向刻意与 terminalNames() 保持一致（编码在前），
     *       免得调用方还要手工翻转一次映射；逆表由本对象按需构建并缓存。
     */
    void setTerminalNames(const QMap<int, QString> &names);

    /** @brief 取"编码 → 终结符名"表（原样保存，供解析器翻译 .lex 编码） */
    const QMap<int, QString> &terminalNames() const { return m_terminalNames; }

    /** @brief 取某终结符的编码；与 terminalCode() 同义，供界面显示 */
    QString symbolWithCode(const QString &symbol) const;

    // ---- 变换 ----

    /**
     * @brief 建立增广文法：追加 `S' -> S`。
     * @param prime 增广符号名，默认 kAugmentedStart（`S'`）。
     * @note 已有增广产生式时不重复追加；`S'` 会被加入非终结符列表首位。
     */
    void augment(const QString &prime = kAugmentedStart);

    /** @brief 是否已增广 */
    bool augmented() const { return m_augmented; }

    /** @brief 用一个文法整体替换当前内容（供化简结果回写） */
    void assignFrom(const Grammar &other);

    /** @brief 清空全部内容 */
    void clear();

    /** @brief 文法全文（每条产生式一行，供界面显示与导出） */
    QString toText() const;

    /** @brief 一句话摘要（产生式数、终结符数、非终结符数） */
    QString summary() const;

private:
    /** @brief 重建非终结符/终结符列表（载入与化简后都要调用） */
    void rebuildSymbolLists();

    QVector<Production> m_productions;   ///< 产生式表
    QStringList m_nonterminals;          ///< 非终结符（首次出现顺序）
    QStringList m_terminals;             ///< 终结符（首次出现顺序）
    QString m_startSymbol;               ///< 开始符号
    QString m_augmentedStart;            ///< 增广开始符号（未增广时为空）
    bool m_augmented = false;            ///< 是否已增广
    QMap<int, QString> m_terminalNames;    ///< 编码 → 终结符名（外部注入）
    // 逆表与其构建标志声明为 mutable：terminalCode() 是 const 查询接口，
    // 但需要按需构建一次逆表（惰性缓存），这属于"逻辑上不改变对象状态"的
    // 内部缓存，用 mutable 表达正是 C++ 的惯用做法。
    mutable QMap<QString, int> m_terminalCodes;  ///< 终结符名 → 编码（逆表，惰性构建）
    mutable bool m_codesBuilt = false;           ///< 逆表是否已构建
};

}  // namespace ccd
