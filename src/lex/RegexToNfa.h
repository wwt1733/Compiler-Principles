/******************************************************************************
 * (1) 版权信息 : 编译原理课程设计 - 华南师范大学计算机学院
 * (2) 文件名称 : RegexToNfa.h
 *     标识符   : RegexToNfa
 *     摘要     : 正则表达式 → NFA 的构造（Thompson 构造法）。算法与讲稿中的
 *                范例完全同构：把正则式转为逆波兰式后用**状态号栈**扫描，遇
 *                到连接/选择/闭包运算符时弹出若干状态号、插入新结点并压回，
 *                遇到操作数时新建两个结点与一条标号边。
 *                多个单词定义之间以"选择"（或）方式合并为一个 NFA，接受状态
 *                携带 {定义名, 单词编码, 是否注释} 标签。
 *                本文件只依赖 QtCore，不含任何 GUI 代码。
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
 * @brief 自动机字母表采用哪种粒度。
 *
 * - CharClass : 按**字符类**（默认，生产路径）。正则式里书写的每个字符集是一个
 *               输入符号，字母表约 20 个，状态转换表与 DOT 图才可读；
 * - Byte      : 按**字节**（经典做法，本系统保留作回归对照）。每个字节值各是
 *               一个输入符号，字母表最大 256 个。
 *
 * 两条路径构造出的自动机应当**语言等价**，自测里用 T20 逐字节比对两个最小 DFA
 * 来证明这一点。
 */
enum class AlphabetMode {
    CharClass,  ///< 按字符类（默认）
    Byte        ///< 按字节（对照用）
};

/**
 * @brief 一条待构造自动机的规则：定义名 + 编译好的正则语法树 + 接受标签。
 *
 * 调用注意：接受标签 code 为 -1 表示该规则是注释（识别后不输出编码）。
 */
struct NfaRule {
    QString name;   ///< 定义名（用于调试与标签显示）
    int code = -1;  ///< 单词编码（注释为 -1）
    bool isComment = false;  ///< 是否注释类
    RegexAst ast;   ///< 该规则的正则语法树
};

/**
 * @brief 正则式 → NFA 构造器。
 *
 * 调用注意：
 *  1. build() 会先为所有规则建一个统一的新开始状态，再用 ε 边连到各规则的
 *     开始状态，因此得到的 NFA 是"多规则并集"；
 *  2. 单个字符集（如 [0-9]）会展开为若干条并列的字符边，字母表由实际出现的
 *     字符决定；
 *  3. 构造过程中的规模有上限保护（见 kMaxStates），超过时报错而不崩溃。
 */
class RegexToNfa
{
public:
    /** 状态数上限：超过即认为输入异常，避免内存失控 */
    static const int kMaxStates = 20000;

    RegexToNfa() = default;

    /**
     * @brief 由多条规则构造合并后的 NFA。
     * @param rules 规则列表（顺序即声明顺序，影响接受状态标签的排列）。
     * @param out   输出 NFA（含 alphabetInfo：字符类字母表）。
     * @param mode  字母表粒度；默认按字符类。
     * @return 成功为 true；失败时 error() 给出原因。
     * @note 按字符类构造时，一个字符集只产生**一个**操作数与 2 个状态（多条
     *       平行标号边），因此 letter=[A-Za-z] 只占 2 个状态而不是 106 个。
     */
    bool build(const QVector<NfaRule> &rules, Nfa &out,
               AlphabetMode mode = AlphabetMode::CharClass);

    /**
     * @brief 由单条正则式构造 NFA（单元测试与演示用）。
     * @param pattern 正则表达式文本（不含 "名字=" 前缀）。
     * @param out     输出 NFA。
     * @return 成功为 true。
     * @note 接受状态不带任何编码标签。
     */
    bool buildFromPattern(const QString &pattern, Nfa &out);

    /** @brief 最近一次错误信息 */
    QString error() const { return m_error; }

    /** @brief 最近一次构造的中间结果：各规则的逆波兰式（界面展示用） */
    const QMap<QString, QString> &postfixMap() const { return m_postfixMap; }

    /**
     * @brief 内部：把逆波兰式记号还原为它代表的单个字符。
     * @param token 形如 "\c"、"\xNN"、"\\" 的记号。
     * @return 单字符文本；无法识别时返回空串。
     * @note 必须与 RegexAst::encodeChar() 的编码规则严格对应。
     *       不能只"去掉反斜杠取剩余部分"，否则不可打印字符的 "\x09"
     *       会变成 3 字符的"符号"，自动机永远匹配不上。
     *       公开为静态函数是为了让单元测试能直接检验词法行为。
     */
    static QString decodeToken(const QString &token);

    /**
     * @brief 把一棵正则语法树展开为逆波兰记号序列。
     * @param ast     语法树。
     * @param index   子树根下标。
     * @param out     输出记号序列。
     * @param symbols 字符类字母表；为空表示按字节模式（每个字节各发一个记号）。
     * @note 与 RegexAst::toPostfix() 输出一致，但直接产出记号，避免字符串往返。
     *       公开为静态函数是为了让单元测试能直接检验记号流是否合法
     *       （按字节模式下每个输入记号必须能还原为**单个字符**）。
     *       按字符类模式时，一个字符集只发**一个**记号 `\C:键1,键2,…`，
     *       由 buildFragment() 展开成一对状态上的多条平行边。
     */
    static void expandPostfix(const RegexAst &ast, int index, QStringList &out,
                              const CharClassAlphabet *symbols = nullptr);

private:
    /**
     * @brief 用状态号栈把逆波兰记号序列构造为 NFA 片段。
     * @param tokens   逆波兰记号序列。
     * @param nfa      目标 NFA（状态与边会被追加）。
     * @param startOut 输出：该片段的开始状态号。
     * @param acceptOut 输出：该片段的接受状态号。
     * @return 成功为 true。
     * @note 本函数即讲稿范例中"处理连接符/选择符/闭包符"三段逻辑的落地。
     */
    bool buildFragment(const QStringList &tokens, Nfa &nfa, int &startOut, int &acceptOut);

    /** @brief 插入一个全新状态，返回其编号 */
    int newState(Nfa &nfa);
    /** @brief 插入一条标号边 */
    void addEdge(Nfa &nfa, int from, int to, const QString &symbol, bool isEpsilon);
    /** @brief 记录错误 */
    bool fail(const QString &message);

    QString m_error;                        ///< 错误信息
    QMap<QString, QString> m_postfixMap;    ///< 定义名 → 逆波兰式（展示用）
};

}  // namespace ccd
