/******************************************************************************
 * (1) 版权信息 : 编译原理课程设计 - 华南师范大学计算机学院
 * (2) 文件名称 : RegexAst.h
 *     标识符   : RegexAst / RegexParser / TokenSpec
 *     摘要     : 第一项任务的前端：把"单词规格文件"中的正则表达式解析为抽象
 *                语法树，并提供：
 *                  （1）逆波兰式（后缀式）输出 —— 供 Thompson 构造使用；
 *                  （2）字符集展开、取反（~）、空串（#）等辅助运算。
 *                支持的运算符号（任务书要求 1.2）：
 *                  转义 \ 、连接（隐式并置）、选择 | 、闭包 * 、正闭包 + 、
 *                  字符集 [ ] 、可选 ? 、括号 ( ) 、非 ~ 、空串 #
 *                本文件只依赖 QtCore，不含任何 GUI 代码。
 * (3) 版本号   : 1.0.0       作者/修改者 : <学号 姓名>   完成日期 : 2026-XX-XX
 * (4) 版本历史 : 1.0.0  2026-XX-XX  首次实现
 *****************************************************************************/

#pragma once

#include <QMap>
#include <QSet>
#include <QString>
#include <QStringList>

namespace ccd {

/** 正则式抽象语法树的结点类型 */
enum class RegexNodeKind {
    Empty,      ///< 空串 ε（源文本中的 #）
    Charset,    ///< 单个字符或字符集（可用 negate 表示取反后的集合）
    Concat,     ///< 连接（childAt(0) 与 childAt(1) 的连接）
    Alternate,  ///< 选择 |
    Star,       ///< 闭包 *（正闭包 + 与可选 ? 会先展开为该形式或其变体）
    Plus,       ///< 正闭包 +（至少出现一次）
    Optional,   ///< 可选 ?（出现零次或一次）
    Epsilon     ///< 由 + 或 ? 展开时引入的显式 ε 分支
};

/**
 * @brief 正则式抽象语法树结点（值语义，使用下标管理子结点，不用裸指针）。
 *
 * 存储结构：
 *  - kind     : 结点类型
 *  - charset  : 字符集合（kind == Charset 时有效），每个元素是单字符
 *  - negate   : 该字符集是否取反（~ 运算的落地形式）
 *  - children : 子结点下标列表
 *  - text     : 原始文本片段，仅供报错与调试显示
 *  - atomName : 【字符集原子名】该字符集在正则式里的"写法"：宏引用记宏名
 *               （letter）、方括号字符集记规范文本（[A-Za-z]）、转义单字符
 *               记该字符（+）、取反项加 ~ 前缀（~ \}）。字符类字母表就是
 *               用它给自动机的输入符号命名的（见 lex/CharClass.h）。
 *               非 Charset 结点恒为空。
 *  - atomFromMacro : atomName 是否取自宏定义名。命名时宏名优先级最高，
 *              这样写了 letter=[A-Za-z] 之后图上显示的是 letter 而不是 [A-Za-z]。
 */
struct RegexNode {
    RegexNodeKind kind = RegexNodeKind::Empty;  ///< 结点类型
    QStringList charset;                        ///< 字符集（每个元素为单字符）
    bool negate = false;                        ///< 字符集是否取反
    QVector<int> children;                      ///< 子结点下标
    QString text;                               ///< 原始文本（调试用）
    QString atomName;                           ///< 字符集原子名（显示用）
    bool atomFromMacro = false;                 ///< atomName 是否来自宏定义名
};

/**
 * @brief 正则式抽象语法树：结点数组 + 根下标。
 *
 * 调用注意：
 *  1. 结点以数组形式集中存放，下标即结点编号，避免裸指针与内存泄漏；
 *  2. negateCharset() 返回"取反后"的字符集，需要全局字符全集，见下文字符全集说明。
 */
class RegexAst
{
public:
    /** 字符全集中"可打印字符"部分的下界与上界（ASCII 32..126） */
    static const int kPrintableFirst = 32;
    static const int kPrintableLast = 126;

    /**
     * @brief 取反运算所依据的"字符全集"。
     * @return 全部 256 个字节值（0x00~0xFF）各作为一个元素的列表。
     * @note 必须覆盖 0x80~0xFF：UTF-8 汉字的每个字符由 3 个高位字节组成，
     *       注释正则 \{ (~ \})* \} 依赖这个全集才能跨过中文继续前进。
     *       公开为静态函数便于单元测试直接校验全长。
     */
    static QStringList unicodeUniverse();

    /** @brief 新建一个结点，返回其下标 */
    int addNode(RegexNodeKind kind, const QString &text = QString());

    /** @brief 取结点（可修改） */
    RegexNode &node(int index) { return m_nodes[index]; }
    /** @brief 取结点（只读） */
    const RegexNode &node(int index) const { return m_nodes[index]; }

    /** @brief 根结点下标（-1 表示空树） */
    int root() const { return m_root; }
    /** @brief 设置根结点下标 */
    void setRoot(int index) { m_root = index; }

    /** @brief 结点总数 */
    int size() const { return m_nodes.size(); }
    /** @brief 是否为空树 */
    bool isEmpty() const { return m_root < 0; }

    /**
     * @brief 深拷贝一棵子树到本树中。
     * @param other 源树。
     * @param index 源树中的结点下标。
     * @return 本树中新建的对应结点下标。
     * @note 用于宏（命名）展开：把宏定义的正则式子树复制到引用位置。
     */
    int cloneSubtree(const RegexAst &other, int index);

    /**
     * @brief 把字符集结点解析为最终的字符列表。
     * @param index 结点下标。
     * @return 该结点最终匹配的所有单字符；取反时返回"可打印 ASCII 全集减原集合"。
     * @note 取反的范围限定为可打印 ASCII（32..126）加上制表符，这样 ~ 的结果
     *       不会包含换行符，符合 // 行注释与 { } 注释的实际需求，
     *       也使 NFA 的字母表保持在可观察的规模内。
     */
    QStringList resolveCharset(int index) const;

    /**
     * @brief 生成逆波兰式（后缀表达式）字符串。
     * @return 以空格分隔的后缀表达式，运算符使用 '|'（选择）、'*'（闭包）、
     *         '+'（正闭包）、'?'（可选）、'.'（连接）、以及以 \x 转义的字符；
     *         空串输出为 '#'。
     * @note 每个字符都以 "\x" 形式写出（x 为字符本身），避免与运算符冲突，
     *       空格等空白字符则写成转义序列。
     */
    QString toPostfix() const;

    /** @brief 生成便于调试的中缀形式文本 */
    QString toInfix() const;

    /**
     * @brief 把单个字符转换为后缀式中的记号。
     * @param ch 单字符文本。
     * @return 可打印字符为 "\c"；反斜杠为 "\\"；不可打印字符为 "\xNN"；空串为 "#"。
     * @note 公开为静态函数的原因：RegexToNfa 展开字符集**必须**复用同一套编码，
     *       否则不可打印字符（制表/换行/回车）会编码成 2 字符记号，
     *       破坏"按空格切分"的记号流（本项目实际踩过这个坑）。
     *       解码端见 RegexToNfa::decodeToken()。
     */
    static QString encodeChar(const QString &ch);

private:
    /** @brief 递归生成后缀表达式 */
    void buildPostfix(int index, QStringList &out) const;
    /** @brief 递归生成中缀文本 */
    QString buildInfix(int index) const;

    QVector<RegexNode> m_nodes;  ///< 全部结点
    int m_root = -1;             ///< 根结点下标
};

/**
 * @brief 正则式解析器：表达式文本 → RegexAst。
 *
 * 支持的语法（运算符优先级由低到高）：
 *   选择 |   →   连接（隐式并置）   →   后缀 * + ?   →   括号/字符集/非 ~/转义/字符
 *
 * 调用注意：
 *  1. 宏（命名）通过 setMacros() 注入；解析到未知标识符时作为"字面量字符序列"
 *     处理（例如正则式里直接写数字或字母）；
 *  2. parse() 失败时 error() 给出带列号的中文错误信息；
 *  3. 预定义特殊命名 eoln（行结束符）由本类内置。
 */
class RegexParser
{
public:
    RegexParser();

    /** @brief 设置宏表（例如 digit → [0-9]" 的已解析子树） */
    void setMacros(const QMap<QString, RegexAst> &macros) { m_macros = macros; }
    /** @brief 取宏表 */
    const QMap<QString, RegexAst> &macros() const { return m_macros; }

    /**
     * @brief 解析一条正则表达式。
     * @param pattern 表达式文本（不含 "名字=" 前缀）。
     * @param out     输出语法树。
     * @return 成功为 true；失败时 error() 给出原因。
     */
    bool parse(const QString &pattern, RegexAst &out);

    /** @brief 最近一次错误信息 */
    QString error() const { return m_error; }

    /**
     * @brief 【调试辅助】把表达式切分为记号序列并返回可读文本。
     * @param pattern 表达式文本。
     * @return 形如 "LBracket[0] Literal[1:'0'] ..." 的文本；失败时返回错误信息。
     * @note 仅供单元测试与问题排查使用，不参与算法逻辑。
     */
    static QString debugTokens(const QString &pattern);

    /** 记号类型（公开以便 .cpp 中的自由函数 endsAtom() 使用） */
    enum class TokKind {
        End,        ///< 表达式结束
        Literal,    ///< 普通字符（单字符）
        Escape,     ///< 转义字符 \x（单字符，已去转义）
        Identifier, ///< 标识符（可能是宏名，也可能是普通字母串）
        Concat,     ///< 显式连接运算符 .（也用于内部插入）
        Alternate,  ///< |
        Star,       ///< *
        Plus,       ///< +
        Optional,   ///< ?
        LPar,       ///< (
        RPar,       ///< )
        LBracket,   ///< [
        RBracket,   ///< ]
        Tilde,      ///< ~
        Empty       ///< #（空串）
    };

private:
    /** 一个记号 */
    struct Token {
        TokKind kind = TokKind::End;  ///< 类型
        QString text;                 ///< 文本（Literal/Escape/Identifier 有效）
        int pos = 0;                  ///< 在源文本中的位置（报错用）
    };

    /** @brief 把表达式文本切分为记号序列（同时插入显式连接记号 '.'） */
    bool tokenize(const QString &pattern, QVector<Token> &tokens);

    // ---- 递归下降语法分析 ----
    /** @brief 选择：concat ('|' concat)* */
    bool parseAlternate(RegexAst &ast, int &result);
    /** @brief 连接：repeat+ */
    bool parseConcat(RegexAst &ast, int &result);
    /** @brief 重复：atom ('*' | '+' | '?')* */
    bool parseRepeat(RegexAst &ast, int &result);
    /** @brief 基本项：'(' alt ')' | '[' set ']' | '~' atom | 字符 | 宏引用 | '#' */
    bool parseAtom(RegexAst &ast, int &result);

    /** @brief 取当前记号 */
    const Token &cur() const { return m_tokens[m_index]; }
    /** @brief 前进一个记号 */
    void advance() { ++m_index; }
    /** @brief 是否已到末尾 */
    bool atEnd() const { return cur().kind == TokKind::End; }
    /** @brief 记录错误（带位置） */
    bool fail(const QString &message, int pos);

    /** @brief 判断当前记号能否作为"连接"的右操作数（用于插入显式连接符） */
    static bool startsAtom(TokKind kind);

    QVector<Token> m_tokens;                 ///< 记号序列
    int m_index = 0;                         ///< 当前记号下标
    QMap<QString, RegexAst> m_macros;        ///< 宏表
    QMap<QString, QStringList> m_builtinSets;///< 内置字符集（eoln 等）
    QString m_error;                         ///< 错误信息
};

}  // namespace ccd
