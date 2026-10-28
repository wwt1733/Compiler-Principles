/******************************************************************************
 * (1) 版权信息 : 编译原理课程设计 - 华南师范大学计算机学院
 * (2) 文件名称 : LexCodeGen.cpp
 *     标识符   : LexCodeGen
 *     摘要     : 词法分析源程序生成器的实现。把最小化 DFA 的状态转换表
 *                翻译成等价的 C++ 代码：状态常量、字符→列号映射、转移函数，
 *                再按所选方法生成状态分派；最后拼上"最长匹配 + 回退 + 输出 .lex"
 *                的驱动逻辑。
 * (3) 版本号   : 1.0.0       作者/修改者 : <学号 姓名>   完成日期 : 2026-XX-XX
 * (4) 版本历史 : 1.0.0  2026-XX-XX  首次实现
 *****************************************************************************/

#include "lex/LexCodeGen.h"

#include <algorithm>

#include "common/Logger.h"

namespace ccd {

namespace {

/** 生成代码里的"无转移"标记 */
const int kNone = -1;

/**
 * 功能：C++ 字符串字面量的转义。
 * 输入：text —— 原文。
 * 输出：可直接放进双引号内的文本。
 * 注意：必须转义反斜线与双引号；制表、回车、换行用转义序列，
 *       其它不可打印字符用三位八进制转义（与生成的 C++ 代码兼容）。
 */
QString cppEscape(const QString &text)
{
    QString result;
    for (const QChar &ch : text) {
        const ushort u = ch.unicode();
        switch (u) {
        case '\\': result += QStringLiteral("\\\\"); break;
        case '"':  result += QStringLiteral("\\\""); break;
        case '\t': result += QStringLiteral("\\t"); break;
        case '\r': result += QStringLiteral("\\r"); break;
        case '\n': result += QStringLiteral("\\n"); break;
        default:
            if (u < 32 || u > 126)
                result += QStringLiteral("\\%1").arg(u, 3, 8, QLatin1Char('0'));
            else
                result += ch;
            break;
        }
    }
    return result;
}

/**
 * 功能：把正则式里的字符还原成它对应的**字节值**。
 * 输入：token —— 形如 "a"、"\{"、"\\"、"\x09"、"\\x0a" 的记号（单字符文本）。
 * 输出：0~255 的字节值。
 * 注意：必须与 RegexToNfa::decodeToken() 的规则保持一致：
 *       记号里的 "\xNN" 是**转义写法**，不能只取首字符。
 *       若这里只取 symbol.at(0)（即取到 '\'），高位字节会全部错位，
 *       字符比较链就永远匹配不中，注释里一旦出现非 ASCII 字节（例如 UTF-8
 *       汉字）词法分析立刻断掉。
 */
int byteOfSymbol(const QString &symbol)
{
    if (symbol.isEmpty())
        return -1;
    if (symbol.size() == 1)
        return symbol.at(0).unicode() & 0xFF;
    // 多字符记号一定是转义写法：逐段解析（同时兼容 "\\x09" 与 "\\\\"）
    if (symbol.at(0) == QLatin1Char('\\')) {
        const QString body = symbol.mid(1);
        if (body.isEmpty())
            return '\\';
        if (body.at(0) == QLatin1Char('x') || body.at(0) == QLatin1Char('X')) {
            bool ok = false;
            const uint value = body.mid(1).toUInt(&ok, 16);
            return ok ? static_cast<int>(value & 0xFF) : -1;
        }
        return body.at(0).unicode() & 0xFF;
    }
    return symbol.at(0).unicode() & 0xFF;
}

/**
 * 功能：把一个字节值写成 C++ 里**无符号**的字符比较常量。
 * 输入：byte —— 0~255。
 * 输出：形如 "0x80"、"'a'"、"0x5c" 的文本。
 * 注意（重要坑）：**绝不能**生成 "'\200'" 这类八进制字符字面量。
 *       C++ 里 '\200' 的类型是 char（有符号），值是 -128；与 unsigned char
 *       的实际字节值 128 比较时，前者按 int 提升为 -128，后者提升为 128，
 *       比较恒为假，于是所有 0x80~0xFF 的边全部失效。
 *       本项目实际踩过这个坑：注释里的 UTF-8 汉字（首字节 0xE5 等）无法
 *       前进，注释被逐个符号拆开输出。改用整型常量 0x80 即无此问题
 *       （0x80 的类型是 int，值为 128，与 unsigned char 提升后的值一致）。
 */
QString cppByteLiteral(int byte)
{
    const int b = byte & 0xFF;
    if (b == '\\')
        return QStringLiteral("0x5c");
    if (b == '\'')
        return QStringLiteral("0x27");
    if (b >= 32 && b <= 126)
        return QStringLiteral("'%1'").arg(QChar(static_cast<ushort>(b)));
    return QStringLiteral("0x%1").arg(b, 2, 16, QLatin1Char('0'));
}

/**
 * 功能：按符号键在字符类表里查一个字符类。
 * 输入：info —— 字符类表；key —— 符号键（"@0"）。
 * 输出：找到时返回其指针，否则返回 nullptr。
 * 注意：类数只有二十上下，线性查找足够；这里做成函数是为了让调用点不必重复写循环。
 */
const CharClassInfo *findClassByKey(const CharClassAlphabet &info, const QString &key)
{
    for (const CharClassInfo &cls : info.classes) {
        if (cls.key == key)
            return &cls;
    }
    return nullptr;
}

/**
 * 功能：生成"变量 var 的取值属于字符类 cls"的布尔表达式。
 * 输入：cls —— 字符类；var —— 变量名（如 "c"、"ch"）。
 * 输出：形如 "(c >= '0' && c <= '9')" 或 "c == ';'" 的表达式；
 *       多个区间之间用 || 连接。
 * 注意：1. 比较常量统一走 cppByteLiteral()，避免有符号字符字面量的坑（§13.12）；
 *       2. **必须精确**，不能用 "else 兜底" —— 不属于任何字符类的字节
 *          （例如 mini-C 的换行符）必须仍然走到 charIndex() 的 return -1，
 *          否则词法错误检测会失效。
 */
QString emitClassTest(const CharClassInfo &cls, const QString &var)
{
    if (cls.ranges.isEmpty())
        return QStringLiteral("0");
    QStringList terms;
    terms.reserve(cls.ranges.size());
    for (const QPair<int, int> &range : cls.ranges) {
        if (range.first == range.second) {
            terms << QStringLiteral("%1 == %2").arg(var, cppByteLiteral(range.first));
        } else {
            terms << QStringLiteral("(%1 >= %2 && %1 <= %3)")
                         .arg(var, cppByteLiteral(range.first), cppByteLiteral(range.second));
        }
    }
    return terms.join(QStringLiteral(" || "));
}

/**
 * 功能：生成字符→列号的判定代码（供 charIndex() 使用）。
 * 输入：alphabet —— DFA 字母表（顺序即列号）；info —— 字符类表。
 * 输出：函数体里的判定语句。
 * 注意：1. 字符类模式（info 非空）：每列一条"是否属于该字符类"的区间判定，
 *          十几列即可覆盖全部 256 个字节，比原来 256 条等值比较短得多；
 *       2. 按字节模式（info 为空）：沿用原来的等值比较链，保证手工构造的
 *          小 DFA 也能正常生成代码。
 */
QString emitCharIndexChain(const QStringList &alphabet, const CharClassAlphabet &info)
{
    QString code;
    if (!info.isEmpty()) {
        for (int i = 0; i < alphabet.size(); ++i) {
            const CharClassInfo *cls = findClassByKey(info, alphabet.at(i));
            if (cls == nullptr)
                continue;
            code += QStringLiteral("    if (%1) return %2;    /* %3 */\n")
                        .arg(emitClassTest(*cls, QStringLiteral("c")))
                        .arg(i)
                        .arg(cls->name);
        }
        return code;
    }

    for (int i = 0; i < alphabet.size(); ++i) {
        const int byte = byteOfSymbol(alphabet.at(i));
        if (byte < 0)
            continue;
        code += QStringLiteral("    if (c == %1) return %2;\n")
                    .arg(cppByteLiteral(byte))
                    .arg(i);
    }
    return code;
}

/**
 * 功能：生成 DFA 状态常量定义。
 * 输入：dfa —— DFA。
 * 输出：形如 "static const int S0 = 0;" 的代码。
 */
QString emitStateConstants(const Dfa &dfa)
{
    QString code;
    for (int s = 0; s < dfa.stateCount; ++s)
        code += QStringLiteral("static const int S%1 = %2;\n").arg(s).arg(s);
    return code;
}

}  // namespace

/**
 * 功能：生成"字面单词常量表"代码（关键字表 + 专用符号表）。
 * 输入：literals —— 字面单词 → 编码；keywords —— 关键字 → 编码。
 * 输出：C++ 代码片段，含两张表与两个查表函数。
 * 注意：
 *  1. 关键字表用于"识别出标识符后改判为关键字编码"；
 *  2. 符号表把运算符/界符的字面量映射到其编码，词法程序据此给出 FinalCode；
 *  3. 两张表都按字典序输出，保证生成结果可复现（便于比对与写进报告）。
 */
QString LexCodeGen::emitLiteralTables(const QMap<QString, int> &literals,
                                      const QMap<QString, int> &keywords) const
{
    QString code;
    code += QStringLiteral(
        "/* ------------------------------------------------------------------ */\n"
        "/* 字面单词常量表：关键字与专用符号的“原文 → 编码”映射。              */\n"
        "/* 说明：关键字与 ID 的正则式完全重合，因此不建自动机，           */\n"
        "/*       而是在识别出标识符后查表改判；专用符号同理。               */\n"
        "/* ------------------------------------------------------------------ */\n");

    // 关键字表
    code += QStringLiteral("static const char* g_keywordTable[][2] = {\n");
    for (auto it = keywords.constBegin(); it != keywords.constEnd(); ++it) {
        code += QStringLiteral("    { \"%1\", \"%2\" },\n")
                    .arg(cppEscape(it.key()))
                    .arg(it.value());
    }
    code += QStringLiteral("};\n");
    code += QStringLiteral("static const int g_keywordCount = %1;\n\n").arg(keywords.size());

    // 符号表（全部字面单词，便于收尾时查编码）
    code += QStringLiteral("static const char* g_literalTable[][2] = {\n");
    for (auto it = literals.constBegin(); it != literals.constEnd(); ++it) {
        code += QStringLiteral("    { \"%1\", \"%2\" },\n")
                    .arg(cppEscape(it.key()))
                    .arg(it.value());
    }
    code += QStringLiteral("};\n");
    code += QStringLiteral("static const int g_literalCount = %1;\n\n").arg(literals.size());

    code += QStringLiteral(
        "/* 功能：在关键字表中查找标识符，命中则返回其编码。\n"
        " * 输入：str —— 标识符原文。\n"
        " * 输出：关键字编码；不是关键字时返回 -1。                  */\n"
        "static int findKeyword(const char* str) {\n"
        "    for (int i = 0; i < g_keywordCount; i++) {\n"
        "        if (strcmp(str, g_keywordTable[i][0]) == 0)\n"
        "            return atoi(g_keywordTable[i][1]);\n"
        "    }\n"
        "    return -1;\n"
        "}\n\n"
        "/* 功能：在字面单词表中查找符号原文，命中则返回其编码。\n"
        " * 输入：str —— 符号原文。\n"
        " * 输出：符号编码；未命中时返回 -1。                  */\n"
        "static int findLiteral(const char* str) {\n"
        "    for (int i = 0; i < g_literalCount; i++) {\n"
        "        if (strcmp(str, g_literalTable[i][0]) == 0)\n"
        "            return atoi(g_literalTable[i][1]);\n"
        "    }\n"
        "    return -1;\n"
        "}\n\n");
    return code;
}

/**
 * 功能：生成方法一（Judgechar/GetNext/GetToken）的状态分派代码。
 * 输入：dfa —— DFA；idCode —— ID 的编码。
 * 输出：C++ 代码片段。
 * 注意：本方法用一个"由状态决定其接受信息"的函数把两个数组关联起来，
 *       等价于讲稿里 GetToken() 内按当前状态分派的做法。
 */
QString LexCodeGen::emitMethodOne(const Dfa &dfa, int idCode) const
{
    Q_UNUSED(idCode);

    QString code;
    code += QStringLiteral(
        "/* ------------------------------------------------------------------ */\n"
        "/* 转换方法一：Judgechar / GetNext / GetToken 三函数式。              */\n"
        "/*   Judgechar(c) —— 判断下一个字符能否使自动机继续前进；            */\n"
        "/*   GetNext()    —— 取下一个字符，累积到当前单词缓冲区；            */\n"
        "/*   GetToken()   —— 驱动自动机跑完一个单词并回退到最近接受位置。     */\n"
        "/* ------------------------------------------------------------------ */\n\n");

    // 状态常量与字符列号
    code += emitStateConstants(dfa);
    code += QStringLiteral("\n/* 状态 → 是否为接受状态 */\nstatic const int g_accept[%1] = {\n")
                .arg(dfa.stateCount);
    code += QStringLiteral("    ");
    for (int s = 0; s < dfa.stateCount; ++s) {
        code += QStringLiteral("%1%2").arg(dfa.accepting.value(s, false) ? 1 : 0)
                    .arg(s + 1 < dfa.stateCount ? QStringLiteral(",") : QString());
        if ((s + 1) % 24 == 0 && s + 1 < dfa.stateCount)
            code += QStringLiteral("\n    ");
    }
    code += QStringLiteral("\n};\n\n");

    code += QStringLiteral("/* 状态 → 该接受状态输出的编码（-1 表示不输出，如注释） */\n"
                           "static const int g_acceptCode[%1] = {\n    ")
                .arg(dfa.stateCount);
    for (int s = 0; s < dfa.stateCount; ++s) {
        const bool accepting = dfa.accepting.value(s, false);
        const bool comment = dfa.acceptIsComment.value(s, false);
        // 注意：此处局部变量不能叫 code —— 外层已有 QString code 累积生成结果，
        //       同名会遮蔽它，导致 "code += QString" 报类型错误。
        const int outCode = accepting ? (comment ? -1 : dfa.acceptCode.value(s, -1)) : -1;
        code += QString::number(outCode);
        code += (s + 1 < dfa.stateCount) ? QStringLiteral(",") : QString();
        if ((s + 1) % 24 == 0 && s + 1 < dfa.stateCount)
            code += QStringLiteral("\n    ");
    }
    code += QStringLiteral("\n};\n\n");

    code += QStringLiteral("/* 状态转换表：g_trans[状态][字母表列号] = 目的状态，-1 表示无转移。\n"
                           " * 列号与字母表的对应关系见下面的 charIndex()；按字符类模式时\n"
                           " * 每一列是一个字符类（letter、digit、+ 等），而不是单个字节。 */\n"
                           "static const int g_trans[%1][%2] = {\n")
                .arg(dfa.stateCount)
                .arg(qMax(1, static_cast<int>(dfa.alphabet.size())));
    for (int s = 0; s < dfa.stateCount; ++s) {
        code += QStringLiteral("    { ");
        for (int k = 0; k < dfa.alphabet.size(); ++k) {
            const auto it = dfa.trans.at(s).constFind(dfa.alphabet.at(k));
            code += QString::number(it == dfa.trans.at(s).constEnd() ? kNone : it.value());
            if (k + 1 < dfa.alphabet.size())
                code += QStringLiteral(",");
        }
        code += QStringLiteral(" },\n");
    }
    code += QStringLiteral("};\n\n");

    code += QStringLiteral("/* 功能：把字符映射为状态转换表的列号。\n"
                           " * 输入：c —— 待判定字符。\n"
                           " * 输出：列号；该字符不属于字母表里任何一列时返回 -1。\n"
                           " * 说明：按字符类模式时，每一条 if 判断的是\"这个字节属于哪个字符类\"\n"
                           " *       （一次覆盖一个区间，而不是 256 条等值比较）。            */\n"
                           "static int charIndex(unsigned char c) {\n");
    code += emitCharIndexChain(dfa.alphabet, dfa.alphabetInfo);
    code += QStringLiteral("    return -1;\n}\n\n");

    return code;
}

/**
 * 功能：生成方法二（switch(state) 内嵌 switch(ch)）的状态分派代码。
 * 输入：dfa —— DFA；idCode —— ID 的编码。
 * 输出：C++ 代码片段。
 * 注意：本方法把转移关系直接展开成双层 switch，不需要转移表；
 *       接受状态与输出编码也都用 switch 分派。
 */
QString LexCodeGen::emitMethodTwo(const Dfa &dfa, int idCode) const
{
    Q_UNUSED(idCode);

    QString code;
    code += QStringLiteral(
        "/* ------------------------------------------------------------------ */\n"
        "/* 转换方法二：switch (state) 内嵌 switch (ch)。                      */\n"
        "/*   外层按 DFA 状态分派，内层按输入字符分派；转移关系全部展开成       */\n"
        "/*   switch 分支，因此不需要额外的状态转换表。                        */\n"
        "/* ------------------------------------------------------------------ */\n\n");

    code += emitStateConstants(dfa);
    code += QStringLiteral("\n");

    code += QStringLiteral("/* 状态 → 该接受状态输出的编码（-1 表示非接受态或注释） */\n"
                           "static int acceptCodeOf(int state) {\n    switch (state) {\n");
    for (int s = 0; s < dfa.stateCount; ++s) {
        const bool accepting = dfa.accepting.value(s, false);
        if (!accepting)
            continue;
        const bool comment = dfa.acceptIsComment.value(s, false);
        // 同样避免用 code 作局部名（见 emitMethodOne 中的说明）
        const int outCode = comment ? -1 : dfa.acceptCode.value(s, -1);
        code += QStringLiteral("    case S%1: return %2;\n").arg(s).arg(outCode);
    }
    code += QStringLiteral("    default: return -1;\n    }\n}\n\n");

    // 为节省篇幅并保持可读性，这里为每个状态生成一个 case，内层顺序比较各符号
    code += QStringLiteral(
        "/* 功能：按当前状态与字符求下一个状态。\n"
        " * 输入：state —— 当前状态；ch —— 输入字符。\n"
        " * 输出：目的状态；无转移时返回 -1。\n"
        " * 说明：外层 switch 分派状态，内层逐个比较该状态有转移的符号。\n"
        " */\n"
        "static int nextState(int state, unsigned char ch) {\n"
        "    switch (state) {\n");

    for (int s = 0; s < dfa.stateCount; ++s) {
        code += QStringLiteral("    case S%1:\n").arg(s);
        if (dfa.trans.at(s).isEmpty()) {
            code += QStringLiteral("        return -1;\n");
            continue;
        }
        for (auto it = dfa.trans.at(s).constBegin(); it != dfa.trans.at(s).constEnd(); ++it) {
            if (it.key().isEmpty())
                continue;
            if (!dfa.alphabetInfo.isEmpty()) {
                // 字符类模式：按类判定（一个类可能覆盖上百个字节，不能只比一个值）
                const CharClassInfo *cls = findClassByKey(dfa.alphabetInfo, it.key());
                if (cls == nullptr)
                    continue;
                code += QStringLiteral("        if (%1) return S%2;    /* %3 */\n")
                            .arg(emitClassTest(*cls, QStringLiteral("ch")))
                            .arg(it.value())
                            .arg(cls->name);
                continue;
            }
            code += QStringLiteral("        if (ch == %1) return S%2;\n")
                        .arg(cppByteLiteral(byteOfSymbol(it.key())))
                        .arg(it.value());
        }
        code += QStringLiteral("        return -1;\n");
    }
    code += QStringLiteral("    default: return -1;\n    }\n}\n\n");
    return code;
}

/**
 * 功能：生成完整的词法分析源程序。
 * 输入：见头文件说明。
 * 输出：C++ 源程序文本；失败时返回空串。
 * 注意：
 *  1. 生成的文件是**自包含**的（只用 C 标准库），因此可以用 gcc/g++ 直接编译；
 *  2. 运行时用法：lexer <源程序路径> <输出的 .lex 文件路径>；
 *     若省略第 2 个参数则输出到标准输出；
 *  3. 单词切分遵循最长匹配：自动机一直前进到无法前进为止，
 *     再回退到**最近一次处于接受状态的位置**；
 *  4. 回退到的接受状态若是注释，则丢弃这段文本（不输出编码）并从该位置继续；
 *  5. 识别出 ID 后若在关键字表中命中，则改判为关键字编码。
 */
QString LexCodeGen::generate(const Dfa &dfa, const QMap<QString, int> &literals,
                             const QMap<QString, int> &keywords, int idCode, Method method,
                             Layout layout, const QString &programName)
{
    m_error.clear();

    if (dfa.stateCount <= 0) {
        m_error = QStringLiteral("DFA 为空（状态数为 0），无法生成词法分析程序。");
        return QString();
    }
    if (dfa.alphabet.isEmpty()) {
        m_error = QStringLiteral("DFA 的字母表为空，无法生成词法分析程序。");
        return QString();
    }
    if (dfa.trans.size() != dfa.stateCount) {
        m_error = QStringLiteral("DFA 状态转换表与状态数不一致（%1 vs %2）。")
                      .arg(dfa.trans.size())
                      .arg(dfa.stateCount);
        return QString();
    }

    const bool oneLine = (layout == Layout::TaskbookSample);
    const QString methodName = (method == Method::JudgeChar)
                                   ? QStringLiteral("方法一：Judgechar/GetNext/GetToken")
                                   : QStringLiteral("方法二：switch(state)");

    QString code;

    // ---- 文件头 ----
    code += QStringLiteral(
        "/******************************************************************************\n"
        " * (1) 版权信息 : 编译原理课程设计 - 华南师范大学计算机学院\n"
        " * (2) 文件名称 : %1_lexer.cpp\n"
        " *     标识符   : %1 语言词法分析程序\n"
        " *     摘要     : 本文件由《编译原理课程设计》系统自动生成，请勿手工修改。\n"
        " *                生成依据：由 %1 语言全部单词正则式经\n"
        " *                正则式 → NFA（Thompson 构造）→ DFA（子集构造法）→\n"
        " *                最小化 DFA（划分法）所得的 %2 状态最小化 DFA。\n"
        " *                转换方法：%3。\n"
        " * (3) 版本号   : 1.0.0       作者/修改者 : 自动生成   完成日期 : 自动\n"
        " * (4) 版本历史 : 1.0.0  由系统生成\n"
        " *\n"
        " * 运行方式：%1_lexer <源程序文件> [输出的 .lex 文件]\n"
        " *           省略第 2 个参数时把单词编码输出到标准输出。\n"
        " * 输出格式：<编码> <值>，每行一个单词；\n"
        " *           ID 的值带双引号，NUM 的值保留原文（含前导零），\n"
        " *           关键字与专用符号只输出编码；注释不输出任何内容；\n"
        " *           文件末尾输出一行 EOF。\n"
        " *****************************************************************************/\n\n")
                .arg(programName, QString::number(dfa.stateCount), methodName);

    // ---- 头文件与全局 ----
    code += QStringLiteral(
        "#include <stdio.h>\n"
        "#include <stdlib.h>\n"
        "#include <string.h>\n\n"
        "/* 单词缓冲区容量：取足够大以容纳最长可能单词 */\n"
        "#define WORD_MAX 4096\n\n"
        "static FILE* g_out = NULL;      /* .lex 输出文件 */\n"
        "static int   g_idCode = %1;     /* ID 的编码，用于判断是否需要查关键字表 */\n"
        "static const int g_oneLine = %2;/* 输出排版：1=任务书示例（拼成一行） */\n\n")
                .arg(idCode)
                .arg(oneLine ? 1 : 0);

    // ---- 字面单词表 ----
    code += emitLiteralTables(literals, keywords);

    // ---- 状态分派（按所选方法）----
    code += (method == Method::JudgeChar) ? emitMethodOne(dfa, idCode)
                                          : emitMethodTwo(dfa, idCode);

    // ---- 驱动程序 ----
    code += QStringLiteral(
        "/* ------------------------------------------------------------------ */\n"
        "/* 驱动逻辑：最长匹配 + 回退 + 按格式输出                              */\n"
        "/* ------------------------------------------------------------------ */\n\n"
        "/* 功能：把单词原文与编码按 AGENT.md 4.3 节的格式写出。\n"
        " * 输入：code —— 单词编码；text —— 单词原文；length —— 原文长度。\n"
        " * 输出：无。\n"
        " * 说明：ID（编码等于 g_idCode）的值带双引号；\n"
        " *       NUM（编码 100）输出原文以保留前导零；\n"
        " *       其它（关键字/专用符号）只输出编码。                       */\n"
        "static void emitToken(int code, const char* text, int length) {\n"
        "    const char* sep = g_oneLine ? \" \" : \"\\n\";\n"
        "    if (code == g_idCode) {\n"
        "        /* 标识符：先查关键字表，命中则改判为关键字编码 */\n"
        "        char word[WORD_MAX];\n"
        "        int n = (length < WORD_MAX - 1) ? length : WORD_MAX - 1;\n"
        "        memcpy(word, text, (size_t)n);\n"
        "        word[n] = '\\0';\n"
        "        const int keywordCode = findKeyword(word);\n"
        "        if (keywordCode >= 0) {\n"
        "            fprintf(g_out, \"%%d%%s\", keywordCode, sep);\n"
        "            return;\n"
        "        }\n"
        "        fprintf(g_out, \"%%d \\\"%%s\\\"%%s\", code, word, sep);\n"
        "        return;\n"
        "    }\n"
        "    if (code == 100) {\n"
        "        /* 数字：输出原文，保留前导零 */\n"
        "        char word[WORD_MAX];\n"
        "        int n = (length < WORD_MAX - 1) ? length : WORD_MAX - 1;\n"
        "        memcpy(word, text, (size_t)n);\n"
        "        word[n] = '\\0';\n"
        "        fprintf(g_out, \"%%d %%s%%s\", code, word, sep);\n"
        "        return;\n"
        "    }\n"
        "    /* 关键字与专用符号：只输出编码 */\n"
        "    fprintf(g_out, \"%%d%%s\", code, sep);\n"
        "}\n\n");

    // 两种方法的"取下一个状态"与"是否为接受状态"写法不同
    if (method == Method::JudgeChar) {
        code += QStringLiteral(
            "/* 功能：取当前状态经字符 ch 转移后的状态。\n"
            " * 输入：state —— 当前状态；ch —— 字符。\n"
            " * 输出：目的状态；无转移时返回 -1。                    */\n"
            "static int stepState(int state, unsigned char ch) {\n"
            "    const int column = charIndex(ch);\n"
            "    if (column < 0)\n"
            "        return -1;\n"
            "    return g_trans[state][column];\n"
            "}\n\n"
            "/* 功能：判断该状态是否为接受状态。\n"
            " * 输入：state —— 状态。输出：接受时为 1。\n"
            " * 注意：用独立的 g_accept 表判断，**不能**用“编码是否为 -1”代替 ——\n"
            " *       注释状态也是接受状态，只是它不输出编码。\n"
            " */\n"
            "static int isAcceptState(int state) {\n"
            "    return (state >= 0) ? g_accept[state] : 0;\n"
            "}\n\n"
            "/* 功能：取该接受状态应输出的编码（-1 表示不输出，如注释）。  */\n"
            "static int acceptCodeOf(int state) {\n"
            "    return (state >= 0) ? g_acceptCode[state] : -1;\n"
            "}\n\n");
    } else {
        code += QStringLiteral(
            "/* 功能：取当前状态经字符 ch 转移后的状态。\n"
            " * 输入：state —— 当前状态；ch —— 字符。\n"
            " * 输出：目的状态；无转移时返回 -1。                    */\n"
            "static int stepState(int state, unsigned char ch) {\n"
            "    return nextState(state, ch);\n"
            "}\n\n"
            "/* 功能：判定状态是否为接受状态。\n"
            " * 输入：state —— 状态。输出：接受时为 1。\n"
            " * 注意：**不能用“编码是否为 -1”代替本判断**——注释状态也是接受状态，\n"
            " *       只是它不输出编码。若漏掉注释，最长匹配就不会记下注释的结束位置，\n"
            " *       注释内容会被逐字符当成符号切碎（本项目实际踩过这个坑）。\n"
            " */\n"
            "static int isAcceptState(int state) {\n"
            "    switch (state) {\n");
        for (int s = 0; s < dfa.stateCount; ++s) {
            if (!dfa.accepting.value(s, false))
                continue;
            code += QStringLiteral("    case S%1: return 1;\n").arg(s);
        }
        code += QStringLiteral("    default: return 0;\n    }\n}\n\n");
    }

    // 主循环
    code += QStringLiteral(
        "/* 功能：读取源程序文本，逐个识别单词并按格式输出。\n"
        " * 输入：text —— 源程序全文；length —— 全文长度。\n"
        " * 输出：无。\n"
        " * 说明：核心是最长匹配——先沿自动机一直前进，记录最近一次\n"
        " *       处于接受状态的位置；无法前进时回退到该位置输出单词，\n"
        " *       再从该位置继续扫描。若最近接受位置是注释，则丢弃这段\n"
        " *       文本（不输出编码）。完全无法识别时报告词法错误。       */\n"
        "static void scan(const char* text, int length) {\n"
        "    int pos = 0;\n"
        "    /* 跳过 UTF-8 BOM（EF BB BF）：源程序常由记事本等工具保存，\n"
        "     * 带 BOM 时若不当成空白跳过，首个单词会被 BOM 字节污染\n"
        "     * （本项目实际踩过：'read' 被识别成 'ead'）。 */\n"
        "    if (length >= 3 && (unsigned char)text[0] == 0xEF\n"
        "        && (unsigned char)text[1] == 0xBB && (unsigned char)text[2] == 0xBF)\n"
        "        pos = 3;\n"
        "    while (pos < length) {\n"
        "        /* 跳过空白字符（空格、制表、回车、换行） */\n"
        "        const unsigned char first = (unsigned char)text[pos];\n"
        "        if (first == ' ' || first == '\\t' || first == '\\r' || first == '\\n') {\n"
        "            pos++;\n"
        "            continue;\n"
        "        }\n\n"
        "        int state = S0;          /* DFA 初态 */\n"
        "        int lastAcceptCode = -1; /* 最近一次接受状态应输出的编码 */\n"
        "        int lastAcceptPos = pos; /* 最近一次接受状态对应的输入位置 */\n"
        "        int p = pos;\n\n"
        "        while (p < length) {\n"
        "            const int next = stepState(state, (unsigned char)text[p]);\n"
        "            if (next < 0)\n"
        "                break;           /* 无法继续前进，结束本单词 */\n"
        "            state = next;\n"
        "            p++;\n"
        "            if (isAcceptState(state)) {\n"
        "                lastAcceptCode = acceptCodeOf(state);\n"
        "                lastAcceptPos = p;\n"
        "            }\n"
        "        }\n\n"
        "        if (lastAcceptPos == pos) {\n"
        "            /* 一个字符都没能识别：词法错误 */\n"
        "            fprintf(stderr, \"词法错误：位置 %%d 处的字符 '%%c'（ASCII %%d）无法识别。\\n\",\n"
        "                    pos, text[pos], (int)(unsigned char)text[pos]);\n"
        "            pos++;\n"
        "            continue;\n"
        "        }\n\n"
        "        if (lastAcceptCode < 0) {\n"
        "            /* 回退到的是注释：参与自动机但不输出编码，丢弃后继续扫描 */\n"
        "            pos = lastAcceptPos;\n"
        "            continue;\n"
        "        }\n\n"
        "        emitToken(lastAcceptCode, text + pos, lastAcceptPos - pos);\n"
        "        pos = lastAcceptPos;\n"
        "    }\n"
        "    /* 拼成一行排版时，单词之间用空格分隔且不换行，\n"
        "     * 因此 EOF 必须另起一行；逐行排版时直接跟在最后一个换行之后。 */\n"
        "    if (g_oneLine)\n"
        "        fprintf(g_out, \"\\nEOF\\n\");\n"
        "    else\n"
        "        fprintf(g_out, \"EOF\\n\");\n"
        "}\n\n"
        "/* 功能：程序入口。\n"
        " * 输入：argv[1] 源程序路径；argv[2] 可选，输出的 .lex 文件路径。\n"
        " * 输出：成功返回 0。                                              */\n"
        "int main(int argc, char** argv) {\n"
        "    if (argc < 2) {\n"
        "        fprintf(stderr, \"用法：%%s <源程序文件> [输出的 .lex 文件]\\n\", argv[0]);\n"
        "        return 1;\n"
        "    }\n\n"
        "    FILE* in = fopen(argv[1], \"rb\");\n"
        "    if (in == NULL) {\n"
        "        fprintf(stderr, \"无法打开源程序文件：%%s\\n\", argv[1]);\n"
        "        return 2;\n"
        "    }\n"
        "    if (argc >= 3) {\n"
        "        g_out = fopen(argv[2], \"wb\");\n"
        "        if (g_out == NULL) {\n"
        "            fprintf(stderr, \"无法创建输出文件：%%s\\n\", argv[2]);\n"
        "            fclose(in);\n"
        "            return 3;\n"
        "        }\n"
        "    } else {\n"
        "        g_out = stdout;\n"
        "    }\n\n"
        "    /* 一次性读入全文（课程设计的测试源程序规模很小，无需分块） */\n"
        "    static char buffer[1024 * 1024];\n"
        "    const size_t read = fread(buffer, 1, sizeof(buffer) - 1, in);\n"
        "    buffer[read] = '\\0';\n"
        "    fclose(in);\n\n"
        "    scan(buffer, (int)read);\n\n"
        "    if (g_out != stdout)\n"
        "        fclose(g_out);\n"
        "    return 0;\n"
        "}\n");

    // 收尾：把模板里为**可读性**写的 "%%" 还原成单个 "%"。
    // 缘由：生成模板本身是 QStringLiteral 字面量，无需为 printf 转义百分号；
    //       但模板里的 %1/%2 占位符又不能与 "%d" 混淆，因此模板中统一写成 "%%d%%s"
    //       再在此处统一还原，既保证生成结果正确，又让模板一眼可读。
    code.replace(QStringLiteral("%%"), QStringLiteral("%"));

    LOG_INFO(QStringLiteral("已生成词法分析源程序（%1，%2 个状态、字母表 %3 个%4，%5 行代码）。")
                 .arg(methodName)
                 .arg(dfa.stateCount)
                 .arg(dfa.alphabet.size())
                 .arg(dfa.alphabetInfo.isEmpty() ? QStringLiteral("符号")
                                                 : QStringLiteral("字符类"))
                 .arg(code.count(QLatin1Char('\n')) + 1));
    return code;
}

/**
 * 功能：生成"程序使用说明"文本。
 * 输入：dfa —— DFA。
 * 输出：说明文本（供界面⑤显示）。
 * 注意：内容为面向使用者的操作步骤，不含算法细节。
 */
QString LexCodeGen::usageNotes(const Dfa &dfa)
{
    return QStringLiteral(
               "生成的词法分析程序特性：\n"
               "  1. 依据：由 %1 个状态的最小化 DFA 生成（任务书要求 1.7）；\n"
               "  2. 单词切分：最长匹配（maximal munch），无法前进时回退到最近接受位置；\n"
               "  3. 关键字：识别出标识符后查关键字表改判（关键字不单独建自动机）；\n"
               "  4. 注释：参与自动机但不输出编码；\n"
               "  5. 输出：按 AGENT.md 4.3 节格式写 <编码> <值>，注释无输出，末尾 EOF；\n"
               "  6. 自包含：只用 C 标准库，可用 gcc/g++ 直接编译，不依赖 Qt。\n\n"
               "运行方式：\n"
               "  <生成的可执行文件> <源程序文件> [输出的 .lex 文件]\n"
               "  省略第 2 个参数时把单词编码输出到标准输出。\n")
        .arg(dfa.stateCount);
}

}  // namespace ccd
