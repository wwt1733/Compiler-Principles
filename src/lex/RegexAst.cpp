/******************************************************************************
 * (1) 版权信息 : 编译原理课程设计 - 华南师范大学计算机学院
 * (2) 文件名称 : RegexAst.cpp
 *     标识符   : RegexAst / RegexParser
 *     摘要     : 正则式抽象语法树与解析器的实现。包含：
 *                  （1）RegexAst：结点增删、子树克隆（宏展开）、字符集求值
 *                      （含 ~ 取反）、逆波兰式与中缀式输出；
 *                  （2）RegexParser：词法分析（含隐式连接符插入）、递归下降
 *                      语法分析、正闭包 + 与可选 ? 的等价改写、内置行结束符
 *                      命名 eoln。
 * (3) 版本号   : 1.0.0       作者/修改者 : <学号 姓名>   完成日期 : 2026-XX-XX
 * (4) 版本历史 : 1.0.0  2026-XX-XX  首次实现
 *****************************************************************************/

#include "lex/RegexAst.h"

#include <QRegularExpression>

#include "lex/CharClass.h"

namespace ccd {

/**
 * 功能：构造"取反"运算所使用的字符全集。
 * 输入：无。
 * 输出：全部 256 个字节值（0x00~0xFF）各作为一个"字符"。
 * 注意：
 *  1. 这里按**字节**而非 Unicode 码位定义全集。原因是生成的词法分析程序
 *     按字节（unsigned char）驱动 DFA，这样 ~ 取反所得的字符集与运行时
 *     真正能遇到的输入单位一一对应；
 *  2. 必须包含 0x80~0xFF：UTF-8 编码的汉字每个字符由 3 个高位字节组成，
 *     若全集只到 0x7E，注释 \{ (~ \})* \} 就会在遇到汉字的第一个字节时
 *     无法继续前进，导致"注释里有中文就断掉"（本项目实际踩过这个坑）；
 *  3. 换行符 0x0A 必须在全集内，这样 ~eoln 才能正确地"排除行结束符"；
 *  4. 代价是字母表由 ~97 增至 256 个符号，DFA 转移表相应变大 —— 对课程
 *     设计的源程序规模（生成代码数百行）完全可以接受。
 */
QStringList RegexAst::unicodeUniverse()
{
    QStringList universe;
    universe.reserve(256);
    for (int code = 0; code <= 0xFF; ++code)
        universe.append(QString(QChar(static_cast<ushort>(code))));
    return universe;
}

namespace {

}  // namespace

// ===========================================================================
//  RegexAst
// ===========================================================================

/**
 * 功能：新建一个语法树结点。
 * 输入：kind —— 结点类型；text —— 原始文本（调试显示用）。
 * 输出：新结点下标。
 * 注意：结点集中存放于 m_nodes，下标即编号，不使用裸指针。
 */
int RegexAst::addNode(RegexNodeKind kind, const QString &text)
{
    RegexNode node;
    node.kind = kind;
    node.text = text;
    m_nodes.append(node);
    return m_nodes.size() - 1;
}

/**
 * 功能：把另一棵树中的子树深拷贝到本树。
 * 输入：other —— 源树；index —— 源树中的结点下标。
 * 输出：本树中新建的对应结点下标。
 * 注意：用于宏展开。子结点下标需要在拷贝过程中重映射，因此采用
 *       "先递归拷贝子结点、再建本结点"的后序方式。
 *       字符集原子名（atomName / atomFromMacro）必须一并拷贝 —— 否则宏
 *       letter=[A-Za-z] 展开后就丢掉了"这是 letter"的信息，字符类字母表
 *       只能退化成 [A-Za-z] 这样的字面文本。
 */
int RegexAst::cloneSubtree(const RegexAst &other, int index)
{
    if (index < 0 || index >= other.size())
        return -1;

    const RegexNode &src = other.node(index);
    RegexNode copy;
    copy.kind = src.kind;
    copy.charset = src.charset;
    copy.negate = src.negate;
    copy.text = src.text;
    copy.atomName = src.atomName;
    copy.atomFromMacro = src.atomFromMacro;

    const int newIndex = m_nodes.size();
    m_nodes.append(copy);  // 先占位，保证子结点递归时下标连续

    for (int child : src.children) {
        const int newChild = cloneSubtree(other, child);
        m_nodes[newIndex].children.append(newChild);
    }
    return newIndex;
}

/**
 * 功能：求字符集结点最终匹配的字符列表。
 * 输入：index —— 结点下标。
 * 输出：字符列表（每个元素为单字符）。
 * 注意：
 *  1. 非字符集结点返回空列表；
 *  2. 取反（negate == true）时返回"全集减原集合"，全集见 unicodeUniverse()；
 *  3. 返回结果已去重，但**保留原始顺序**，便于生成稳定可复现的自动机。
 */
QStringList RegexAst::resolveCharset(int index) const
{
    if (index < 0 || index >= m_nodes.size())
        return QStringList();

    const RegexNode &n = m_nodes.at(index);
    if (n.kind != RegexNodeKind::Charset)
        return QStringList();

    if (!n.negate)
        return n.charset;

    QSet<QString> excluded;
    for (const QString &ch : n.charset)
        excluded.insert(ch);

    QStringList result;
    for (const QString &ch : unicodeUniverse()) {
        if (!excluded.contains(ch))
            result.append(ch);
    }
    return result;
}

/**
 * 功能：把单个字符编码为后缀式中的记号。
 * 输入：ch —— 单字符文本。
 * 输出：形如 "\c" 的记号；不可打印字符退化为 "\xNN" 十六进制写法。
 * 注意：
 *  1. 统一加反斜杠前缀，可彻底避免字符与运算符（| * + ? . ( ) [ ] ~ #）冲突；
 *  2. 反斜杠本身编码为 "\\"，即两个反斜杠字符；
 *  3. 空字符（如空串）返回 "#"。
 */
QString RegexAst::encodeChar(const QString &ch)
{
    if (ch.isEmpty())
        return QStringLiteral("#");
    if (ch.size() > 1)
        return ch;  // 理论上不会出现，保险处理

    const QChar c = ch.at(0);
    if (c == QLatin1Char('\\'))
        return QStringLiteral("\\\\");
    if (c.unicode() >= kPrintableFirst && c.unicode() <= kPrintableLast)
        return QStringLiteral("\\") + ch;

    // 不可打印字符（制表符、换行符等）用十六进制转义，保证后缀式可读且无歧义。
    // 注意：**只用小写 "x" 与小写十六进制数字**。若用 toUpper() 把整个串转成
    // "\X09"，解码端就得同时兼容两种写法，容易漏（本项目实际踩过：
    // encodeChar 产出 "\X09" 而 decodeToken 只认 "\x09"，导致换行符记号
    // 无法还原成单字符，注释自动机因此无法跨行匹配）。
    return QStringLiteral("\\x%1").arg(static_cast<int>(c.unicode()), 2, 16, QLatin1Char('0'));
}

/**
 * 功能：递归生成后缀表达式。
 * 输入：index —— 当前结点；out —— 输出列表。
 * 输出：无（结果追加到 out）。
 * 注意：遍历顺序为"左子 → 右子 → 运算符"，即标准后序遍历。
 */
void RegexAst::buildPostfix(int index, QStringList &out) const
{
    if (index < 0 || index >= m_nodes.size())
        return;

    const RegexNode &n = m_nodes.at(index);
    switch (n.kind) {
    case RegexNodeKind::Empty:
        out.append(QStringLiteral("#"));
        return;
    case RegexNodeKind::Epsilon:
        out.append(QStringLiteral("#"));
        return;
    case RegexNodeKind::Charset: {
        const QStringList chars = resolveCharset(index);
        if (chars.isEmpty()) {
            // 空字符集：不可能匹配任何字符，用不可能出现的记号占位并标为错误源
            out.append(QStringLiteral("#"));
            return;
        }
        if (chars.size() == 1) {
            out.append(encodeChar(chars.first()));
            return;
        }
        // 多字符集展开为若干单字符的选择：c1 c2 | c3 | ...
        for (int i = 0; i < chars.size(); ++i) {
            out.append(encodeChar(chars.at(i)));
            if (i > 0)
                out.append(QStringLiteral("|"));
        }
        return;
    }
    case RegexNodeKind::Concat:
        // 注意：Concat 结点的子结点个数可能 > 2（parseConcat 会把 a.b.c 合成一个多叉结点）。
        // 二元运算符的后缀写法必须为 n 个操作数配 n-1 个运算符：
        //   a b c . .    而不是    a b c .
        // 只写一个 "." 会让片段栈剩下 n-1 个片段，构造时报
        // "逆波兰式不完整或运算符不匹配"（本项目实际踩过这个坑：
        // 注释正则 \{ (~ \})* \} 正好是 3 个子结点的连接）。
        for (int child : n.children)
            buildPostfix(child, out);
        for (int i = 1; i < n.children.size(); ++i)
            out.append(QStringLiteral("."));
        return;
    case RegexNodeKind::Alternate:
        for (int child : n.children)
            buildPostfix(child, out);
        for (int i = 1; i < n.children.size(); ++i)
            out.append(QStringLiteral("|"));
        return;
    case RegexNodeKind::Star:
        if (!n.children.isEmpty())
            buildPostfix(n.children.first(), out);
        out.append(QStringLiteral("*"));
        return;
    case RegexNodeKind::Plus:
        if (!n.children.isEmpty())
            buildPostfix(n.children.first(), out);
        out.append(QStringLiteral("+"));
        return;
    case RegexNodeKind::Optional:
        if (!n.children.isEmpty())
            buildPostfix(n.children.first(), out);
        out.append(QStringLiteral("?"));
        return;
    }
}

/**
 * 功能：生成逆波兰式（后缀表达式）。
 * 输入：无（使用本对象的根结点）。
 * 输出：以空格分隔的后缀表达式；空树返回空串。
 * 注意：运算符含义为 '|' 选择、'.' 连接、'*' 闭包、'+' 正闭包、'?' 可选，
 *       操作数为 "\c" 形式的单字符或 "#"（空串）。
 */
QString RegexAst::toPostfix() const
{
    if (m_root < 0)
        return QString();

    QStringList out;
    buildPostfix(m_root, out);
    return out.join(QLatin1Char(' '));
}

/**
 * 功能：递归生成中缀文本（调试与界面显示用）。
 * 输入：index —— 当前结点。
 * 输出：中缀文本。
 * 注意：为保证可读性，连接用空格隐式表示，选择用 "|"，后缀运算符紧随子式。
 */
QString RegexAst::buildInfix(int index) const
{
    if (index < 0 || index >= m_nodes.size())
        return QString();

    const RegexNode &n = m_nodes.at(index);
    switch (n.kind) {
    case RegexNodeKind::Empty:
    case RegexNodeKind::Epsilon:
        return QStringLiteral("#");
    case RegexNodeKind::Charset: {
        const QStringList chars = resolveCharset(index);
        if (!n.negate && chars.size() == 1)
            return chars.first();
        QString joined;
        for (const QString &ch : chars)
            joined += ch;
        return QStringLiteral("[%1]%2").arg(joined, n.negate ? QStringLiteral("(取反)") : QString());
    }
    case RegexNodeKind::Concat: {
        QStringList parts;
        for (int child : n.children)
            parts.append(buildInfix(child));
        return parts.join(QLatin1Char(' '));
    }
    case RegexNodeKind::Alternate: {
        QStringList parts;
        for (int child : n.children)
            parts.append(buildInfix(child));
        return QStringLiteral("(") + parts.join(QStringLiteral(" | ")) + QStringLiteral(")");
    }
    case RegexNodeKind::Star:
        return buildInfix(n.children.value(0)) + QStringLiteral("*");
    case RegexNodeKind::Plus:
        return buildInfix(n.children.value(0)) + QStringLiteral("+");
    case RegexNodeKind::Optional:
        return buildInfix(n.children.value(0)) + QStringLiteral("?");
    }
    return QString();
}

/** 功能：生成中缀形式文本。输入：无。输出：中缀文本或空串。 */
QString RegexAst::toInfix() const
{
    if (m_root < 0)
        return QString();
    return buildInfix(m_root);
}

// ===========================================================================
//  RegexParser
// ===========================================================================

/**
 * 功能：构造解析器并初始化内置字符集。
 * 输入：无。
 * 输出：无。
 * 注意：内置 eoln（行结束符）用于 // 行注释的定义 //(~eoln)*。
 */
RegexParser::RegexParser()
{
    m_builtinSets.insert(QStringLiteral("eoln"),
                         QStringList{QStringLiteral("\n"), QStringLiteral("\r")});
}

/**
 * 功能：把记号类型转换为可读名称（调试用）。
 * 输入：kind —— 记号类型。
 * 输出：英文短名。
 */
static const char *tokenKindName(RegexParser::TokKind kind)
{
    using K = RegexParser::TokKind;
    switch (kind) {
    case K::End: return "End";
    case K::Literal: return "Literal";
    case K::Escape: return "Escape";
    case K::Identifier: return "Identifier";
    case K::Concat: return "Concat";
    case K::Alternate: return "Alternate";
    case K::Star: return "Star";
    case K::Plus: return "Plus";
    case K::Optional: return "Optional";
    case K::LPar: return "LPar";
    case K::RPar: return "RPar";
    case K::LBracket: return "LBracket";
    case K::RBracket: return "RBracket";
    case K::Tilde: return "Tilde";
    case K::Empty: return "Empty";
    }
    return "?";
}

/**
 * 功能：把表达式切分为记号序列并返回可读文本（调试辅助）。
 * 输入：pattern —— 表达式文本。
 * 输出：形如 "LBracket@0 Literal@1:'0' ..." 的文本；切分失败时返回错误信息。
 * 注意：本函数只用于排查问题，不参与算法逻辑，也不改变对象状态。
 */
QString RegexParser::debugTokens(const QString &pattern)
{
    RegexParser parser;
    QVector<Token> tokens;
    if (!parser.tokenize(pattern, tokens))
        return QStringLiteral("tokenize 失败：") + parser.error();

    QStringList parts;
    for (const Token &t : tokens) {
        QString text = QString::fromLatin1(tokenKindName(t.kind))
                       + QStringLiteral("@%1").arg(t.pos);
        if (!t.text.isEmpty())
            text += QStringLiteral(":'%1'").arg(t.text);
        parts.append(text);
    }
    return parts.join(QLatin1Char(' '));
}

/**
 * 功能：记录一条错误信息。
 * 输入：message —— 错误描述；pos —— 出错位置（从 0 计）。
 * 输出：恒返回 false，便于在返回语句中直接使用。
 * 注意：只保留第一条错误，避免级联报错掩盖真正原因。
 */
bool RegexParser::fail(const QString &message, int pos)
{
    if (m_error.isEmpty())
        m_error = QStringLiteral("第 %1 列：%2").arg(pos + 1).arg(message);
    return false;
}

/**
 * 功能：判断记号类型能否作为"连接"的右操作数起点。
 * 输入：kind —— 记号类型。
 * 输出：可以作为起点时为 true。
 * 注意：用于在词法阶段插入显式连接记号 '.'，使语法分析阶段的连接规则
 *       只需处理一种形式（与讲稿范例的写法一致）。
 */
bool RegexParser::startsAtom(TokKind kind)
{
    switch (kind) {
    case TokKind::Literal:
    case TokKind::Escape:
    case TokKind::Identifier:
    case TokKind::LPar:
    case TokKind::LBracket:
    case TokKind::Tilde:
    case TokKind::Empty:
        return true;
    default:
        return false;
    }
}

/**
 * 功能：判断记号类型能否作为"连接"的左操作数结尾。
 * 输入：kind —— 记号类型。
 * 输出：可以作为结尾时为 true。
 * 注意：
 *  1. 只有"能结束一个原子"的记号才允许左侧连接，避免出现 "|." 这类非法串；
 *  2. **取反记号 ~ 不在此列表中**：~ 是前缀一元运算符，它本身不产生操作数，
 *     而是把它后面的那个原子取反。若把 ~ 计入，词法阶段会在 "~" 与它的
 *     操作数之间插入连接记号，把 "~eoln" 切成 "~ . eoln"，一元运算就会
 *     找不到操作数而报错。
 *     取反参与连接的场景（如 "\{ ~ \}" 中 "~ }" 之间需要连接）改由
 *     RegexParser::parseAtom() 在解析完取反项后主动吃掉紧随的连接记号，
 *     这样既保证语义正确，又不破坏 ~eoln 这类写法。
 */
static bool endsAtom(RegexParser::TokKind kind)
{
    switch (kind) {
    case RegexParser::TokKind::Literal:
    case RegexParser::TokKind::Escape:
    case RegexParser::TokKind::Identifier:
    case RegexParser::TokKind::RPar:
    case RegexParser::TokKind::RBracket:
    case RegexParser::TokKind::Star:
    case RegexParser::TokKind::Plus:
    case RegexParser::TokKind::Optional:
    case RegexParser::TokKind::Empty:
    case RegexParser::TokKind::Tilde:
        return true;
    default:
        return false;
    }
}

/**
 * 功能：把正则表达式文本切分为记号序列，并插入显式连接记号。
 * 输入：pattern —— 表达式文本；tokens —— 输出记号序列。
 * 输出：成功为 true。
 * 注意：
 *  1. 连续的字母/数字/下划线被识别为一个 Identifier，便于宏名（digit、
 *     letter）与 eoln 这类命名整体处理；若该名字不是宏，则由语法分析阶段
 *     退化为"逐字符连接"处理（见 parseAtom）；
 *  2. 转义 \x 产生 Escape 记号，其中反斜杠被去除、仅保留被转义字符；
 *  3. 连续空白在表达式内部被跳过（正则式里不表达空格本身，如需匹配空格
 *     请使用 [ ] 或 \ 加空格的写法）；
 *  4. 插入连接记号的规则：前一个记号能"结束原子"、后一个记号能"开始原子"。
 */
bool RegexParser::tokenize(const QString &pattern, QVector<Token> &tokens)
{
    // ---- 第一遍：切分记号 ----
    // 注意：**在方括号内不做标识符合并**。[abc] 里的 a、b、c 必须各成一个记号，
    // 否则会被合并成标识符 "abc"，甚至把 [A-Za-z] 合并成 "A"、"Za"、"z"，
    // 导致范围解析彻底错乱。因此这里跟踪方括号深度，深度大于 0 时把字母与
    // 数字都当作单字符处理。
    QVector<Token> raw;
    int depth = 0;
    for (int i = 0; i < pattern.size(); ++i) {
        const QChar ch = pattern.at(i);

        // 跳过空白（表达式内部的空白只是排版，不参与匹配）
        if (ch.isSpace())
            continue;

        Token token;
        token.pos = i;

        if (ch == QLatin1Char('\\')) {
            if (i + 1 >= pattern.size())
                return fail(QStringLiteral("转义符号 \\ 后面缺少字符"), i);
            ++i;
            token.kind = TokKind::Escape;
            token.text = QString(pattern.at(i));
        } else if (depth == 0 && ch.isLetter()) {
            // 方括号外：连续的字母/数字/下划线构成一个标识符（宏名、关键字等）
            int j = i;
            QString name;
            while (j < pattern.size()
                   && (pattern.at(j).isLetterOrNumber() || pattern.at(j) == QLatin1Char('_'))) {
                name += pattern.at(j);
                ++j;
            }
            i = j - 1;
            token.kind = TokKind::Identifier;
            token.text = name;
        } else if (depth > 0 && ch.isLetter()) {
            // 方括号内：每个字母各自成记号，避免 [abc] 被合并成标识符 "abc"
            token.kind = TokKind::Identifier;
            token.text = QString(ch);
        } else {
            switch (ch.unicode()) {
            case '|': token.kind = TokKind::Alternate; break;
            case '*': token.kind = TokKind::Star; break;
            case '+': token.kind = TokKind::Plus; break;
            case '?': token.kind = TokKind::Optional; break;
            case '(': token.kind = TokKind::LPar; break;
            case ')': token.kind = TokKind::RPar; break;
            case '[': token.kind = TokKind::LBracket; ++depth; break;
            case ']': token.kind = TokKind::RBracket; if (depth > 0) --depth; break;
            case '~': token.kind = TokKind::Tilde; break;
            case '#': token.kind = TokKind::Empty; break;
            case '.': token.kind = TokKind::Concat; break;
            default:
                // 含方括号内的字母与数字，以及所有普通字符
                token.kind = TokKind::Literal;
                token.text = QString(ch);
                break;
            }
        }
        raw.append(token);
    }

    // ---- 第二遍：在方括号外插入显式连接记号 '.' ----
    //
    // 关键约束（曾经踩坑）：**不得在 [ ] 字符集内部插入连接符**。
    // 例如 [0-9] 若被插入成 "0 . - . 9"，字符集解析会看到 Concat 记号而报错。
    // 由于第一遍已在方括号内禁用标识符合并，这里只需判断前后记号能否相接；
    // 字符集内部的记号（Literal/Escape）之间不会被插入，因为它们在 raw 中
    // 已不满足 startsAtom/endsAtom 之外的任何特殊情形——为稳妥起见，
    // 仍显式跟踪方括号深度。
    tokens.clear();
    int bracketDepth = 0;
    for (int i = 0; i < raw.size(); ++i) {
        const Token &token = raw.at(i);
        bool needInsert = (i > 0) && (bracketDepth == 0) && (token.kind != TokKind::Concat)
                          && endsAtom(raw.at(i - 1).kind) && startsAtom(token.kind);
        // 例外：取反记号 ~ 与其**操作数**之间绝不能插入连接记号。
        // 因为 ~ 是一元前缀运算符，紧随其后的那个记号就是它的操作数；
        // 若在此插入 "."，就会把 "~eoln" 切成 "~ . eoln"，一元运算找不到操作数。
        // 与此相对，~ 与"再后面"的记号之间需要插入连接（例如 "\{ ~ \}" 里
        // 取反项与右花括号之间），这一情形由前一记号是操作数、当前记号是 ~
        // 之外的情形自然覆盖（见 Tilde 已计入 endsAtom）。
        if (needInsert && raw.at(i - 1).kind == TokKind::Tilde)
            needInsert = false;
        if (needInsert) {
            Token dot;
            dot.kind = TokKind::Concat;
            dot.pos = token.pos;
            tokens.append(dot);
        }
        tokens.append(token);

        if (token.kind == TokKind::LBracket)
            ++bracketDepth;
        else if (token.kind == TokKind::RBracket && bracketDepth > 0)
            --bracketDepth;
    }

    Token end;
    end.kind = TokKind::End;
    end.pos = pattern.size();
    tokens.append(end);
    return true;
}

// ---------------------------------------------------------------------------
// 递归下降分析
// ---------------------------------------------------------------------------

/**
 * 功能：解析选择运算（优先级最低）：concat ('|' concat)*。
 * 输入：ast —— 目标语法树；result —— 输出结点下标。
 * 输出：成功为 true。
 * 注意：只有一个分支时不生成 Alternate 结点，避免语法树冗余。
 */
bool RegexParser::parseAlternate(RegexAst &ast, int &result)
{
    int first = -1;
    if (!parseConcat(ast, first))
        return false;

    QVector<int> branches;
    branches.append(first);

    while (!atEnd() && cur().kind == TokKind::Alternate) {
        advance();
        int next = -1;
        if (!parseConcat(ast, next))
            return false;
        branches.append(next);
    }

    if (branches.size() == 1) {
        result = first;
        return true;
    }
    const int node = ast.addNode(RegexNodeKind::Alternate, QStringLiteral("|"));
    ast.node(node).children = branches;
    result = node;
    return true;
}

/**
 * 功能：解析连接运算（优先级居中）：repeat+。
 * 输入：ast —— 目标语法树；result —— 输出结点下标。
 * 输出：成功为 true。
 * 注意：连接记号在词法阶段已显式插入；此处把 >=2 个操作数合成一个
 *       Concat 结点（多于两个时合成左结合的多叉结点，后序遍历时效果等价）。
 */
bool RegexParser::parseConcat(RegexAst &ast, int &result)
{
    int first = -1;
    if (!parseRepeat(ast, first))
        return false;

    QVector<int> parts;
    parts.append(first);

    while (!atEnd() && cur().kind == TokKind::Concat) {
        advance();
        int next = -1;
        if (!parseRepeat(ast, next))
            return false;
        parts.append(next);
    }

    if (parts.size() == 1) {
        result = first;
        return true;
    }
    const int node = ast.addNode(RegexNodeKind::Concat, QStringLiteral("."));
    ast.node(node).children = parts;
    result = node;
    return true;
}

/**
 * 功能：解析后缀重复运算（优先级最高）：atom ('*' | '+' | '?')*。
 * 输入：ast —— 目标语法树；result —— 输出结点下标。
 * 输出：成功为 true。
 * 注意：多个后缀连续出现（如 a*?）按从左到右依次包裹，与常见正则一致。
 */
bool RegexParser::parseRepeat(RegexAst &ast, int &result)
{
    int atomIndex = -1;
    if (!parseAtom(ast, atomIndex))
        return false;

    while (!atEnd()) {
        const TokKind kind = cur().kind;
        if (kind != TokKind::Star && kind != TokKind::Plus && kind != TokKind::Optional)
            break;

        RegexNodeKind nodeKind = RegexNodeKind::Star;
        if (kind == TokKind::Plus)
            nodeKind = RegexNodeKind::Plus;
        else if (kind == TokKind::Optional)
            nodeKind = RegexNodeKind::Optional;

        const int node = ast.addNode(nodeKind, cur().text);
        ast.node(node).children.append(atomIndex);
        atomIndex = node;
        advance();
    }
    result = atomIndex;
    return true;
}

/**
 * 功能：解析基本项（优先级最高）。
 * 输入：ast —— 目标语法树；result —— 输出结点下标。
 * 输出：成功为 true。
 * 注意：支持的形式为
 *         '(' 选择 ')'          —— 括号分组
 *         '[' 字符集 ']'         —— 字符集（支持 a-z 范围）
 *         '~' 基本项             —— 非（限定作用于单字符/字符集/内置命名）
 *         '\' 字符               —— 转义字符
 *         '#'                    —— 空串
 *         标识符                 —— 宏引用；非宏时退化为逐字符连接
 *         其它单字符             —— 字面量
 */
bool RegexParser::parseAtom(RegexAst &ast, int &result)
{
    if (atEnd())
        return fail(QStringLiteral("表达式意外结束，缺少操作数"), cur().pos);

    const Token token = cur();
    switch (token.kind) {
    case TokKind::LPar: {
        advance();
        int inner = -1;
        if (!parseAlternate(ast, inner))
            return false;
        if (atEnd() || cur().kind != TokKind::RPar)
            return fail(QStringLiteral("括号不匹配，缺少 )"), token.pos);
        advance();
        result = inner;
        return true;
    }
    case TokKind::LBracket: {
        // 字符集：逐个读入字符，支持 a-z 形式的范围
        // 说明：方括号内的字母在词法阶段被拆成单字符标识符（见 tokenize），
        //       因此这里 Identifier 与 Literal/Escape 一样按"一个字符"处理。
        advance();
        const int node = ast.addNode(RegexNodeKind::Charset, QStringLiteral("[]"));
        bool closed = false;
        while (!atEnd()) {
            if (cur().kind == TokKind::RBracket) {
                closed = true;
                advance();
                break;
            }
            // 取一个字符（可能是 Literal、Escape 或单字符 Identifier）
            if (cur().kind != TokKind::Literal && cur().kind != TokKind::Escape
                && cur().kind != TokKind::Identifier)
                return fail(QStringLiteral("字符集中出现非法记号"), cur().pos);
            if (cur().kind == TokKind::Identifier && cur().text.size() != 1)
                return fail(QStringLiteral("字符集中不支持多字符标识符"), cur().pos);
            const QString low = cur().text;
            advance();

            // 检查是否为范围 a-z
            if (!atEnd() && cur().kind == TokKind::Literal && cur().text == QStringLiteral("-")
                && !atEnd()) {
                const int dashPos = cur().pos;
                advance();
                if (atEnd() || (cur().kind != TokKind::Literal && cur().kind != TokKind::Escape
                                && cur().kind != TokKind::Identifier))
                    return fail(QStringLiteral("字符集范围 a-z 的右端缺失"), dashPos);
                const QString high = cur().text;
                advance();

                const ushort lo = low.at(0).unicode();
                const ushort hi = high.at(0).unicode();
                if (lo > hi)
                    return fail(QStringLiteral("字符集范围 %1-%2 的起止顺序颠倒").arg(low, high),
                                dashPos);
                for (ushort c = lo; c <= hi; ++c)
                    ast.node(node).charset.append(QString(QChar(c)));
            } else {
                ast.node(node).charset.append(low);
            }
        }
        if (!closed)
            return fail(QStringLiteral("字符集缺少 ]"), token.pos);
        // 去重但保持顺序
        QStringList unique;
        for (const QString &ch : ast.node(node).charset) {
            if (!unique.contains(ch))
                unique.append(ch);
        }
        if (unique.isEmpty())
            return fail(QStringLiteral("字符集为空，无法匹配任何字符"), token.pos);
        ast.node(node).charset = unique;

        // 记下这个字符集的"原子名"（规范化的 [..] 文本），供字符类字母表命名。
        // 说明：取的是去重后的成员集合而不是源文本片段，因此 [a-zA-Z] 与
        //       [a-zA-Za] 会得到同一个名字，便于把内容相同的字符集合并成一类。
        QVector<int> atomBytes;
        atomBytes.reserve(unique.size());
        for (const QString &ch : unique) {
            if (!ch.isEmpty())
                atomBytes.append(static_cast<int>(ch.at(0).unicode()) & 0xFF);
        }
        ast.node(node).atomName = CharClassBuilder::bytesToSetText(atomBytes);

        result = node;
        return true;
    }
    case TokKind::Tilde: {
        // 非运算：只允许作用于单个字符、字符集或内置字符集命名
        advance();
        if (atEnd())
            return fail(QStringLiteral("非运算 ~ 后面缺少操作数"), token.pos);

        // 说明：取反项解析完毕后必须检查后面是否跟着一个连接记号并**吃掉它**。
        // 原因：~ 未计入 endsAtom（否则 "~eoln" 会被切成 "~ . eoln"），
        // 因此 "\{ ~ \}" 的记号流是 "Escape Concat Tilde Escape Concat ..."，
        // 其中的 Concat 位于"取反项"与它后面的操作数之间。若不在这里吃掉，
        // 外层连接解析会把它当成一个待解析的操作数而报错；
        // 若直接丢弃不处理，语义上也不会出错（连接由外层按顺序拼接），
        // 但吃掉它可以让语法树的连接关系与记号流保持一致。
        auto consumeFollowingConcat = [this]() {
            if (!atEnd() && cur().kind == TokKind::Concat)
                advance();
        };

        if (cur().kind == TokKind::LBracket) {
            // ~[0-9]：先解析字符集，再置取反标志
            int inner = -1;
            if (!parseAtom(ast, inner))
                return false;
            ast.node(inner).negate = true;
            ast.node(inner).atomName = QStringLiteral("~") + ast.node(inner).atomName;
            result = inner;
            consumeFollowingConcat();
            return true;
        }
        if (cur().kind == TokKind::Escape || cur().kind == TokKind::Literal) {
            const QString ch = cur().text;
            advance();
            const int node = ast.addNode(RegexNodeKind::Charset, QStringLiteral("~") + ch);
            ast.node(node).charset.append(ch);
            ast.node(node).negate = true;
            ast.node(node).atomName = QStringLiteral("~") + ch;
            result = node;
            consumeFollowingConcat();
            return true;
        }
        if (cur().kind == TokKind::Identifier) {
            const QString name = cur().text;
            advance();
            const auto it = m_builtinSets.constFind(name);
            if (it == m_builtinSets.constEnd()) {
                // 也允许对宏名取反
                const auto macroIt = m_macros.constFind(name);
                if (macroIt == m_macros.constEnd())
                    return fail(QStringLiteral("非运算 ~ 只能作用于字符、字符集或内置命名，"
                                               "未知命名：%1").arg(name),
                                token.pos);
                const int inner = ast.cloneSubtree(macroIt.value(), macroIt.value().root());
                // 【加固】宏体必须是**一个简单字符集**，取反才有地方落地。
                // 历史缺陷：若宏体是 letter|digit 这类复合正则式，negate 会被置在一个
                // Alternate 结点上，而 resolveCharset() 只认 Charset 结点 —— 结果
                // ~m 被**静默**当成 m，自动机悄悄变错。这里改为明确报错。
                if (inner < 0 || ast.node(inner).kind != RegexNodeKind::Charset)
                    return fail(QStringLiteral("非运算 ~ 只支持\"宏体是单个字符集\"的宏，"
                                               "而宏 %1 的正则式不是简单字符集").arg(name),
                                token.pos);
                ast.node(inner).negate = true;
                if (!ast.node(inner).atomName.isEmpty())
                    ast.node(inner).atomName = QStringLiteral("~") + ast.node(inner).atomName;
                result = inner;
                consumeFollowingConcat();
                return true;
            }
            const int node = ast.addNode(RegexNodeKind::Charset, QStringLiteral("~") + name);
            ast.node(node).charset = it.value();
            ast.node(node).negate = true;
            ast.node(node).atomName = QStringLiteral("~") + name;
            result = node;
            consumeFollowingConcat();
            return true;
        }
        return fail(QStringLiteral("非运算 ~ 只能作用于字符、字符集或内置命名"), token.pos);
    }
    case TokKind::Escape: {
        const int node = ast.addNode(RegexNodeKind::Charset, token.text);
        ast.node(node).charset.append(token.text);
        // 转义单字符的原子名就是该字符本身（如 \+ 记作 +）
        ast.node(node).atomName = token.text;
        advance();
        result = node;
        return true;
    }
    case TokKind::Empty: {
        const int node = ast.addNode(RegexNodeKind::Empty, QStringLiteral("#"));
        advance();
        result = node;
        return true;
    }
    case TokKind::Identifier: {
        const QString name = token.text;
        advance();
        // 优先当作宏引用
        const auto it = m_macros.constFind(name);
        if (it != m_macros.constEnd()) {
            result = ast.cloneSubtree(it.value(), it.value().root());
            return true;
        }
        const auto builtin = m_builtinSets.constFind(name);
        if (builtin != m_builtinSets.constEnd()) {
            const int node = ast.addNode(RegexNodeKind::Charset, name);
            ast.node(node).charset = builtin.value();
            ast.node(node).atomName = name;  // 内置命名（eoln）直接作原子名
            result = node;
            return true;
        }
        // 非宏名：退化为"逐字符连接"（例如正则式里直接写 if、then 等关键字）
        int chain = -1;
        for (int i = 0; i < name.size(); ++i) {
            const int node = ast.addNode(RegexNodeKind::Charset, QString(name.at(i)));
            ast.node(node).charset.append(QString(name.at(i)));
            ast.node(node).atomName = QString(name.at(i));
            if (chain < 0) {
                chain = node;
            } else {
                const int concat = ast.addNode(RegexNodeKind::Concat, QStringLiteral("."));
                ast.node(concat).children = {chain, node};
                chain = concat;
            }
        }
        result = chain;
        return true;
    }
    case TokKind::Literal: {
        const int node = ast.addNode(RegexNodeKind::Charset, token.text);
        ast.node(node).charset.append(token.text);
        ast.node(node).atomName = token.text;  // 单字符字面量的原子名就是它自己
        advance();
        result = node;
        return true;
    }
    default:
        return fail(QStringLiteral("此处不应出现该运算符"), token.pos);
    }
}

/**
 * 功能：解析一条完整的正则表达式。
 * 输入：pattern —— 表达式文本；out —— 输出语法树。
 * 输出：成功为 true；失败时 error() 给出带列号的原因。
 * 注意：
 *  1. 每次调用都会重置内部状态与错误信息；
 *  2. 解析完成后必须恰好消费完全部记号，多余的记号（如多余的 ) ）会报错。
 */
bool RegexParser::parse(const QString &pattern, RegexAst &out)
{
    m_error.clear();
    m_index = 0;
    m_tokens.clear();

    out = RegexAst();  // 清空输出

    if (pattern.trimmed().isEmpty()) {
        // 空表达式视为空串（ε）：任务书中以 # 显式表示，这里容错处理
        const int node = out.addNode(RegexNodeKind::Empty, QStringLiteral("#"));
        out.setRoot(node);
        return true;
    }

    if (!tokenize(pattern, m_tokens))
        return false;

    int rootIndex = -1;
    if (!parseAlternate(out, rootIndex))
        return false;

    if (!atEnd())
        return fail(QStringLiteral("存在多余的记号，请检查括号与运算符是否配对"), cur().pos);

    out.setRoot(rootIndex);
    return true;
}

}  // namespace ccd
