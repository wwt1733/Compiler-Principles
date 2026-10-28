/******************************************************************************
 * (1) 版权信息 : 编译原理课程设计 - 华南师范大学计算机学院
 * (2) 文件名称 : LanguageConfig.cpp
 *     标识符   : LanguageConfigRegistry
 *     摘要     : 语言配置表的实现。关键字列表与 AGENT.md §6 的编码表严格对应，
 *                顺序即"连续编码"的分配顺序，不得随意调整。
 * (3) 版本号   : 1.0.0       作者/修改者 : <学号 姓名>   完成日期 : 2026-XX-XX
 * (4) 版本历史 : 1.0.0  2026-XX-XX  首次实现
 *****************************************************************************/

#include "common/LanguageConfig.h"

namespace ccd {

namespace {

/**
 * 功能：构建全部语言配置表。
 * 输入：无。
 * 输出：按 TINY、mini-C 顺序排列的配置列表。
 * 注意：
 *  1. TINY 的关键字编码自 103 起连续分配（if=103 … write=110），
 *     与 AGENT.md §6.1 一致，顺序不可调整；
 *  2. mini-C 的关键字编码自 103 起连续分配（else=103 … for=111），
 *     与 AGENT.md §6.2 一致；
 *  3. TINY 不区分大小写（单词表明确说明），mini-C 区分大小写。
 */
QVector<LanguageConfig> buildConfigs()
{
    QVector<LanguageConfig> configs;

    // ---------------- TINY 语言 ----------------
    LanguageConfig tiny;
    tiny.id = LanguageId::Tiny;
    tiny.key = QStringLiteral("tiny");
    tiny.displayName = QStringLiteral("TINY 语言");
    tiny.wordsFile = QStringLiteral("tiny_words.txt");
    tiny.grammarFile = QStringLiteral("tiny_grammar.txt");
    tiny.grammarFixedFile = QString();
    tiny.semanticsFile = QStringLiteral("tiny_semantics.txt");
    tiny.sampleFile = QStringLiteral("sample.tny");
    tiny.keywords = QStringList{
        QStringLiteral("if"),     QStringLiteral("then"),   QStringLiteral("else"),
        QStringLiteral("end"),    QStringLiteral("repeat"), QStringLiteral("until"),
        QStringLiteral("read"),   QStringLiteral("write"),
    };
    configs.append(tiny);

    // ---------------- mini-C 语言 ----------------
    LanguageConfig minic;
    minic.id = LanguageId::MiniC;
    minic.key = QStringLiteral("minic");
    minic.displayName = QStringLiteral("mini-C 语言");
    minic.wordsFile = QStringLiteral("minic_words.txt");
    minic.grammarFile = QStringLiteral("minic_grammar.txt");
    // mini-C 文法只保留一份（data/minic_grammar.txt，采用老师发布版本，
    // 含 dowhile-stmt 产生式）；任务书原文版另存为 minic_grammar_taskbook.txt，
    // 用于在测试报告中做对比实验，故此处不设置 grammarFixedFile。
    minic.grammarFixedFile = QString();
    minic.semanticsFile = QStringLiteral("minic_semantics.txt");
    minic.sampleFile = QStringLiteral("sample.mc");
    // 关键字顺序必须与 data/minic_words.txt 中 Keyword103L 的书写顺序**逐字一致**，
    // 因为该顺序决定连续编码的分配（103 else、104 if、…、111 while、112 for）。
    // 注意：老师随堂发布的单词表顺序为 …, return, void, do, while, for，
    //       与任务书 PDF 的 "…return void while for"（漏印 do）不同，
    //       此处以老师发布的单词表为准（见 AGENT.md §6.2 注 5）。
    minic.keywords = QStringList{
        QStringLiteral("else"),   QStringLiteral("if"),     QStringLiteral("int"),
        QStringLiteral("float"),  QStringLiteral("double"), QStringLiteral("return"),
        QStringLiteral("void"),   QStringLiteral("do"),     QStringLiteral("while"),
        QStringLiteral("for"),
    };
    configs.append(minic);

    return configs;
}

/**
 * 功能：构建 TINY 的"单词编码 → 文法终结符名"对照表。
 * 输入：无。
 * 输出：编码到终结符名的映射。
 * 注意：
 *  1. 关键字直接用其小写原文作终结符名（文法里就写 if/then/else/…）；
 *  2. 关系与赋值运算符沿用任务书 BNF 的写法：
 *       <  → LT      <= → LTEQ    <> → NE
 *       >  → RT      >= → RTEQ    =  → EQ
 *     说明：任务书 BNF 用 RT 表示 ">"（不是 GT），本表与 `data/tiny_grammar.txt`
 *     的书写保持逐字一致，改一处必须改另一处；
 *  3. 注释 102 **不列入** —— 它不输出编码，不会出现在 .lex 里。
 */
QMap<int, QString> buildTinyTerminalNames()
{
    QMap<int, QString> names;
    // 数字与标识符
    names.insert(100, QStringLiteral("NUMBER"));
    names.insert(101, QStringLiteral("ID"));
    // 关键字
    names.insert(103, QStringLiteral("if"));
    names.insert(104, QStringLiteral("then"));
    names.insert(105, QStringLiteral("else"));
    names.insert(106, QStringLiteral("end"));
    names.insert(107, QStringLiteral("repeat"));
    names.insert(108, QStringLiteral("until"));
    names.insert(109, QStringLiteral("read"));
    names.insert(110, QStringLiteral("write"));
    // 专用符号
    names.insert(200, QStringLiteral("PLUS"));
    names.insert(201, QStringLiteral("MINUS"));
    names.insert(202, QStringLiteral("MULTIPLY"));
    names.insert(203, QStringLiteral("DIVIDE"));
    names.insert(204, QStringLiteral("MOD"));
    names.insert(205, QStringLiteral("POWER"));
    names.insert(206, QStringLiteral("LT"));
    names.insert(207, QStringLiteral("LTEQ"));
    names.insert(208, QStringLiteral("NE"));
    names.insert(209, QStringLiteral("RT"));
    names.insert(210, QStringLiteral("RTEQ"));
    names.insert(211, QStringLiteral("EQ"));
    names.insert(212, QStringLiteral("LBM"));
    names.insert(213, QStringLiteral("RBM"));
    names.insert(214, QStringLiteral("SEMI"));
    names.insert(215, QStringLiteral("ASSIGN"));
    names.insert(216, QStringLiteral("LPAR"));
    names.insert(217, QStringLiteral("RPAR"));
    return names;
}

/**
 * 功能：构建 mini-C 的"单词编码 → 文法终结符名"对照表。
 * 输入：无。
 * 输出：编码到终结符名的映射。
 * 注意：
 *  1. 本表与 `data/minic_grammar.txt` 里的书写**逐字一致**；
 *  2. mini-C 的圆括号/方括号/花括号/逗号用了历史命名
 *     LMM/RMM（圆）、LLM/RLM（方）、LBM/RBM（花）、DOU（逗号），
 *     这些名字来自同课程历史项目，本表沿用以免与参考数据对照时混淆；
 *  3. 注释 102 不列入；`//`（225）已被注释吸收、`_`（200）永不输出，
 *     二者均不列入。
 */
QMap<int, QString> buildMinicTerminalNames()
{
    QMap<int, QString> names;
    // 数字与标识符
    names.insert(100, QStringLiteral("NUM"));
    names.insert(101, QStringLiteral("ID"));
    // 关键字
    names.insert(103, QStringLiteral("else"));
    names.insert(104, QStringLiteral("if"));
    names.insert(105, QStringLiteral("int"));
    names.insert(106, QStringLiteral("float"));
    names.insert(107, QStringLiteral("double"));
    names.insert(108, QStringLiteral("return"));
    names.insert(109, QStringLiteral("void"));
    names.insert(110, QStringLiteral("do"));
    names.insert(111, QStringLiteral("while"));
    names.insert(112, QStringLiteral("for"));
    // 专用符号
    names.insert(201, QStringLiteral("PLUS"));
    names.insert(202, QStringLiteral("MINUS"));
    names.insert(203, QStringLiteral("MULTIPLY"));
    names.insert(204, QStringLiteral("DIVIDE"));
    names.insert(206, QStringLiteral("POWER"));
    names.insert(207, QStringLiteral("DOT"));
    names.insert(208, QStringLiteral("LT"));
    names.insert(209, QStringLiteral("LTEQ"));
    names.insert(210, QStringLiteral("RT"));
    names.insert(211, QStringLiteral("RTEQ"));
    names.insert(212, QStringLiteral("EQ"));
    names.insert(213, QStringLiteral("NE"));
    names.insert(214, QStringLiteral("ASSIGN"));
    names.insert(215, QStringLiteral("SEMI"));
    names.insert(216, QStringLiteral("DOU"));     // ,
    names.insert(217, QStringLiteral("INC"));     // ++
    names.insert(218, QStringLiteral("DEC"));     // --
    // 括号家族的名字必须与**文法**的用法一致，否则语法分析会整片失败：
    //   圆括号 `(` / `)` —— 函数定义、函数调用、条件、循环、带括号表达式，
    //                      文法里写作 LLM / RLM
    //   方括号 `[` / `]` —— 数组声明、数组下标，
    //                      文法里写作 LMM / RMM
    // 依据：老师参考数据 `09_minic_words.txt` 用"符号行 + 名字行"逐位对应的写法，
    //      `\(` `\)` `\[` `\]` 依次对应 `LLM` `RLM` `LMM` `RMM`（见 AGENT.md §6.2 注 6）。
    // 本项目早期把 219 与 221 的名字写反了，后果是 `int f(int x){}` 这类函数定义
    // 在语法分析时按 `LMM`（方括号）去找 `(`，整片报错 —— 这是阶段 7 联调
    // 才暴露出来的真实缺陷，不是笔误级别的注释问题。
    names.insert(219, QStringLiteral("LLM"));     // (
    names.insert(220, QStringLiteral("RLM"));     // )
    names.insert(221, QStringLiteral("LMM"));     // [
    names.insert(222, QStringLiteral("RMM"));     // ]
    names.insert(223, QStringLiteral("LBM"));     // {
    names.insert(224, QStringLiteral("RBM"));     // }
    names.insert(226, QStringLiteral("MOD"));     // %
    names.insert(227, QStringLiteral("NOT"));     // !
    // 小数（`Num_205=digit+ \. digit+`）也归到 NUM。
    // mini-C 文法里数字只有一个终结符 `NUM`（产生式 `factor -> NUM`），
    // int 与 float 字面量在语法上不做区分（`type-indicator -> int|float|double`
    // 才是区分点），因此小数与整数共用同一个终结符名。
    // 若不登记 205，小数在语法分析时就会变成"查不到名字的编码"而整片报错。
    names.insert(205, QStringLiteral("NUM"));     // 小数（与整数 100 同名）
    return names;
}

/**
 * 功能：取某语言的"终结符名 → 编码"逆表（惰性构建并缓存）。
 * 输入：id —— 语言标识。
 * 输出：逆表常引用。
 * 注意：1. 逆表与正表一一对应，构建一次后常驻，避免每次翻译都线性扫描；
 *       2. **同一个名字可能对应多个编码**（例如 mini-C 的 `NUM` 同时对应
 *          整数 100 与小数 205）。此时取**编码较小**的那个作为规范值，
 *          因此这里"已存在就不覆盖"。QMap 按编码升序迭代，最小值先到；
 *          若反过来让后来的覆盖前面的，`NUM` 就会变成 205，
 *          生成词法分析程序时按名字取编码就会取错（AGENT.md §13.21）。
 */
const QMap<QString, int> &reverseTerminalNames(LanguageId id)
{
    static QMap<QString, int> tinyReverse;
    static QMap<QString, int> minicReverse;
    static bool tinyBuilt = false;
    static bool minicBuilt = false;

    QMap<QString, int> &target = (id == LanguageId::MiniC) ? minicReverse : tinyReverse;
    bool &built = (id == LanguageId::MiniC) ? minicBuilt : tinyBuilt;
    if (!built) {
        const QMap<int, QString> &forward = LanguageConfigRegistry::terminalNames(id);
        for (auto it = forward.constBegin(); it != forward.constEnd(); ++it) {
            if (!target.contains(it.value()))
                target.insert(it.value(), it.key());
        }
        built = true;
    }
    return target;
}

}  // namespace

// ---------------------------------------------------------------------------
// 注册表
// ---------------------------------------------------------------------------

/**
 * 功能：取全部语言配置。
 * 输入：无。
 * 输出：静态配置列表的常引用（进程内只构造一次）。
 * 注意：返回顺序固定为 TINY、mini-C，界面下拉框直接按下标使用。
 */
const QVector<LanguageConfig> &LanguageConfigRegistry::all()
{
    static const QVector<LanguageConfig> s_configs = buildConfigs();
    return s_configs;
}

/**
 * 功能：取指定语言的配置。
 * 输入：id —— 语言标识。
 * 输出：配置的常引用。
 * 注意：越界时返回 TINY 配置（不抛异常，保证界面稳健）。
 */
const LanguageConfig &LanguageConfigRegistry::get(LanguageId id)
{
    const QVector<LanguageConfig> &list = all();
    for (const LanguageConfig &cfg : list) {
        if (cfg.id == id)
            return cfg;
    }
    return list.first();
}

/**
 * 功能：由短键取语言标识。
 * 输入：key —— "tiny" 或 "minic"（不区分大小写）。
 * 输出：语言标识；未知键返回 Tiny。
 */
LanguageId LanguageConfigRegistry::fromKey(const QString &key)
{
    const QString lower = key.trimmed().toLower();
    const QVector<LanguageConfig> &list = all();
    for (const LanguageConfig &cfg : list) {
        if (cfg.key == lower)
            return cfg.id;
    }
    return LanguageId::Tiny;
}

/**
 * 功能：取"单词编码 → 文法终结符名"对照表。
 * 输入：id —— 语言标识。
 * 输出：映射常引用（进程内只构建一次）。
 * 注意：返回常引用以避免每次调用都拷贝整张表。
 */
const QMap<int, QString> &LanguageConfigRegistry::terminalNames(LanguageId id)
{
    static const QMap<int, QString> s_tiny = buildTinyTerminalNames();
    static const QMap<int, QString> s_minic = buildMinicTerminalNames();
    return (id == LanguageId::MiniC) ? s_minic : s_tiny;
}

/**
 * 功能：由终结符名取编码。
 * 输入：id —— 语言标识；name —— 终结符名（区分大小写）。
 * 输出：单词编码；名字不在表中时返回 -1。
 * 注意：文法的终结符名必须与此表逐字一致，否则文法里会出现"未定义终结符"，
 *       由 Grammar::load 报错而不是在这里静默返回 -1。
 */
int LanguageConfigRegistry::terminalCode(LanguageId id, const QString &name)
{
    return reverseTerminalNames(id).value(name, -1);
}

}  // namespace ccd
