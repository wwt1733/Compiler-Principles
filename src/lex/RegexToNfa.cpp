/******************************************************************************
 * (1) 版权信息 : 编译原理课程设计 - 华南师范大学计算机学院
 * (2) 文件名称 : RegexToNfa.cpp
 *     标识符   : RegexToNfa
 *     摘要     : Thompson 构造法的实现。逆波兰式扫描 + 状态号栈：
 *                  （1）操作数       —— 新建两个状态，其间连一条标号边；
 *                  （2）连接符 '.'   —— 弹 4 个状态号，在中间补一条 ε 边；
 *                  （3）选择符 '|'   —— 弹 4 个状态号，新建 2 个状态做分支；
 *                  （4）闭包符 '*'   —— 弹 2 个状态号，新建 2 个状态加 ε 边；
 *                  （5）正闭包 '+'   —— 等价于 R R*，展开后再构造；
 *                  （6）可选 '?'     —— 等价于 R | ε，展开后再构造。
 *                该算法与讲稿/范例的写法保持一致，便于在实验报告中对齐说明。
 * (3) 版本号   : 1.0.0       作者/修改者 : <学号 姓名>   完成日期 : 2026-XX-XX
 * (4) 版本历史 : 1.0.0  2026-XX-XX  首次实现
 *****************************************************************************/

#include "lex/RegexToNfa.h"

#include "common/Logger.h"
#include "lex/CharClass.h"

namespace ccd {

namespace {

/** 逆波兰式中各类运算符的记号文本，均以反斜杠转义前缀写出单字符 */
const QLatin1Char kOpConcat('.');
const QLatin1Char kOpAlternate('|');
const QLatin1Char kOpStar('*');
const QLatin1Char kOpPlus('+');
const QLatin1Char kOpOptional('?');
const QLatin1Char kOpEmpty('#');

/**
 * 功能：只保留 used 里的字符类，并重新编号。
 * 输入：table —— 原始字符类表；used —— 真正出现在自动机边上的符号键。
 * 输出：新的字符类表（键重新编号为 @0、@1 …）。
 * 注意：必须做这一步。生成的词法程序用"类的下标"当状态转换表的列号，
 *       若表里留着一个根本没有边的类，列号就会错位；界面上也会多出一列空列。
 */
CharClassAlphabet pruneToUsed(const CharClassAlphabet &table, const QSet<QString> &used)
{
    CharClassAlphabet pruned;
    pruned.byteClass.fill(-1, 256);

    QVector<int> oldToNew(table.classes.size(), -1);
    for (int old = 0; old < table.classes.size(); ++old) {
        const CharClassInfo &info = table.classes.at(old);
        if (!used.contains(info.key))
            continue;
        CharClassInfo copy = info;
        copy.key = QStringLiteral("@%1").arg(pruned.classes.size());
        oldToNew[old] = pruned.classes.size();
        pruned.classes.append(copy);
    }
    for (int b = 0; b < 256 && b < table.byteClass.size(); ++b) {
        const int old = table.byteClass.at(b);
        if (old >= 0 && old < oldToNew.size())
            pruned.byteClass[b] = oldToNew.at(old);
    }
    return pruned;
}

}  // namespace

// ---------------------------------------------------------------------------
// 辅助
// ---------------------------------------------------------------------------

/**
 * 功能：插入一个全新状态。
 * 输入：nfa —— 目标 NFA。
 * 输出：新状态编号。
 * 注意：状态编号从 0 开始连续分配；超过上限时返回 -1（由调用者检查）。
 */
int RegexToNfa::newState(Nfa &nfa)
{
    if (nfa.stateCount >= kMaxStates)
        return -1;
    return nfa.stateCount++;
}

/**
 * 功能：插入一条状态转换边。
 * 输入：nfa —— 目标 NFA；from/to —— 起止状态；symbol —— 标号；isEpsilon —— 是否 ε 边。
 * 输出：无。
 * 注意：symbol 为空且 isEpsilon 为真表示 ε 边；其它情况必须是单字符。
 */
void RegexToNfa::addEdge(Nfa &nfa, int from, int to, const QString &symbol, bool isEpsilon)
{
    nfa.edges.append(Transition(from, to, isEpsilon ? QString() : symbol, isEpsilon));
}

/**
 * 功能：记录一条错误信息。
 * 输入：message —— 错误描述。
 * 输出：恒返回 false，便于直接用于返回语句。
 * 注意：只保留第一条错误，避免级联信息干扰判断。
 */
bool RegexToNfa::fail(const QString &message)
{
    if (m_error.isEmpty())
        m_error = message;
    return false;
}

/**
 * 功能：把逆波兰式记号还原为它代表的**单个字符**。
 * 输入：token —— 形如 "\c"、"\xNN"、"\\" 的记号。
 * 输出：单字符文本；无法识别时返回空串。
 * 注意：
 *  1. 必须与 RegexAst::encodeChar() 的编码规则严格对应；
 *  2. **不能简单地"去掉反斜杠取剩余部分"**：不可打印字符编码为 "\x09"
 *     这类 4 字符形式，去反斜杠后会得到 "x09" 这样的 3 字符"符号"，
 *     于是 addEdge 里就出现了长度不为 1 的输入符号，自动机永远匹配不上
 *     （本项目实际踩过：注释 \{ ~ \} 因换行符编码错误而无法跨行匹配）。
 */
QString RegexToNfa::decodeToken(const QString &token)
{
    if (token.isEmpty())
        return QString();
    if (!token.startsWith(QLatin1Char('\\')))
        return token.at(0);  // 运算符等单字符记号

    const QString body = token.mid(1);
    if (body.isEmpty())
        return QString();
    // "\\" → 反斜杠本身
    if (body == QStringLiteral("\\"))
        return QStringLiteral("\\");
    // "\xNN" → 十六进制码位
    if (body.size() == 3 && (body.at(0) == QLatin1Char('x') || body.at(0) == QLatin1Char('X'))) {
        bool ok = false;
        const ushort code = body.mid(1).toUShort(&ok, 16);
        if (ok)
            return QString(QChar(code));
        return QString();
    }
    // "\c" → 单字符
    if (body.size() == 1)
        return body;
    return QString();
}

/**
 * 功能：把正则语法树展开为逆波兰记号序列。
 * 输入：ast —— 语法树；index —— 子树根；out —— 输出记号序列。
 * 输出：无（结果追加到 out）。
 * 注意：与 RegexAst::toPostfix() 的规则完全一致：字符集展开为若干单字符的
 *       选择，运算符使用单字符记号。
 */
void RegexToNfa::expandPostfix(const RegexAst &ast, int index, QStringList &out,
                               const CharClassAlphabet *symbols)
{
    if (index < 0 || index >= ast.size())
        return;

    const RegexNode &node = ast.node(index);
    switch (node.kind) {
    case RegexNodeKind::Empty:
    case RegexNodeKind::Epsilon:
        out.append(QString(kOpEmpty));
        return;
    case RegexNodeKind::Charset: {
        const QStringList chars = ast.resolveCharset(index);
        if (chars.isEmpty())
            return;

        if (symbols != nullptr) {
            // ---- 按字符类模式：整个字符集只发**一个**记号 ----
            // 记号形如 "\C:@0,@3"，表示"这些字符类中的任意一个"。
            // 不展开成 | 链的原因：n 个字符的字符集原来的写法会额外产生
            // n-1 个选择运算（每个 2 个状态），letter(52) 要 106 个状态、
            // ~ \}(255) 要 1018 个状态 —— 这正是过去 NFA 有 1563 个状态、
            // 图形根本没法看的原因。改成"一个操作数 + 平行边"后只剩 2 个状态。
            //
            // 关于前缀：必须是字节模式绝不可能产生的记号。字节模式的记号只有
            // "\c"（2 字符）、"\xNN"（4 字符）与 "\\"，因此 3 字符的 "\C:"
            // 前缀不会与之冲突（否则字面量 C 的记号 "\C" 会被误判）。
            QStringList keys;
            for (const QString &ch : chars) {
                if (ch.isEmpty())
                    continue;
                const QString key = symbols->symbolOfByte(static_cast<int>(ch.at(0).unicode()) & 0xFF);
                if (key.isEmpty())
                    continue;  // 不属于任何字符类（理论上不会发生）
                if (!keys.contains(key))
                    keys.append(key);
            }
            if (keys.isEmpty())
                return;
            out.append(QStringLiteral("\\C:") + keys.join(QLatin1Char(',')));
            return;
        }

        for (int i = 0; i < chars.size(); ++i) {
            // 必须用 RegexAst::encodeChar：不可打印字符（制表/换行/回车）要编码成
            // "\x09" 这类形式，直接写 "\" + 原字符会得到 2 个字符的记号，
            // 破坏"按空格切分"的记号流（本项目实际踩过这个坑）。
            out.append(RegexAst::encodeChar(chars.at(i)));
            if (i > 0)
                out.append(QString(kOpAlternate));
        }
        return;
    }
    case RegexNodeKind::Concat:
        // 与 RegexAst::buildPostfix 一致：n 个子结点要发 n-1 个连接符。
        // 只发一个会让片段栈剩下多余片段（构造时报"逆波兰式不完整"）。
        for (int child : node.children)
            expandPostfix(ast, child, out, symbols);
        for (int i = 1; i < node.children.size(); ++i)
            out.append(QString(kOpConcat));
        return;
    case RegexNodeKind::Alternate:
        for (int child : node.children)
            expandPostfix(ast, child, out, symbols);
        for (int i = 1; i < node.children.size(); ++i)
            out.append(QString(kOpAlternate));
        return;
    case RegexNodeKind::Star:
        expandPostfix(ast, node.children.value(0), out, symbols);
        out.append(QString(kOpStar));
        return;
    case RegexNodeKind::Plus:
        expandPostfix(ast, node.children.value(0), out, symbols);
        out.append(QString(kOpPlus));
        return;
    case RegexNodeKind::Optional:
        expandPostfix(ast, node.children.value(0), out, symbols);
        out.append(QString(kOpOptional));
        return;
    }
}

// ---------------------------------------------------------------------------
// 核心：状态号栈构造
// ---------------------------------------------------------------------------

/**
 * 功能：用状态号栈把逆波兰记号序列构造为一段 NFA 片段。
 * 输入：tokens —— 逆波兰记号序列；nfa —— 目标 NFA。
 * 输出：startOut/acceptOut —— 该片段的开始与接受状态号。
 * 返回：成功为 true。
 * 注意（算法要点，与讲稿范例一致）：
 *  1. 栈中保存的是"片段"的两个端点，压栈顺序为 (开始, 接受)；
 *  2. 连接符：弹出四个端点 s1 s2 s3 s4（栈顶在下标 0），在 s3 与 s2 之间加
 *     ε 边完成拼接，再把 s4（前段开始）与 s1（后段接受）压回；
 *  3. 选择符：新建两个状态 i、i+1，i 经 ε 分别到 s4、s2，s3、s1 经 ε 分别到
 *     i+1，即 (前段|后段)；
 *  4. 闭包符：新建 i、i+1，加 ε 边 i→i+1（跳过）、i→s1、s2→i+1、s2→s1
 *     （重复），形成 * 的语义；
 *  5. 正闭包 + 与可选 ? 在此处按等价式直接展开：R+ ⇒ R R*，R? ⇒ R | ε，
 *     这样核心构造只需处理 '.'、'|'、'*' 三种运算符。
 *
 * 实现方式说明：由于 '+' 与 '?' 需要"重复使用同一片段"，而本构造器生成的是
 * 共享结点的图结构，故实现上不在记号层展开，而是**在片段栈层面**每次重新
 * 展开整个已构造子片段——为保持实现简洁与可读，这里采用更直接的做法：
 *  '+' 视为在已有片段基础上再叠一个 '*' 分支，'?' 视为追加一个 ε 分支。
 */
bool RegexToNfa::buildFragment(const QStringList &tokens, Nfa &nfa, int &startOut, int &acceptOut)
{
    // 片段栈：每压入一个片段就压入两个状态号（开始、接受）
    QVector<int> stack;
    stack.reserve(tokens.size() * 2);

    for (int i = 0; i < tokens.size(); ++i) {
        const QString token = tokens.at(i);

        if (token == QString(kOpConcat)) {
            // ---- 连接运算：栈顶四个状态号 s1 s2 s3 s4 ----
            if (stack.size() < 4)
                return fail(QStringLiteral("错误：连接运算 . 的操作数不足"));
            const int s1 = stack.takeLast();
            const int s2 = stack.takeLast();
            const int s3 = stack.takeLast();
            const int s4 = stack.takeLast();
            addEdge(nfa, s3, s2, QString(), true);  // 前段接受 → 后段开始
            stack.append(s4);                       // 整体开始
            stack.append(s1);                       // 整体接受
            continue;
        }

        if (token == QString(kOpAlternate)) {
            // ---- 选择运算：栈顶四个状态号 s1 s2 s3 s4 ----
            if (stack.size() < 4)
                return fail(QStringLiteral("错误：选择运算 | 的操作数不足"));
            const int s1 = stack.takeLast();
            const int s2 = stack.takeLast();
            const int s3 = stack.takeLast();
            const int s4 = stack.takeLast();

            const int i0 = newState(nfa);
            const int i1 = newState(nfa);
            if (i0 < 0 || i1 < 0)
                return fail(QStringLiteral("错误：NFA 状态数超过上限 %1").arg(kMaxStates));

            addEdge(nfa, i0, s4, QString(), true);
            addEdge(nfa, i0, s2, QString(), true);
            addEdge(nfa, s3, i1, QString(), true);
            addEdge(nfa, s1, i1, QString(), true);

            stack.append(i0);
            stack.append(i1);
            continue;
        }

        if (token == QString(kOpStar)) {
            // ---- 闭包运算：栈顶两个状态号 s1 s2 ----
            if (stack.size() < 2)
                return fail(QStringLiteral("错误：闭包运算 * 的操作数不足"));
            const int s1 = stack.takeLast();
            const int s2 = stack.takeLast();

            const int i0 = newState(nfa);
            const int i1 = newState(nfa);
            if (i0 < 0 || i1 < 0)
                return fail(QStringLiteral("错误：NFA 状态数超过上限 %1").arg(kMaxStates));

            addEdge(nfa, i0, s2, QString(), true);   // 进入原片段
            addEdge(nfa, i0, i1, QString(), true);   // 跳过（零次）
            addEdge(nfa, s1, s2, QString(), true);   // 重复（多次）
            addEdge(nfa, s1, i1, QString(), true);   // 结束

            stack.append(i0);
            stack.append(i1);
            continue;
        }

        if (token == QString(kOpPlus)) {
            // ---- 正闭包：R+ 等价于 R R*，即"至少出现一次" ----
            // 构造：新建入口 i0 与出口 i1；
            //   i0 --ε--> s2        进入原片段（必经，保证至少一次）
            //   s1 --ε--> s2        回到片段开始，实现重复
            //   s1 --ε--> i1        结束
            if (stack.size() < 2)
                return fail(QStringLiteral("错误：正闭包运算 + 的操作数不足"));
            const int s1 = stack.takeLast();
            const int s2 = stack.takeLast();

            const int i0 = newState(nfa);
            const int i1 = newState(nfa);
            if (i0 < 0 || i1 < 0)
                return fail(QStringLiteral("错误：NFA 状态数超过上限 %1").arg(kMaxStates));

            addEdge(nfa, i0, s2, QString(), true);
            addEdge(nfa, s1, s2, QString(), true);
            addEdge(nfa, s1, i1, QString(), true);

            stack.append(i0);
            stack.append(i1);
            continue;
        }

        if (token == QString(kOpOptional)) {
            // ---- 可选：R? 等价于 R | ε ----
            if (stack.size() < 2)
                return fail(QStringLiteral("错误：可选运算 ? 的操作数不足"));
            const int s1 = stack.takeLast();
            const int s2 = stack.takeLast();

            const int i0 = newState(nfa);
            const int i1 = newState(nfa);
            if (i0 < 0 || i1 < 0)
                return fail(QStringLiteral("错误：NFA 状态数超过上限 %1").arg(kMaxStates));

            // i0 可直接到 i1（不出现），也可经原片段到 i1
            addEdge(nfa, i0, s2, QString(), true);
            addEdge(nfa, s1, i1, QString(), true);
            addEdge(nfa, i0, i1, QString(), true);

            stack.append(i0);
            stack.append(i1);
            continue;
        }

        if (token == QString(kOpEmpty)) {
            // ---- 空串 ε：新建两个状态并以 ε 边相连 ----
            const int s0 = newState(nfa);
            const int s1 = newState(nfa);
            if (s0 < 0 || s1 < 0)
                return fail(QStringLiteral("错误：NFA 状态数超过上限 %1").arg(kMaxStates));
            addEdge(nfa, s0, s1, QString(), true);
            stack.append(s0);
            stack.append(s1);
            continue;
        }

        // ---- 操作数 ----
        // 两种形式：
        //   "\C:键1,键2,…"  —— 按字符类模式下的一个字符集（一个操作数、多条平行边）
        //   "\c" / "\xNN"   —— 按字节模式下的单个字符
        if (token.startsWith(QStringLiteral("\\C:"))) {
            const QStringList keys = token.mid(3).split(QLatin1Char(','), Qt::SkipEmptyParts);
            if (keys.isEmpty())
                return fail(QStringLiteral("错误：字符类记号 %1 里没有任何符号").arg(token));

            const int s0 = newState(nfa);
            const int s1 = newState(nfa);
            if (s0 < 0 || s1 < 0)
                return fail(QStringLiteral("错误：NFA 状态数超过上限 %1").arg(kMaxStates));
            // 一个字符集 = 一对状态 + |C| 条平行标号边（Thompson 的标准写法）。
            // 这些类两两不交（等价类划分保证），因此在同一状态下不会互相竞争。
            for (const QString &key : keys)
                addEdge(nfa, s0, s1, key, false);
            stack.append(s0);
            stack.append(s1);
            continue;
        }

        const QString symbol = decodeToken(token);
        if (symbol.isEmpty())
            return fail(QStringLiteral("错误：记号 %1 无法还原为单个字符").arg(token));

        const int s0 = newState(nfa);
        const int s1 = newState(nfa);
        if (s0 < 0 || s1 < 0)
            return fail(QStringLiteral("错误：NFA 状态数超过上限 %1").arg(kMaxStates));
        addEdge(nfa, s0, s1, symbol, false);
        stack.append(s0);
        stack.append(s1);
    }

    if (stack.size() != 2)
        return fail(QStringLiteral("错误：逆波兰式不完整或运算符不匹配（片段栈剩余 %1 个状态）")
                        .arg(stack.size()));

    startOut = stack.at(0);
    acceptOut = stack.at(1);
    return true;
}

// ---------------------------------------------------------------------------
// 对外接口
// ---------------------------------------------------------------------------

/**
 * 功能：由多条规则构造合并后的 NFA。
 * 输入：rules —— 规则列表；out —— 输出 NFA；mode —— 字母表粒度。
 * 输出：成功为 true。
 * 注意：
 *  1. 先为每条规则单独构造片段，再新建统一开始状态用 ε 边连到各片段开始状态，
 *     这正是任务书"多个需生成 DFA 图的正则式之间实现的是或（选择）算法"的落地；
 *  2. 接受状态登记以规则为单位的标签；同一状态被多条规则共享时标签会累积；
 *  3. 状态编号 0 固定为统一开始状态；
 *  4. 按字符类模式时，字母表不是字节而是"字符类"，且**只保留真正出现在边上的类**
 *     （类的下标就是状态转换表的列号，见 CharClassAlphabet 的约定）。
 */
bool RegexToNfa::build(const QVector<NfaRule> &rules, Nfa &out, AlphabetMode mode)
{
    out = Nfa();
    m_error.clear();
    m_postfixMap.clear();

    if (rules.isEmpty())
        return fail(QStringLiteral("没有需要构造自动机的规则（请检查规格文件中是否含有"
                                   "带编码的定义）"));

    // ---- 0. 按字符类模式：先收集全部字符集原子，求出字符类字母表 ----
    // 必须在构造片段之前算完：字符集要按"它覆盖了哪些字符类"发记号。
    CharClassAlphabet symbols;
    if (mode == AlphabetMode::CharClass) {
        QVector<CharAtom> atoms;
        for (const NfaRule &rule : rules) {
            if (!rule.ast.isEmpty())
                CharClassBuilder::collectAtoms(rule.ast, atoms);
        }
        symbols = CharClassBuilder::build(atoms);
        if (symbols.isEmpty()) {
            // 极端情况：全部规则都是 # 空串之类，没有任何字符集原子
            LOG_WARN(QStringLiteral("规格里没有字符集原子，字符类字母表为空，"
                                    "已回退到按字节字母表。"));
            mode = AlphabetMode::Byte;
        }
    }
    const CharClassAlphabet *symbolTable =
        (mode == AlphabetMode::CharClass) ? &symbols : nullptr;

    // 统一开始状态
    const int globalStart = newState(out);
    if (globalStart != 0)
        return fail(QStringLiteral("内部错误：统一开始状态编号不为 0"));
    out.start = globalStart;

    for (const NfaRule &rule : rules) {
        if (rule.ast.isEmpty()) {
            LOG_WARN(QStringLiteral("规则 %1 的语法树为空，已跳过。").arg(rule.name));
            continue;
        }

        QStringList tokens;
        expandPostfix(rule.ast, rule.ast.root(), tokens, symbolTable);
        // 对照区显示的是"人类可读"的逆波兰式（与 RegexAst::toPostfix 一致），
        // 而不是内部使用的记号流（字符类模式下内部记号是 \C:@0 这种形式）。
        m_postfixMap.insert(rule.name, rule.ast.toPostfix());

        if (tokens.isEmpty()) {
            LOG_WARN(QStringLiteral("规则 %1 展开后没有记号，已跳过。").arg(rule.name));
            continue;
        }

        int fragStart = -1;
        int fragAccept = -1;
        if (!buildFragment(tokens, out, fragStart, fragAccept))
            return false;

        // 统一开始状态 ε 到本片段开始状态
        addEdge(out, out.start, fragStart, QString(), true);

        // 接受状态打标签（同一状态下多条规则的标签按规则顺序累积）
        AcceptTag tag;
        tag.defName = rule.name;
        tag.code = rule.code;
        tag.isComment = rule.isComment;
        out.accepts[fragAccept].append(tag);
    }

    if (symbolTable != nullptr) {
        QSet<QString> used;
        for (const Transition &e : out.edges) {
            if (!e.isEpsilon && !e.symbol.isEmpty())
                used.insert(e.symbol);
        }
        out.alphabetInfo = pruneToUsed(symbols, used);
        out.alphabet.clear();
        for (const CharClassInfo &info : out.alphabetInfo.classes)
            out.alphabet.append(info.key);
    } else {
        out.alphabet = out.collectAlphabet();
    }

    LOG_INFO(QStringLiteral("NFA 构造完成：%1 个状态、%2 条边、%3 个接受状态、"
                            "字母表 %4 个%5。")
                 .arg(out.stateCount)
                 .arg(out.edges.size())
                 .arg(out.accepts.size())
                 .arg(out.alphabet.size())
                 .arg(out.alphabetInfo.isEmpty() ? QStringLiteral("符号")
                                                 : QStringLiteral("字符类")));
    return true;
}

/**
 * 功能：由单条正则式构造 NFA（单元测试与界面演示用）。
 * 输入：pattern —— 正则式文本；out —— 输出 NFA。
 * 输出：成功为 true。
 * 注意：接受状态不带编码标签，仅用于验证构造算法的正确性。
 *       **固定走按字节模式**：这个入口是给"单条正则式"的单元测试与演示用的，
 *       没有规格文件里的宏名作参照，字母表按字节才与教材例题（a|b)*abb 等一致。
 */
bool RegexToNfa::buildFromPattern(const QString &pattern, Nfa &out)
{
    RegexParser parser;
    RegexAst ast;
    if (!parser.parse(pattern, ast))
        return fail(QStringLiteral("正则式解析失败：%1").arg(parser.error()));

    NfaRule rule;
    rule.name = pattern;
    rule.code = -1;
    rule.ast = ast;

    QVector<NfaRule> rules;
    rules.append(rule);
    return build(rules, out, AlphabetMode::Byte);
}

}  // namespace ccd
