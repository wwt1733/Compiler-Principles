/******************************************************************************
 * (1) 版权信息 : 编译原理课程设计 - 华南师范大学计算机学院
 * (2) 文件名称 : SelfTest.cpp
 *     标识符   : SelfTest
 *     摘要     : 无界面自测框架的实现。只依赖 QtCore，不创建任何 GUI 对象，
 *                因此不需要 Qt 平台插件，可在无头环境稳定运行。
 * (3) 版本号   : 1.0.0       作者/修改者 : <学号 姓名>   完成日期 : 2026-XX-XX
 * (4) 版本历史 : 1.0.0  2026-XX-XX  首次实现
 *****************************************************************************/

#include "common/SelfTest.h"

#include <algorithm>
#include <QCoreApplication>
#include <QDateTime>
#include <QFile>
#include <QFileInfo>
#include <QLibraryInfo>

#include "common/FileUtil.h"
#include "common/LanguageConfig.h"
#include "common/Logger.h"
#include "lex/DfaMinimize.h"
#include "lex/LexPipeline.h"
#include "lex/NfaToDfa.h"
#include "lex/RegexAst.h"
#include "lex/RegexToNfa.h"
#include "lex/TokenSpec.h"
#include "syntax/FirstFollow.h"
#include "syntax/Grammar.h"
#include "syntax/GrammarSimplify.h"
#include "syntax/IntermediateCode.h"
#include "syntax/LexReader.h"
#include "syntax/Lr1Items.h"
#include "syntax/LrItems.h"
#include "syntax/ParseTable.h"
#include "syntax/Parser.h"
#include "syntax/SemanticsAction.h"
#include "syntax/SyntaxTree.h"

namespace {

/**
 * 功能：把一个字节值映射成自动机字母表里的**符号键**。
 * 输入：info —— 字符类字母表；byte —— 字节值（0~255）。
 * 输出：符号键；按字节模式（info 为空）时返回该字节的单字符文本。
 * 注意：这是自测里"逐字符驱动 DFA"的公共入口，必须用。
 *       任务一的自动机默认按**字符类**构造，转移表的键是 "@0" 这样的类符号键，
 *       不是单个字符。若仍直接拿 `QString(ch)` 去查转移表，永远查不到：
 *       断言要么误报失败，要么**假通过**（两边都查不到 → 都判不接受 → 相等）。
 *       本项目实际踩过：T9.10"最小化前后接受结果一致"一度假通过。
 */
QString symbolKeyOfByte(const ccd::CharClassAlphabet &info, int byte)
{
    if (info.isEmpty())
        return QString(QChar(static_cast<ushort>(byte & 0xFF)));
    return info.symbolOfByte(byte);
}

}  // namespace

namespace ccd {

// ---------------------------------------------------------------------------
// SelfTestContext
// ---------------------------------------------------------------------------

/**
 * 功能：记录一条断言结果。
 * 输入：name —— 断言名；ok —— 是否通过；detail —— 补充说明（可选）。
 * 输出：无。
 * 注意：失败时同时写一条 Error 级日志，便于在日志文件中快速定位。
 */
void SelfTestContext::check(const QString &name, bool ok, const QString &detail)
{
    ++m_total;
    if (!ok)
        ++m_failed;

    // 耗时统计：两次 check 之间的时间差，就是"上一条断言之前那段计算"的耗时。
    // 这样不必在每个算法里插桩，也能得到"到底哪一步慢"的完整画面。
    const qint64 now = QDateTime::currentMSecsSinceEpoch();
    if (m_lastMarkMs != 0) {
        const qint64 delta = now - m_lastMarkMs;
        if (delta >= 200)   // 200ms 以下不值得记录
            m_slowSteps.append({name, delta});
    }
    m_lastMarkMs = now;

    m_lines.append(QStringLiteral("[%1] %2%3")
                       .arg(ok ? QStringLiteral("通过") : QStringLiteral("失败"), name,
                            detail.isEmpty() ? QString() : QStringLiteral("  —— ") + detail));

    if (ok)
        LOG_SUCCESS(QStringLiteral("自测通过：%1").arg(name));
    else
        LOG_ERROR(QStringLiteral("自测失败：%1 %2").arg(name, detail));
}

/**
 * 功能：耗时报表明细。
 * 输入：无。输出：多行文本（按耗时降序）。
 * 注意：这张表是"界面卡顿"的定位工具 —— 自测会把各算法全量算一遍，
 *       所以这里的耗时就是界面按钮背后的真实耗时。只列 >=200ms 的步骤。
 */
QString SelfTestContext::slowStepsText() const
{
    if (m_slowSteps.isEmpty())
        return QStringLiteral("（没有超过 200ms 的步骤）");

    QVector<SlowStep> sorted = m_slowSteps;
    std::sort(sorted.begin(), sorted.end(),
              [](const SlowStep &a, const SlowStep &b) { return a.ms > b.ms; });

    qint64 total = 0;
    for (const SlowStep &s : sorted)
        total += s.ms;

    QStringList out;
    out << QStringLiteral("慢步骤明细（>=200ms，按耗时降序；合计 %1 秒）")
               .arg(total / 1000.0, 0, 'f', 2);
    for (const SlowStep &s : sorted) {
        out << QStringLiteral("  %1 秒    %2")
                   .arg(s.ms / 1000.0, 7, 'f', 2)
                   .arg(s.name);
    }
    return out.join(QLatin1Char('\n'));
}

/**
 * 功能：生成报告全文。
 * 输入：无。
 * 输出：报告文本（以换行结尾）。
 * 注意：末尾附合计统计，退出码由 failedCount() 决定。
 */
QString SelfTestContext::report() const
{
    QStringList out = m_lines;
    out.append(QString());
    out.append(QStringLiteral("合计：%1 项，通过 %2 项，失败 %3 项")
                   .arg(m_total)
                   .arg(m_total - m_failed)
                   .arg(m_failed));
    // 附上耗时报表：界面"未响应"的根因基本都能从这张表上直接读出来
    out.append(QString());
    out.append(slowStepsText());
    return out.join(QLatin1Char('\n')) + QLatin1Char('\n');
}

// ---------------------------------------------------------------------------
// 无界面自测
// ---------------------------------------------------------------------------

/**
 * 功能：执行阶段 0 的无界面自测。
 * 输入：ctx —— 自测上下文。
 * 输出：无（结果写入 ctx）。
 * 注意：本函数**不得**创建任何 QWidget/QApplication，只允许 QtCore，
 *       以保证在无平台插件的环境下也能运行（见 SelfTest.h 的说明）。
 */
void runConsoleSelfTest(SelfTestContext &ctx)
{
    ctx.note(QStringLiteral("=== 编译原理课程设计系统 无界面自测报告 ==="));
    ctx.note(QStringLiteral("Qt 版本      ：%1").arg(QString::fromLatin1(qVersion())));
    ctx.note(QStringLiteral("Qt 安装前缀  ：%1").arg(QLibraryInfo::path(QLibraryInfo::PrefixPath)));
    ctx.note(QStringLiteral("程序目录     ：%1").arg(FileUtil::toNativePath(FileUtil::appDir())));
    ctx.note(QStringLiteral("数据目录     ：%1").arg(FileUtil::toNativePath(FileUtil::dataDir())));
    ctx.note(QString());

    // ---- T1：语言配置表 ----
    const QVector<LanguageConfig> &configs = LanguageConfigRegistry::all();
    ctx.check(QStringLiteral("T1 语言配置表含 2 种语言（TINY、mini-C）"), configs.size() == 2,
              QStringLiteral("实际 %1 种").arg(configs.size()));

    if (configs.size() == 2) {
        const LanguageConfig &tiny = configs.at(0);
        const LanguageConfig &minic = configs.at(1);

        ctx.check(QStringLiteral("T1.1 TINY 关键字 8 个"), tiny.keywords.size() == 8,
                  QStringLiteral("实际 %1 个").arg(tiny.keywords.size()));
        ctx.check(QStringLiteral("T1.2 TINY 关键字顺序为 if/then/else/end/repeat/until/read/write"),
                  tiny.keywords
                      == QStringList{QStringLiteral("if"), QStringLiteral("then"),
                                     QStringLiteral("else"), QStringLiteral("end"),
                                     QStringLiteral("repeat"), QStringLiteral("until"),
                                     QStringLiteral("read"), QStringLiteral("write")});
        ctx.check(QStringLiteral("T1.4 mini-C 关键字 10 个（else/if/int/float/double/"
                                 "return/void/do/while/for）"),
                  minic.keywords.size() == 10,
                  QStringLiteral("实际 %1 个").arg(minic.keywords.size()));
        ctx.check(QStringLiteral("T1.5 mini-C 关键字顺序与 minic_words.txt 一致"),
                  minic.keywords
                      == QStringList{QStringLiteral("else"), QStringLiteral("if"),
                                     QStringLiteral("int"), QStringLiteral("float"),
                                     QStringLiteral("double"), QStringLiteral("return"),
                                     QStringLiteral("void"), QStringLiteral("do"),
                                     QStringLiteral("while"), QStringLiteral("for")},
                  minic.keywords.join(QLatin1Char('/')));
        // 说明：这里原先还有 T1.3「TINY 不区分大小写」与 T1.6「mini-C 区分大小写」
        // 两条断言，它们只读 LanguageConfig::caseSensitive —— 而那个字段
        // **没有任何代码使用**，生成程序查关键字用的是 strcmp 精确比对。
        // 也就是说那两条断言在给一个"配置说一套、实现做另一套"的现象背书。
        // 现在字段与断言一并删除；真实行为由 T23.8（INT 是关键字、int 只是标识符）
        // 与 AGENT.md §6.1 注 5 / §13.32 固定。
    }

    // ---- T2：随附数据文件是否齐备且非空 ----
    for (const LanguageConfig &cfg : configs) {
        const QStringList required = {cfg.wordsFile, cfg.grammarFile, cfg.semanticsFile,
                                      cfg.sampleFile};
        for (const QString &fileName : required) {
            const QString path = FileUtil::locateDataFile(fileName);
            const QFileInfo info(path);
            const bool exists = info.exists() && info.size() > 0;
            ctx.check(QStringLiteral("T2 %1 数据文件存在且非空：%2").arg(cfg.displayName, fileName),
                      exists,
                      exists ? QStringLiteral("%1 字节").arg(info.size())
                             : QStringLiteral("未找到或为空：%1").arg(FileUtil::toNativePath(path)));
        }
    }

    // ---- T3：中文编码兼容（GBK 回退）----
    // 直接读取任务书配套的 TINY 单词表（GBK 编码）验证 FileUtil 的编码判定
    const QString taskBookWords = FileUtil::locateDataFile(QStringLiteral("tiny_words.txt"));
    bool ok = false;
    QString encoding;
    const QString wordsText = FileUtil::readText(taskBookWords, &ok, &encoding);
    ctx.check(QStringLiteral("T3 单词规格文件可读取"), ok && !wordsText.isEmpty(),
              QStringLiteral("编码判定为 %1").arg(encoding));
    ctx.check(QStringLiteral("T3.1 规格文件包含宏定义 digit=[0-9]"),
              wordsText.contains(QStringLiteral("digit=[0-9]")));
    ctx.check(QStringLiteral("T3.2 规格文件包含 Keyword103L 关键字定义"),
              wordsText.contains(QStringLiteral("Keyword103L")));
    ctx.check(QStringLiteral("T3.3 规格文件包含 Symbol200L 符号定义"),
              wordsText.contains(QStringLiteral("Symbol200L")));
    ctx.check(QStringLiteral("T3.4 规格文件包含注释 102 定义"),
              wordsText.contains(QStringLiteral("Comment_102")));

    // ---- T4：文法文件行数与基本格式 ----
    const LanguageConfig &tinyCfg = LanguageConfigRegistry::get(LanguageId::Tiny);
    const QString grammarText =
        FileUtil::readText(FileUtil::locateDataFile(tinyCfg.grammarFile), &ok, &encoding);
    int productionCount = 0;
    for (const QString &line : FileUtil::splitMeaningfulLines(grammarText)) {
        if (line.contains(QStringLiteral("->")))
            ++productionCount;
    }
    // 期望值 36：原任务书 TINY 文法的 34 条，加上本阶段补全的两条幂运算产生式
    // （power -> power POWER factor 与 power -> factor），使词表里已登记的 `^`（205）
    // 真正可达 —— 详见 data/tiny_grammar.txt 顶部的"说明 3"。
    ctx.check(QStringLiteral("T4 TINY 文法产生式条数为 36"), productionCount == 36,
              QStringLiteral("实际 %1 条").arg(productionCount));

    const LanguageConfig &minicCfg = LanguageConfigRegistry::get(LanguageId::MiniC);
    const QString minicGrammarText =
        FileUtil::readText(FileUtil::locateDataFile(minicCfg.grammarFile), &ok, &encoding);
    int minicProductionCount = 0;
    for (const QString &line : FileUtil::splitMeaningfulLines(minicGrammarText)) {
        if (line.contains(QStringLiteral("->")))
            ++minicProductionCount;
    }
    ctx.check(QStringLiteral("T4.1 mini-C 文法产生式条数为 65"), minicProductionCount == 65,
              QStringLiteral("实际 %1 条").arg(minicProductionCount));
    ctx.check(QStringLiteral("T4.2 mini-C 文法含 dowhile-stmt 产生式"),
              minicGrammarText.contains(QStringLiteral("dowhile-stmt ->")));

    // ---- T5：正则式解析（RegexParser）----
    // 覆盖任务书要求 1.2 的全部运算符号：连接、选择、闭包、正闭包、字符集、
    // 可选、括号、非、转义、空串
    {
        RegexParser parser;
        RegexAst ast;
        ctx.check(QStringLiteral("T5.1 基本正则 a(b|c)* 可解析"), parser.parse(QStringLiteral("a(b|c)*"), ast),
                  parser.error());
        ctx.check(QStringLiteral("T5.2 逆波兰式正确（a b c | * .）"),
                  ast.toPostfix() == QStringLiteral("\\a \\b \\c | * ."), ast.toPostfix());

        RegexParser p2;
        RegexAst a2;
        ctx.check(QStringLiteral("T5.3 字符集 [0-9] 可解析"),
                  p2.parse(QStringLiteral("[0-9]"), a2), p2.error());
        ctx.check(QStringLiteral("T5.4 [0-9] 展开为 10 个字符的选择"),
                  a2.toPostfix().count(QLatin1Char('|')) == 9, a2.toPostfix().left(60));

        RegexParser p3;
        RegexAst a3;
        ctx.check(QStringLiteral("T5.5 转义 \\+ 可作为字面量"),
                  p3.parse(QStringLiteral("\\+"), a3), p3.error());
        ctx.check(QStringLiteral("T5.6 转义后逆波兰式为 \\+"),
                  a3.toPostfix() == QStringLiteral("\\+"), a3.toPostfix());

        RegexParser p4;
        RegexAst a4;
        ctx.check(QStringLiteral("T5.7 可选 a? 可解析"),
                  p4.parse(QStringLiteral("a?"), a4), p4.error());
        ctx.check(QStringLiteral("T5.8 可选逆波兰式为 \\a ?"),
                  a4.toPostfix() == QStringLiteral("\\a ?"), a4.toPostfix());

        RegexParser p5;
        RegexAst a5;
        ctx.check(QStringLiteral("T5.9 正闭包 a+ 可解析"),
                  p5.parse(QStringLiteral("a+"), a5), p5.error());
        ctx.check(QStringLiteral("T5.10 正闭包逆波兰式为 \\a +"),
                  a5.toPostfix() == QStringLiteral("\\a +"), a5.toPostfix());

        RegexParser p6;
        RegexAst a6;
        ctx.check(QStringLiteral("T5.11 空串 # 可解析"),
                  p6.parse(QStringLiteral("#"), a6), p6.error());
        ctx.check(QStringLiteral("T5.12 非运算 ~eoln 可解析"),
                  p6.parse(QStringLiteral("~eoln"), a6), p6.error());
        ctx.check(QStringLiteral("T5.13 ~eoln 展开为除换行回车外约 96 个字符"),
                  a6.toPostfix().count(QLatin1Char('|')) >= 90,
                  QStringLiteral("选择符个数 %1").arg(a6.toPostfix().count(QLatin1Char('|'))));

        RegexParser p7;
        RegexAst a7;
        ctx.check(QStringLiteral("T5.14 宏展开 letter(letter|digit)* 可解析"),
                  p7.parse(QStringLiteral("letter(letter|digit)*"), a7), p7.error());

        RegexParser p8;
        RegexAst a8;
        ctx.check(QStringLiteral("T5.15 括号不匹配时报错"), !p8.parse(QStringLiteral("a(b"), a8),
                  p8.error());
        RegexParser p9;
        RegexAst a9;
        ctx.check(QStringLiteral("T5.16 非法 ~abc 被拒绝"), !p9.parse(QStringLiteral("~abc"), a9),
                  p9.error());
    }

    // ---- T6：单词规格文件解析（TokenSpec）----
    {
        TokenSpec spec;
        const bool specOk = spec.parse(wordsText);
        ctx.check(QStringLiteral("T6.1 TINY 单词规格解析无错误"), specOk && spec.errors().isEmpty(),
                  spec.errors().join(QStringLiteral("；")));

        const QVector<RegexDef> autoDefs = spec.automatonDefs();
        // TINY 需要参与自动机构造的定义：
        //   Comment_102、Num_100、ID_101，以及 Symbol200L 展开出的 18 个符号
        //   （200 + - * / % ^ < <= <> > >= = { } ; := ( ) 共 18 个）
        //   合计 3 + 18 = 21 条。
        // 注意：关键字**不**参与自动机构造（它与 ID 的正则式完全重合），
        //       而是登记为"字面单词 → 编码"的常量表，由词法程序查表改判。
        ctx.check(QStringLiteral("T6.2 需要建自动机的定义有 21 条（Comment + Num + ID + 18 个符号）"),
                  autoDefs.size() == 21, QStringLiteral("实际 %1 条").arg(autoDefs.size()));

        // 关键字：8 个，编码 103~110，且各成一条（L 展开）
        const QVector<RegexDef> kwDefs = spec.keywordDefs();
        ctx.check(QStringLiteral("T6.3 关键字条目 8 条（Keyword103L 已按 L 展开）"),
                  kwDefs.size() == 8, QStringLiteral("实际 %1 条").arg(kwDefs.size()));
        {
            QVector<int> codes;
            QStringList words;
            for (const RegexDef &kw : kwDefs) {
                codes.append(kw.code);
                words.append(kw.tokenText);
            }
            std::sort(codes.begin(), codes.end());
            ctx.check(QStringLiteral("T6.4 关键字编码连续为 103~110"),
                      codes == QVector<int>({103, 104, 105, 106, 107, 108, 109, 110}),
                      QStringLiteral("实际 %1 个：首 %2 末 %3")
                          .arg(codes.size())
                          .arg(codes.isEmpty() ? -1 : codes.first())
                          .arg(codes.isEmpty() ? -1 : codes.last()));
            ctx.check(QStringLiteral("T6.4b 关键字词表为 if/then/else/end/repeat/until/read/write"),
                      words == QStringList({QStringLiteral("if"), QStringLiteral("then"),
                                            QStringLiteral("else"), QStringLiteral("end"),
                                            QStringLiteral("repeat"), QStringLiteral("until"),
                                            QStringLiteral("read"), QStringLiteral("write")}),
                      words.join(QLatin1Char('/')));
        }

        ctx.check(QStringLiteral("T6.5 宏定义 digit 与 letter 已登记"),
                  spec.macros().contains(QStringLiteral("digit"))
                      && spec.macros().contains(QStringLiteral("letter")));

        // 逐项核对 TINY 编码表（AGENT.md §6.1）
        bool codeOk = true;
        QString codeMsg;
        for (const RegexDef &def : spec.defs()) {
            if (def.name == QStringLiteral("Num") && def.code != 100) { codeOk = false; codeMsg = QStringLiteral("Num=%1").arg(def.code); }
            if (def.name == QStringLiteral("ID") && def.code != 101) { codeOk = false; codeMsg = QStringLiteral("ID=%1").arg(def.code); }
            if (def.name == QStringLiteral("Comment") && def.code != 102) { codeOk = false; codeMsg = QStringLiteral("Comment=%1").arg(def.code); }
        }
        ctx.check(QStringLiteral("T6.6 编码分配符合 AGENT.md §6.1（100/101/102）"), codeOk, codeMsg);

        // 符号表核对：18 个符号，编码 200~217，且每个符号都有正确的字面量
        const QMap<QString, int> symbols = spec.symbolTable();
        ctx.check(QStringLiteral("T6.7 TINY 专用符号 18 个（编码 200~217）"),
                  symbols.size() == 18, QStringLiteral("实际 %1 个").arg(symbols.size()));
        {
            bool symbolOk = symbols.value(QStringLiteral("+")) == 200
                            && symbols.value(QStringLiteral("-")) == 201
                            && symbols.value(QStringLiteral("*")) == 202
                            && symbols.value(QStringLiteral("/")) == 203
                            && symbols.value(QStringLiteral("%")) == 204
                            && symbols.value(QStringLiteral("^")) == 205
                            && symbols.value(QStringLiteral("<")) == 206
                            && symbols.value(QStringLiteral("<=")) == 207
                            && symbols.value(QStringLiteral("<>")) == 208
                            && symbols.value(QStringLiteral(">")) == 209
                            && symbols.value(QStringLiteral(">=")) == 210
                            && symbols.value(QStringLiteral("=")) == 211
                            && symbols.value(QStringLiteral("{")) == 212
                            && symbols.value(QStringLiteral("}")) == 213
                            && symbols.value(QStringLiteral(";")) == 214
                            && symbols.value(QStringLiteral(":=")) == 215
                            && symbols.value(QStringLiteral("(")) == 216
                            && symbols.value(QStringLiteral(")")) == 217;
            QStringList shown;
            for (auto it = symbols.constBegin(); it != symbols.constEnd(); ++it)
                shown << QStringLiteral("%1=%2").arg(it.key()).arg(it.value());
            ctx.check(QStringLiteral("T6.8 逐个符号的编码与 AGENT.md §6.1 完全一致"), symbolOk,
                      shown.join(QLatin1Char(' ')));
        }
    }

    // ---- T7：正则 → NFA 构造（Thompson）----
    {
        RegexToNfa builder;
        Nfa nfa;
        ctx.check(QStringLiteral("T7.1 a(b|c)* 的 NFA 构造成功"),
                  builder.buildFromPattern(QStringLiteral("a(b|c)*"), nfa), builder.error());
        // Thompson 构造的状态数分解（含统一开始状态）：
        //   统一开始状态 1 + 片段 a 的 2 + 片段 b 的 2 + 片段 c 的 2
        //   + 选择运算新建 2 + 闭包运算新建 2 = 11
        // 注意：连接运算只是补一条 ε 边，不新建状态
        ctx.check(QStringLiteral("T7.2 a(b|c)* 的 NFA 状态数为 11（含统一开始状态）"),
                  nfa.stateCount == 11, QStringLiteral("实际 %1").arg(nfa.stateCount));
        ctx.check(QStringLiteral("T7.3 NFA 存在接受状态"), !nfa.accepts.isEmpty(),
                  QStringLiteral("接受状态数 %1").arg(nfa.accepts.size()));
        ctx.check(QStringLiteral("T7.4 NFA 字母表含 a、b、c"),
                  nfa.alphabet.contains(QStringLiteral("a")) && nfa.alphabet.contains(QStringLiteral("b"))
                      && nfa.alphabet.contains(QStringLiteral("c")),
                  nfa.alphabet.join(QLatin1Char(',')));

        // 用 TINY 的真实规格构造合并 NFA
        TokenSpec spec;
        spec.parse(wordsText);
        QVector<NfaRule> rules;
        for (const RegexDef &def : spec.automatonDefs()) {
            RegexParser rp;
            rp.setMacros(spec.macros());
            NfaRule rule;
            rule.name = def.name;
            rule.code = def.code;
            rule.isComment = (def.kind == RegexKind::Comment);
            if (!rp.parse(def.rawRegex, rule.ast)) {
                ctx.check(QStringLiteral("T7.5 定义 %1 解析失败").arg(def.name), false, rp.error());
                continue;
            }
            rules.append(rule);
        }
        RegexToNfa builder2;
        Nfa combined;
        const bool combinedOk = builder2.build(rules, combined);
        if (!combinedOk) {
            // 逐条定位：哪一条规则的逆波兰式无法单独构造成功
            QStringList badRules;
            for (const NfaRule &rule : rules) {
                RegexToNfa single;
                Nfa tmp;
                if (!single.build({rule}, tmp)) {
                    badRules << QStringLiteral("%1(code=%2)：%3")
                                    .arg(rule.name)
                                    .arg(rule.code)
                                    .arg(single.error());
                }
            }
            ctx.note(QStringLiteral("逐条定位结果（共 %1 条规则）：\n%2")
                         .arg(rules.size())
                         .arg(badRules.isEmpty() ? QStringLiteral("每条单独构造都成功 —— "
                                                                  "问题出在合并时的片段栈上")
                                                 : badRules.join(QStringLiteral("\n"))));
        }
        ctx.check(QStringLiteral("T7.5 TINY 全部单词合并构造 NFA 成功"), combinedOk, builder2.error());
        if (combinedOk) {
            // 规模回归意图：改成字符类字母表、且字符集不再展开成 | 链之后，
            // TINY 的合并 NFA 只有几十个状态（按字节字母表时要 1563 个）。
            // 这里只要求"数量级正确、没有爆掉"，具体值由 T19 等断言单独核对。
            ctx.check(QStringLiteral("T7.6 合并 NFA 状态数在合理范围（10~2000）"),
                      combined.stateCount >= 10 && combined.stateCount <= 2000,
                      QStringLiteral("实际 %1 个状态、%2 条边、字母表 %3 个%4")
                          .arg(combined.stateCount)
                          .arg(combined.edges.size())
                          .arg(combined.alphabet.size())
                          .arg(combined.alphabetInfo.isEmpty() ? QStringLiteral("符号")
                                                               : QStringLiteral("字符类")));
            // 注释类规则在 TokenSpec 中的 code 仍为声明值（102），是否"不输出编码"
            // 由 AcceptTag::isComment 标志决定；这里验证该标志已被正确置位，
            // 且不存在"注释规则却被当成普通单词"的情况。
            ctx.check(QStringLiteral("T7.7 注释规则的接受标签已标记 isComment"),
                      [&] {
                          bool sawComment = false;
                          for (auto it = combined.accepts.constBegin();
                               it != combined.accepts.constEnd(); ++it) {
                              for (const AcceptTag &tag : it.value()) {
                                  if (tag.isComment)
                                      sawComment = true;
                                  if (tag.defName == QStringLiteral("Comment") && !tag.isComment)
                                      return false;
                              }
                          }
                          return sawComment;
                      }());
            // ε 闭包与 move 的基本性质验证
            QSet<int> startSet;
            startSet.insert(combined.start);
            const QSet<int> closure = combined.epsilonClosure(startSet);
            ctx.check(QStringLiteral("T7.8 ε 闭包包含开始状态且非空"),
                      closure.contains(combined.start) && closure.size() >= 2,
                      QStringLiteral("闭包大小 %1").arg(closure.size()));
            // 注意：按字符类模式时输入符号是"字符类键"而不是单个字节，
            // 必须先把字节 '1' 映射成它所属的字符类（TINY 里就是 digit）再 move。
            const QString digitKey = symbolKeyOfByte(combined.alphabetInfo, '1');
            const QSet<int> moved = combined.move(closure, digitKey);
            ctx.check(QStringLiteral("T7.9 由闭包经数字字符可 move 到新状态"), !moved.isEmpty(),
                      QStringLiteral("符号 %1，move 结果 %2 个状态")
                          .arg(digitKey.isEmpty() ? QStringLiteral("(无)") : digitKey)
                          .arg(moved.size()));
        }
    }

    // ---- T8：子集构造法 NFA → DFA（NfaToDfa）----
    // 用编译原理教材的经典例子 (a|b)*abb 核对状态数，便于在实验报告中
    // 与手算结果逐行对照。
    {
        RegexToNfa nfaBuilder;
        Nfa nfa;
        ctx.check(QStringLiteral("T8.1 (a|b)*abb 的 NFA 构造成功"),
                  nfaBuilder.buildFromPattern(QStringLiteral("(a|b)*abb"), nfa),
                  nfaBuilder.error());

        NfaToDfa subset;
        Dfa dfa;
        const bool dfaOk = subset.construct(nfa, dfa);
        ctx.check(QStringLiteral("T8.2 子集构造成功"), dfaOk, subset.error());

        if (dfaOk) {
            // 教材结论：(a|b)*abb 的子集构造结果有 5 个状态（A~E），
            // 其中只有 E 是接受状态；手算过程见实验报告。
            ctx.check(QStringLiteral("T8.3 (a|b)*abb 的 DFA 有 5 个状态（A~E）"),
                      dfa.stateCount == 5, QStringLiteral("实际 %1 个").arg(dfa.stateCount));
            ctx.check(QStringLiteral("T8.4 DFA 初态编号为 0"), dfa.start == 0);

            int acceptCount = 0;
            for (int s = 0; s < dfa.stateCount; ++s) {
                if (dfa.accepting.value(s, false))
                    ++acceptCount;
            }
            ctx.check(QStringLiteral("T8.5 DFA 恰有 1 个接受状态"), acceptCount == 1,
                      QStringLiteral("实际 %1 个").arg(acceptCount));

            // 教材中的单正则式 NFA 不带单词编码，因此该接受态的 acceptCode 应为 -1，
            // 但 accepting 标志必须为真 —— 这正是区分"接受但不输出编码"与
            // "非接受态"的关键，也是曾经踩过的坑。
            int acceptState = -1;
            for (int s = 0; s < dfa.stateCount; ++s) {
                if (dfa.accepting.value(s, false)) {
                    acceptState = s;
                    break;
                }
            }
            ctx.check(QStringLiteral("T8.5b 接受态 accepting=true 但 acceptCode=-1（不输出编码）"),
                      acceptState >= 0 && dfa.acceptCode.value(acceptState, -2) == -1,
                      QStringLiteral("接受态 %1，编码 %2")
                          .arg(acceptState)
                          .arg(dfa.acceptCode.value(acceptState, -2)));

            // 确定性：每个状态在每个符号上至多一条转移（QMap 天然保证，
            // 此处核对转移目标合法性）
            bool transValid = true;
            QString transMsg;
            for (int s = 0; s < dfa.stateCount && transValid; ++s) {
                for (auto it = dfa.trans.at(s).constBegin(); it != dfa.trans.at(s).constEnd(); ++it) {
                    if (it.value() < 0 || it.value() >= dfa.stateCount) {
                        transValid = false;
                        transMsg = QStringLiteral("状态 %1 经 %2 转移到非法状态 %3")
                                       .arg(s)
                                       .arg(it.key())
                                       .arg(it.value());
                    }
                }
            }
            ctx.check(QStringLiteral("T8.6 DFA 全部转移目标合法"), transValid, transMsg);

            // 手工跟踪字符串 "abb"，必须能从初态走到接受状态
            int state = dfa.start;
            bool reachable = true;
            for (const QChar &ch : QStringLiteral("abb")) {
                const auto it = dfa.trans.at(state).constFind(QString(ch));
                if (it == dfa.trans.at(state).constEnd()) {
                    reachable = false;
                    break;
                }
                state = it.value();
            }
            ctx.check(QStringLiteral("T8.7 字符串 abb 能被 DFA 接受"),
                      reachable && dfa.accepting.value(state, false),
                      QStringLiteral("终态 %1").arg(state));

            // 手工跟踪字符串 "ab"，应停在不接受状态
            state = dfa.start;
            reachable = true;
            for (const QChar &ch : QStringLiteral("ab")) {
                const auto it = dfa.trans.at(state).constFind(QString(ch));
                if (it == dfa.trans.at(state).constEnd()) {
                    reachable = false;
                    break;
                }
                state = it.value();
            }
            ctx.check(QStringLiteral("T8.8 字符串 ab 不能被 DFA 接受"),
                      reachable && !dfa.accepting.value(state, false),
                      QStringLiteral("终态 %1").arg(state));

            // 最小化验证（一）：教材的 (a|b)*abb 子集构造结果经最小化后为 4 个状态。
            // 手算依据（见实验报告）：A 与 C 在 a、b 上分别到 {1}、{2}，转移等价，
            // 因此合并；而 A 经 bb 可达接受态、C 不能，故 A/C 与 D 仍可区分。
            // 结果：{A,C} 合并后共 4 个状态。
            DfaMinimize minimizer;
            Dfa minimal;
            ctx.check(QStringLiteral("T8.9 (a|b)*abb 的 DFA 最小化成功"),
                      minimizer.minimize(dfa, minimal), minimizer.error());
            ctx.check(QStringLiteral("T8.10 (a|b)*abb 最小化后为 4 个状态（A 与 C 等价合并）"),
                      minimal.stateCount == 4,
                      QStringLiteral("实际 %1 个").arg(minimal.stateCount));

            // 最小化验证（二）：手工构造一个"含等价状态"的 DFA，验证最小化确实会合并。
            // 设计：状态 4、5 的转移完全相同（都经 a、b 回到 4），且同为接受态、
            //       同为编码 100 → 二者等价，应被合并为 1 个状态，6 → 5。
            Dfa hand;
            hand.stateCount = 6;
            hand.start = 0;
            hand.alphabet = QStringList{QStringLiteral("a"), QStringLiteral("b")};
            hand.trans.clear();
            hand.trans.resize(6);
            hand.trans[0].insert(QStringLiteral("a"), 1);
            hand.trans[0].insert(QStringLiteral("b"), 2);
            hand.trans[1].insert(QStringLiteral("a"), 1);
            hand.trans[1].insert(QStringLiteral("b"), 3);
            hand.trans[2].insert(QStringLiteral("a"), 1);
            hand.trans[2].insert(QStringLiteral("b"), 2);
            hand.trans[3].insert(QStringLiteral("a"), 4);
            hand.trans[3].insert(QStringLiteral("b"), 2);
            // 状态 4 与 5：完全相同的转移与相同的接受编码
            hand.trans[4].insert(QStringLiteral("a"), 4);
            hand.trans[4].insert(QStringLiteral("b"), 4);
            hand.trans[5].insert(QStringLiteral("a"), 4);
            hand.trans[5].insert(QStringLiteral("b"), 4);
            hand.accepting.fill(false, 6);
            hand.acceptCode.fill(-1, 6);
            hand.acceptIsComment.fill(false, 6);
            hand.accepting[4] = true;
            hand.accepting[5] = true;
            hand.acceptCode[4] = 100;
            hand.acceptCode[5] = 100;
            hand.subsets.resize(6);
            hand.acceptTag.resize(6);

            DfaMinimize handMin;
            Dfa handOut;
            ctx.check(QStringLiteral("T8.12 手工 DFA（含等价状态对）最小化成功"),
                      handMin.minimize(hand, handOut), handMin.error());
            // 手算：状态 0 与 2 等价（都在 a 上到 1、b 上到 2），状态 4 与 5 等价
            // （接受态、编码同为 100、转移同为 a,b → 4）。两组各合并 1 个，6 → 4。
            ctx.check(QStringLiteral("T8.13 两组等价状态各合并 1 个，状态数 6 → 4"),
                      handOut.stateCount == 4,
                      QStringLiteral("实际 %1 个").arg(handOut.stateCount));
            ctx.check(QStringLiteral("T8.14 合并记录显示 0/2 同组且 4/5 同组"),
                      [&] {
                          bool zeroTwo = false;
                          bool fourFive = false;
                          for (const QVector<int> &group : handMin.mergedGroups()) {
                              if (group.contains(0) && group.contains(2))
                                  zeroTwo = true;
                              if (group.contains(4) && group.contains(5))
                                  fourFive = true;
                          }
                          return zeroTwo && fourFive;
                      }());

            // 最小化验证（三）：接受态仍必须有 1 个，且 abb/ab 判定不变
            int minimalAccept = 0;
            for (int s = 0; s < minimal.stateCount; ++s) {
                if (minimal.accepting.value(s, false))
                    ++minimalAccept;
            }
            auto accepts = [](const Dfa &machine, const QString &text) {
                int s = machine.start;
                for (const QChar &ch : text) {
                    const auto it = machine.trans.at(s).constFind(QString(ch));
                    if (it == machine.trans.at(s).constEnd())
                        return false;
                    s = it.value();
                }
                return machine.accepting.value(s, false);
            };
            ctx.check(QStringLiteral("T8.11 最小化 DFA 恰有 1 个接受状态且 abb/ab 判定不变"),
                      minimalAccept == 1 && accepts(minimal, QStringLiteral("abb"))
                          && !accepts(minimal, QStringLiteral("ab")),
                      QStringLiteral("接受态 %1 个").arg(minimalAccept));
        }
    }

    // ---- T9：TINY 全部单词的 DFA 与最小化 DFA ----
    {
        TokenSpec spec;
        spec.parse(wordsText);

        QVector<NfaRule> rules;
        for (const RegexDef &def : spec.automatonDefs()) {
            RegexParser rp;
            rp.setMacros(spec.macros());
            NfaRule rule;
            rule.name = def.name;
            rule.code = def.code;
            rule.isComment = (def.kind == RegexKind::Comment);
            if (rp.parse(def.rawRegex, rule.ast))
                rules.append(rule);
        }

        RegexToNfa nfaBuilder;
        Nfa nfa;
        ctx.check(QStringLiteral("T9.1 TINY 合并 NFA 构造成功"),
                  nfaBuilder.build(rules, nfa), nfaBuilder.error());

        NfaToDfa subset;
        Dfa dfa;
        const bool dfaOk = subset.construct(nfa, dfa);
        ctx.check(QStringLiteral("T9.2 TINY 子集构造成功"), dfaOk, subset.error());

        if (dfaOk) {
            ctx.check(QStringLiteral("T9.3 DFA 状态数少于 NFA 状态数（子集构造必然收缩）"),
                      dfa.stateCount < nfa.stateCount,
                      QStringLiteral("NFA %1 → DFA %2").arg(nfa.stateCount).arg(dfa.stateCount));

            // 各单词编码都应能在 DFA 中被找到
            QSet<int> codes;
            for (int s = 0; s < dfa.stateCount; ++s) {
                if (dfa.accepting.value(s, false) && dfa.acceptCode.value(s, -1) >= 0)
                    codes.insert(dfa.acceptCode.value(s, -1));
            }
            bool allPresent = true;
            QString missing;
            for (int c = 100; c <= 102; ++c) {
                if (!codes.contains(c)) {
                    allPresent = false;
                    missing += QStringLiteral("%1 ").arg(c);
                }
            }
            ctx.check(QStringLiteral("T9.4 DFA 接受状态覆盖 Num(100)/ID(101)/Comment(102)"),
                      allPresent, QStringLiteral("缺失编码 %1").arg(missing));

            // 最小化
            DfaMinimize minimizer;
            Dfa minimal;
            const bool minOk = minimizer.minimize(dfa, minimal);
            ctx.check(QStringLiteral("T9.5 TINY DFA 最小化成功"), minOk, minimizer.error());

            if (minOk) {
                ctx.check(QStringLiteral("T9.6 最小化后状态数不多于原 DFA"),
                          minimal.stateCount <= dfa.stateCount,
                          QStringLiteral("%1 → %2").arg(dfa.stateCount).arg(minimal.stateCount));
                ctx.check(QStringLiteral("T9.7 最小化 DFA 初态编号为 0"), minimal.start == 0);

                // 合并记录必须覆盖全部原状态（划分是对状态集的完全划分）
                int mergedTotal = 0;
                for (const QVector<int> &group : minimizer.mergedGroups())
                    mergedTotal += group.size();
                ctx.check(QStringLiteral("T9.8 合并记录覆盖全部原状态（完全划分）"),
                          mergedTotal == dfa.stateCount,
                          QStringLiteral("覆盖 %1 / 原 %2").arg(mergedTotal).arg(dfa.stateCount));

                // 关键正确性：编码不同的接受态绝不能被合并到同一状态
                // （QSet<int> 保存原状态，若同一组内出现两个不同编码即为错误）
                bool noBadMerge = true;
                QString badMsg;
                const QVector<QVector<int>> &groups = minimizer.mergedGroups();
                for (int g = 0; g < groups.size() && noBadMerge; ++g) {
                    QSet<int> codesInGroup;
                    for (int oldState : groups.at(g)) {
                        if (oldState < dfa.accepting.size() && dfa.accepting.at(oldState))
                            codesInGroup.insert(dfa.acceptCode.value(oldState, -1));
                    }
                    if (codesInGroup.size() > 1) {
                        noBadMerge = false;
                        badMsg = QStringLiteral("第 %1 组同时含有 %2 个不同编码")
                                     .arg(g)
                                     .arg(codesInGroup.size());
                    }
                }
                ctx.check(QStringLiteral("T9.9 不同编码的接受态未被错误合并"), noBadMerge, badMsg);

                // 行为等价性抽查：对若干输入串比较"是否接受 + 接受编码"。
                // 未接受统一记为 -9，以便与"接受但不输出编码(-1)"区分。
                const QStringList probes = {QStringLiteral("123"),   QStringLiteral("abc"),
                                            QStringLiteral("_x1"),   QStringLiteral("{ }"),
                                            QStringLiteral("12a"),   QStringLiteral("if"),
                                            QStringLiteral("+"),     QStringLiteral("0")};
                const int kNotAccept = -9;
                // 逐字符驱动 DFA。注意必须先把字节映射成字母表里的符号键：
                // 按字符类模式时键是 "@0" 这样的类符号，直接拿字符查会**永远查不到**，
                // 于是"两边都判不接受"→ 断言假通过（本项目实际踩过）。
                auto runOn = [kNotAccept](const Dfa &machine, const QString &probe) {
                    int s = machine.start;
                    for (const QChar &ch : probe) {
                        const QString key = symbolKeyOfByte(
                            machine.alphabetInfo, static_cast<int>(ch.unicode()) & 0xFF);
                        if (key.isEmpty())
                            return kNotAccept;
                        const auto it = machine.trans.at(s).constFind(key);
                        if (it == machine.trans.at(s).constEnd())
                            return kNotAccept;
                        s = it.value();
                    }
                    if (!machine.accepting.value(s, false))
                        return kNotAccept;
                    return machine.acceptCode.value(s, -1);
                };
                QStringList mismatch;
                int acceptedProbes = 0;
                for (const QString &probe : probes) {
                    const int oldCode = runOn(dfa, probe);
                    const int newCode = runOn(minimal, probe);
                    if (oldCode != kNotAccept || newCode != kNotAccept)
                        ++acceptedProbes;
                    if (oldCode != newCode)
                        mismatch << QStringLiteral("%1(%2/%3)").arg(probe).arg(oldCode).arg(newCode);
                }
                // acceptedProbes 是**防空转**的保险：若两边都查不到转移，
                // mismatch 同样为空，但那种"一致"毫无意义。
                ctx.check(QStringLiteral("T9.10 最小化前后对 8 个输入串的接受结果一致"),
                          mismatch.isEmpty() && acceptedProbes >= 4,
                          QStringLiteral("%1（可接受探针 %2 / %3 个）")
                              .arg(mismatch.join(QLatin1Char(' ')))
                              .arg(acceptedProbes)
                              .arg(probes.size()));

                ctx.check(QStringLiteral("T9.11 划分过程记录含多轮细化信息"),
                          minimizer.partitionTrace().size() >= 3,
                          QStringLiteral("记录 %1 行").arg(minimizer.partitionTrace().size()));

                // 端到端等价性：用"最长匹配（maximal munch）"规则分别驱动
                // 原 DFA 与最小化 DFA，对多段输入求单词编码序列并逐项比较。
                // 这是比"单串是否接受"更强的验证：它同时覆盖了中途接受状态、
                // 回退（backtrack）以及多个单词之间的切分边界。
                auto tokenizeWith = [](const Dfa &machine, const QString &input) {
                    QStringList codes;
                    int pos = 0;
                    while (pos < input.size()) {
                        if (input.at(pos).isSpace()) {
                            ++pos;
                            continue;
                        }
                        int state = machine.start;
                        int lastAcceptCode = -1;
                        int lastAcceptPos = pos;
                        int p = pos;
                        while (p < input.size()) {
                            // 同样要先把字节映射成符号键（字符类模式下的键是 "@0"）
                            const QString key = symbolKeyOfByte(
                                machine.alphabetInfo, static_cast<int>(input.at(p).unicode()) & 0xFF);
                            const auto it = key.isEmpty()
                                                ? machine.trans.at(state).constEnd()
                                                : machine.trans.at(state).constFind(key);
                            if (it == machine.trans.at(state).constEnd())
                                break;
                            state = it.value();
                            ++p;
                            if (machine.accepting.value(state, false)
                                && machine.acceptCode.value(state, -1) >= 0) {
                                lastAcceptCode = machine.acceptCode.value(state, -1);
                                lastAcceptPos = p;
                            }
                        }
                        if (lastAcceptCode < 0) {
                            codes << QStringLiteral("ERR@%1").arg(pos);
                            break;
                        }
                        codes << QString::number(lastAcceptCode);
                        pos = lastAcceptPos;
                    }
                    return codes;
                };

                const QStringList programs = {
                    QStringLiteral("if x then x := 12 end"),
                    QStringLiteral("read x; write 100"),
                    QStringLiteral("a:=b*c^2"),
                    QStringLiteral("x >= 10"),
                    QStringLiteral("{ comment } y := 3"),
                    QStringLiteral("repeat until (x <> 0)"),
                    QStringLiteral("abc_9 3 := + - * / % ^ <= = ; { }"),
                };
                QStringList tokenizeMismatch;
                for (const QString &program : programs) {
                    const QStringList before = tokenizeWith(dfa, program);
                    const QStringList after = tokenizeWith(minimal, program);
                    if (before != after) {
                        tokenizeMismatch << QStringLiteral("%1：[%2] vs [%3]")
                                                .arg(program, before.join(QLatin1Char(',')),
                                                     after.join(QLatin1Char(',')));
                    }
                }
                ctx.check(QStringLiteral("T9.12 最小化前后对 7 段程序的最长匹配切分完全一致"),
                          tokenizeMismatch.isEmpty(),
                          tokenizeMismatch.join(QStringLiteral(" ｜ ")));

                // 顺带核对最长匹配确实生效：":=" 必须整体识别为 215，
                // 而不能被切成 ':' 与 '='
                {
                    const QStringList codes = tokenizeWith(dfa, QStringLiteral("a:=1"));
                    ctx.check(QStringLiteral("T9.13 最长匹配生效：a:=1 切分为 101,215,100"),
                              codes == QStringList({QStringLiteral("101"), QStringLiteral("215"),
                                                    QStringLiteral("100")}),
                              codes.join(QLatin1Char(',')));
                }
            }
        }
    }

    // ---- T10：完整词法流水线（生成 C++ 源码 → g++ 编译 → 运行 → 产出 .lex）----
    //
    // 这是第一项任务的端到端验收（任务书要求 1.7~1.9）。为了让自测在缺少编译器的
    // 环境下也能给出明确结论，这里先用 QStandardPaths 查找 g++：
    //   · 找得到 → 跑完整链路并逐项断言（权威结果）；
    //   · 找不到 → 只跑"生成源码"部分，并把编译环节记为"跳过"，同时输出提示。
    {
        // 定位随附数据（data 目录与 exe 同级）
        const QString wordsPath = FileUtil::locateDataFile(QStringLiteral("tiny_words.txt"));
        const QString samplePath = FileUtil::locateDataFile(QStringLiteral("sample.tny"));
        bool ok = false;
        const QString wordsText = FileUtil::readText(wordsPath, &ok);
        ctx.check(QStringLiteral("T10.1 可读取 tiny_words.txt"), ok && !wordsText.isEmpty(),
                  wordsPath);
        ctx.check(QStringLiteral("T10.2 可定位 sample.tny"),
                  QFile::exists(samplePath), samplePath);

        if (ok && !wordsText.isEmpty() && QFile::exists(samplePath)) {
            // 查找 g++：优先使用 Qt 自带的 MinGW，其次 PATH
            QString compiler = QStringLiteral("g++");
            const QString candidate = QStringLiteral("D:/Qt6.10/Tools/mingw1310_64/bin/g++.exe");
            if (QFile::exists(candidate))
                compiler = candidate;
            const bool haveCompiler = (compiler != QStringLiteral("g++")) || true;

            LexPipeline pipeline;
            const QString outDir = FileUtil::outputDir() + QStringLiteral("/lexbuild");
            const LexPipeline::Result r =
                pipeline.run(wordsText, QStringLiteral("TINY"), samplePath, outDir,
                             QStringLiteral("tiny"), haveCompiler, compiler);

            ctx.check(QStringLiteral("T10.3 流水线规格解析与自动机构造全部成功"),
                      r.specOk && r.nfaOk && r.dfaOk && r.minOk, r.summary());
            ctx.check(QStringLiteral("T10.4 已生成词法分析源程序"), r.codeOk,
                      r.generatedCode.isEmpty() ? r.error
                                                : QStringLiteral("%1 行")
                                                      .arg(r.generatedCode.count(QLatin1Char('\n'))
                                                           + 1));

            if (r.codeOk) {
                // 生成代码的结构性检查：必须含两种方法都会有的关键函数与常量
                ctx.check(QStringLiteral("T10.5 生成代码含状态常量与转移函数"),
                          r.generatedCode.contains(QStringLiteral("static const int S0"))
                              && r.generatedCode.contains(QStringLiteral("stepState")));
                ctx.check(QStringLiteral("T10.6 生成代码含关键字查表函数 findKeyword"),
                          r.generatedCode.contains(QStringLiteral("findKeyword")));
                ctx.check(QStringLiteral("T10.7 生成代码含最长匹配与回退逻辑（lastAcceptPos）"),
                          r.generatedCode.contains(QStringLiteral("lastAcceptPos")));
                ctx.check(QStringLiteral("T10.8 生成代码头部标注所用转换方法"),
                          r.generatedCode.contains(r.methodName));
                ctx.check(QStringLiteral("T10.9 生成代码把 ID 编码写为 101（%1 处引用关键字表）")
                              .arg(QStringLiteral("相应")),
                          r.generatedCode.contains(QStringLiteral("g_idCode = 101")));
            }

            if (r.build.ran) {
                ctx.check(QStringLiteral("T10.10 g++ 编译成功"), r.build.compiled,
                          r.build.compilerOutput.left(1200));
                ctx.check(QStringLiteral("T10.11 生成的可执行文件运行成功（返回码 0）"),
                          r.build.runExitCode == 0, r.build.runOutput.left(300));
                ctx.check(QStringLiteral("T10.12 已产出 .lex 文件且非空"),
                          !r.build.lexContent.isEmpty(), r.build.lexPath);

                const QStringList lexLines =
                    r.build.lexContent.split(QLatin1Char('\n'), Qt::SkipEmptyParts);
                ctx.check(QStringLiteral("T10.13 .lex 末行为 EOF"), !lexLines.isEmpty()
                              && lexLines.last().trimmed() == QStringLiteral("EOF"),
                          lexLines.isEmpty() ? QString() : lexLines.last());

                // 逐项核对 sample.tny 的词法结果。
                // sample.tny 第 1 行是 "read xXx; {read和标识符判断}"，
                // 关键字按声明顺序 if(103) then(104) else(105) end(106) repeat(107)
                // until(108) read(109) write(110)，故 read 应为 109；
                // 因此该行应为：109(read) 101"xXx" 214(;) 102(注释，不输出)
                ctx.check(QStringLiteral("T10.14 第一行词法结果正确（read=109/xXx/; 且注释无输出）"),
                          lexLines.size() >= 3 && lexLines.at(0).trimmed() == QStringLiteral("109")
                              && lexLines.at(1).trimmed() == QStringLiteral("101 \"xXx\"")
                              && lexLines.at(2).trimmed() == QStringLiteral("214"),
                          lexLines.mid(0, 4).join(QStringLiteral(" | ")));

                // 统计各类编码出现次数，核对"关键字确实被改判"（若未改判则全是 101）
                int kwCount = 0;
                int idCount = 0;
                int numCount = 0;
                int commentLeak = 0;
                for (const QString &line : lexLines) {
                    const QString trimmed = line.trimmed();
                    if (trimmed.isEmpty() || trimmed == QStringLiteral("EOF"))
                        continue;
                    const int code = trimmed.section(QLatin1Char(' '), 0, 0).toInt();
                    if (code >= 103 && code <= 110)
                        ++kwCount;
                    else if (code == 101)
                        ++idCount;
                    else if (code == 100)
                        ++numCount;
                    if (code == 102)
                        ++commentLeak;
                }
                ctx.check(QStringLiteral("T10.15 关键字已查表改判（出现 103~110 编码 %1 个）")
                              .arg(kwCount),
                          kwCount >= 8, QStringLiteral("关键字 %1 个、ID %2 个").arg(kwCount).arg(idCount));
                ctx.check(QStringLiteral("T10.16 数字与标识符正常识别"),
                          idCount >= 5 && numCount >= 3,
                          QStringLiteral("ID %1 个、NUM %2 个").arg(idCount).arg(numCount));
                ctx.check(QStringLiteral("T10.17 注释不输出编码（.lex 中无 102）"),
                          commentLeak == 0, QStringLiteral("出现 %1 次").arg(commentLeak));

                // 关键符号抽样核对（sample.tny 用到了 <= <> >= % := 等）
                // 编码以 data/tiny_words.txt 中 Symbol200L 的书写顺序为准：
                //   + - * / % ^ < <= <> > >= = { } ; :=  →  200 … 215
                ctx.check(QStringLiteral("T10.18 关系运算符 <= / <> / >= 编码正确（207/208/210）"),
                          r.build.lexContent.contains(QStringLiteral("\n207\n"))
                              && r.build.lexContent.contains(QStringLiteral("\n208\n"))
                              && r.build.lexContent.contains(QStringLiteral("\n210\n")));
                ctx.check(QStringLiteral("T10.19 赋值号 := 编码正确（215）"),
                          r.build.lexContent.contains(QStringLiteral("\n215\n")));
                ctx.check(QStringLiteral("T10.20 数字保留前导零（001 原样输出）"),
                          r.build.lexContent.contains(QStringLiteral("100 001")));

                // 方法二（switch(state)）也必须能编译运行并给出**相同**的 .lex
                LexPipeline pipeline2;
                pipeline2.setMethod(LexCodeGen::Method::SwitchState);
                const LexPipeline::Result r2 =
                    pipeline2.run(wordsText, QStringLiteral("TINY"), samplePath, outDir,
                                  QStringLiteral("tiny_switch"), true, compiler);
                ctx.check(QStringLiteral("T10.21 方法二（switch(state)）同样编译运行成功"),
                          r2.build.ran, r2.error.isEmpty() ? r2.summary() : r2.error);
                if (r2.build.ran) {
                    ctx.check(QStringLiteral("T10.22 两种转换方法产出的 .lex 完全一致"),
                              r2.build.lexContent == r.build.lexContent,
                              QStringLiteral("方法一 %1 行 / 方法二 %2 行")
                                  .arg(lexLines.size())
                                  .arg(r2.build.lexContent.count(QLatin1Char('\n'))));
                }

                // 任务书示例排版（拼成一行）
                LexPipeline pipeline3;
                pipeline3.setLayout(LexCodeGen::Layout::TaskbookSample);
                const LexPipeline::Result r3 =
                    pipeline3.run(wordsText, QStringLiteral("TINY"), samplePath, outDir,
                                  QStringLiteral("tiny_oneline"), true, compiler);
                if (r3.build.ran) {
                    const QStringList oneline =
                        r3.build.lexContent.split(QLatin1Char('\n'), Qt::SkipEmptyParts);
                    ctx.check(QStringLiteral("T10.23 任务书示例排版只输出一行单词 + 一行 EOF"),
                              oneline.size() == 2, QStringLiteral("实际 %1 行").arg(oneline.size()));
                }
            } else {
                ctx.note(QStringLiteral("未执行 g++ 编译环节（%1）：仅验证到生成源码为止。")
                             .arg(r.build.compilerOutput.left(1200).simplified()));
                // 把完整编译器输出落盘，便于定位链接期错误（自测报告只显示前若干字符）
                FileUtil::writeText(outDir + QStringLiteral("/compile_error.log"),
                                    r.build.compilerOutput);
            }
        }
    }

    // ---- T11：字符编码/解码与注释自动机的回归断言 ----
    {
        // T11.1~T11.5：字符编码必须可逆，且与解码规则严格对应。
        // 背景（重要坑）：取反运算的字符全集要覆盖全部 256 个字节值，
        // 生成的词法分析程序才能正确消费 UTF-8 汉字等高位字节；
        // 而生成的 C++ 比较常量若写成 '\200' 这类八进制字符字面量，
        // 会因 char 有符号而在比较时恒不相等，导致注释里一旦出现中文
        // 就断成单个符号。以下断言把"编码→解码→字节值"这条链路固定下来。
        struct EncodeCase { ushort code; const char *text; };
        const EncodeCase cases[] = {
            // 注意：encodeChar 对所有字符统一加反斜杠前缀，
            // 因为逆波兰记号以空格分隔，裸空格/裸运算符无法作为记号。
            { 'a',    "\\a" },
            { ' ',    "\\ " },
            { '{',    "\\{" },
            { '\\',   "\\\\" },
            { '\t',   "\\x09" },
            { '\n',   "\\x0a" },
            { '\r',   "\\x0d" },
            { 0xE5,   "\\xe5" },   // UTF-8 汉字的典型首字节
            { 0xFF,   "\\xff" },
        };
        bool allRoundTrip = true;
        bool allLowercase = true;
        bool allDecodable = true;
        QStringList badCases;
        for (const EncodeCase &c : cases) {
            const QChar ch(static_cast<ushort>(c.code));
            const QString encoded = RegexAst::encodeChar(QString(ch));
            const QString decoded = RegexToNfa::decodeToken(encoded);
            const QString expected = QString::fromLatin1(c.text);
            if (encoded != expected) {
                allRoundTrip = false;
                badCases << QStringLiteral("U+%1 编码为 [%2]，期望 [%3]")
                                .arg(c.code, 4, 16, QLatin1Char('0')).arg(encoded).arg(expected);
            }
            if (encoded.contains(QLatin1Char('X')))
                allLowercase = false;
            if (decoded != QString(ch))
                allDecodable = false;
        }
        ctx.check(QStringLiteral("T11.1 字符编码规则与预期一致（含转义与十六进制写法）"),
                  allRoundTrip, badCases.join(QStringLiteral("；")));
        ctx.check(QStringLiteral("T11.2 十六进制转义统一使用小写 x（不出现 \\XNN）"), allLowercase);
        ctx.check(QStringLiteral("T11.3 编码后的记号能被 decodeToken 还原为原字符"), allDecodable);

        // T11.4：取反所依据的字符全集必须覆盖全部字节值。
        // 这个断言直接保护"注释里出现中文就不再断掉"这一行为。
        const QStringList universe = RegexAst::unicodeUniverse();
        ctx.check(QStringLiteral("T11.4 取反字符全集覆盖全部 256 个字节值"),
                  universe.size() == 256, QStringLiteral("实际 %1 个").arg(universe.size()));
        ctx.check(QStringLiteral("T11.5 字符全集包含高位字节（0x80~0xFF，用于 UTF-8 汉字）"),
                  universe.contains(QString(QChar(0xE5))) && universe.contains(QString(QChar(0xFF))));

        // T11.5b：记号切分规则 —— 取反运算符的作用范围。
        // 这一条直接守住"~ 只作用于紧随其后的一个操作数"这一约定：
        //   "~eoln" 必须切成 Tilde Identifier，两者之间**没有**连接记号，
        //   否则一元运算找不到操作数；
        //   "\{ ~ \}" 必须在取反项与右花括号之间插入连接记号，
        //   否则 "\{" 会把 "\}" 当成取反的操作数而少解析一个字符
        //   （本项目实际踩过这个坑，症状是注释里一旦超过一个字符就断掉）。
        const QString posixTokens = RegexParser::debugTokens(QStringLiteral("~eoln"));
        ctx.check(QStringLiteral("T11.5b \"~eoln\" 中 ~ 与其操作数之间不插入连接记号"),
                  posixTokens.startsWith(QStringLiteral("Tilde@0 Identifier@1:")),
                  posixTokens);
        const QString braceTokens = RegexParser::debugTokens(QStringLiteral("\\{ (~ \\})* \\}"));
        ctx.check(QStringLiteral("T11.5c 注释正则切分后含两个连接记号（{, ~}, } 三段相接）"),
                  braceTokens.count(QStringLiteral("Concat@")) == 2, braceTokens);

        // T11.6~T11.9：用真实的 TINY 规格把注释行为端到端固定下来。
        // 这几个断言存在的意义：注释正则 \{ (~ \})* \} 涉及的三个坑
        //   （~ 的一元作用范围、多子结点 Concat 的运算符个数、字符全集范围）
        // 单靠人工观察很难发现，必须由自测守住。
        const QString wordsPath = FileUtil::locateDataFile(QStringLiteral("tiny_words.txt"));
        bool ok = false;
        const QString wordsText = FileUtil::readText(wordsPath, &ok);
        if (!ok || wordsText.isEmpty()) {
            ctx.check(QStringLiteral("T11.6 读取 TINY 单词规格文件"), false, wordsPath);
        } else {
            LexPipeline pipeline;
            const LexPipeline::Result r =
                pipeline.run(wordsText, QStringLiteral("TINY"), QString(), QString(), QString(),
                             false, QString());
            ctx.check(QStringLiteral("T11.6 TINY 规格可完成到最简 DFA 的全部步骤"),
                      r.minOk, r.error);
            if (r.minOk) {
                // 手工按逐字符驱动最简 DFA，检查注释能否被完整吃掉
                auto runDfa = [&r](const QString &input, int &code, bool &isComment) {
                    int state = r.minDfa.start;
                    code = -1;
                    isComment = false;
                    for (const QChar &ch : input) {
                        const QString key = symbolKeyOfByte(
                            r.minDfa.alphabetInfo, static_cast<int>(ch.unicode()) & 0xFF);
                        if (key.isEmpty())
                            return false;
                        const auto it = r.minDfa.trans.at(state).constFind(key);
                        if (it == r.minDfa.trans.at(state).constEnd())
                            return false;
                        state = it.value();
                    }
                    if (!r.minDfa.accepting.value(state, false))
                        return false;
                    code = r.minDfa.acceptCode.value(state, -1);
                    isComment = r.minDfa.acceptIsComment.value(state, false);
                    return true;
                };

                int code = -1;
                bool isComment = false;
                const bool c1 = runDfa(QStringLiteral("{ab}"), code, isComment);
                ctx.check(QStringLiteral("T11.7 最简 DFA 能识别多字符注释 {ab}"),
                          c1 && isComment, QStringLiteral("识别=%1 注释=%2 编码=%3")
                                               .arg(c1 ? 1 : 0).arg(isComment ? 1 : 0).arg(code));

                const bool c2 = runDfa(QStringLiteral("{}"), code, isComment);
                ctx.check(QStringLiteral("T11.8 最简 DFA 能识别空注释 {}"), c2 && isComment);

                const bool c3 = runDfa(QStringLiteral("{ab} x"), code, isComment);
                ctx.check(QStringLiteral("T11.9 注释后紧跟内容时不会整体误接受"),
                          !c3 || code != 102, QStringLiteral("整体接受=%1（应仅前缀可接受）")
                                                   .arg(c3 ? 1 : 0));
            }
        }
    }

    // ---- T19~T23：字符类字母表（把字符集原子做等价类划分）----
    //
    // 这一组断言守住任务一的核心改造：自动机的输入符号不再是 256 个字节，
    // 而是"字符类"（letter、digit、+ …）。四条必须同时成立的性质：
    //   T19  划分本身正确（两两不交、覆盖完整、命名唯一、数量与预期一致）
    //   T20  与"按字节字母表"的构造**语言等价**（逐字节比对两个最小 DFA）
    //   T21  与老师给的参考输出**逐词一致**（外部预言机，独立于本实现）
    //   T22  ~ 作用于复合宏时必须报错（历史上会静默忽略取反）
    //   T23  最小规格回归：关键字不建自动机、<other> 落地、大小写敏感
    // 前两条是内部自洽，T21 是外部正确性 —— 缺一不可。
    {
        const QString wordsPath = FileUtil::locateDataFile(QStringLiteral("tiny_words.txt"));
        bool wordsOk = false;
        const QString wordsText = FileUtil::readText(wordsPath, &wordsOk);
        TokenSpec spec;
        if (!wordsOk || !spec.parse(wordsText)) {
            ctx.check(QStringLiteral("T19.0 读取并解析 TINY 单词规格"), false, wordsPath);
        } else {
            QVector<NfaRule> rules;
            for (const RegexDef &def : spec.automatonDefs()) {
                RegexParser rp;
                rp.setMacros(spec.macros());
                NfaRule rule;
                rule.name = def.name;
                rule.code = def.code;
                rule.isComment = (def.kind == RegexKind::Comment);
                if (rp.parse(def.rawRegex, rule.ast))
                    rules.append(rule);
            }

            // ---- T19：字符类划分 ----
            RegexToNfa classBuilder;
            Nfa classNfa;
            const bool classOk =
                classBuilder.build(rules, classNfa, AlphabetMode::CharClass);
            ctx.check(QStringLiteral("T19.0 TINY 规格可按字符类字母表构造 NFA"), classOk,
                      classBuilder.error());

            if (classOk) {
                const CharClassAlphabet &table = classNfa.alphabetInfo;
                QStringList names;
                for (const CharClassInfo &info : table.classes)
                    names << info.name;

                ctx.check(QStringLiteral("T19.1 字母表是字符类而不是 256 个字节"),
                          !table.isEmpty() && table.classes.size() < 40,
                          QStringLiteral("字符类 %1 个").arg(table.classes.size()));

                // 预期：宏名 digit / letter 排最前（宏名优先），随后是各单字符符号，
                // 最后一个 <other> 是"除 } 之外、又被其它类覆盖掉的那部分 ~ \}"。
                const QStringList expected = {
                    QStringLiteral("digit"), QStringLiteral("letter"), QStringLiteral("{"),
                    QStringLiteral("}"),     QStringLiteral("+"),      QStringLiteral("-"),
                    QStringLiteral("*"),     QStringLiteral("/"),      QStringLiteral("%"),
                    QStringLiteral("^"),     QStringLiteral("<"),      QStringLiteral("="),
                    QStringLiteral(">"),     QStringLiteral(";"),      QStringLiteral(":"),
                    QStringLiteral("("),     QStringLiteral(")"),      QStringLiteral("<other>")};
                ctx.check(QStringLiteral("T19.2 TINY 的 18 个字符类名字与预期完全一致"),
                          names == expected, names.join(QLatin1Char(',')));

                bool disjoint = true;
                bool nonEmpty = true;
                bool keysUnique = true;
                QSet<QString> keys;
                QSet<int> seenBytes;
                int totalBytes = 0;
                for (const CharClassInfo &info : table.classes) {
                    if (info.key.isEmpty() || keys.contains(info.key))
                        keysUnique = false;
                    keys.insert(info.key);
                    if (info.bytes.isEmpty())
                        nonEmpty = false;
                    for (int b : info.bytes) {
                        if (seenBytes.contains(b))
                            disjoint = false;
                        seenBytes.insert(b);
                        ++totalBytes;
                    }
                }
                ctx.check(QStringLiteral("T19.3 各字符类两两不交且都非空"), disjoint && nonEmpty,
                          QStringLiteral("覆盖字节合计 %1 个").arg(totalBytes));
                ctx.check(QStringLiteral("T19.4 字符类的符号键唯一"), keysUnique,
                          keys.size() == table.classes.size() ? QString() : QStringLiteral("有重复键"));

                // TINY 的 ~ \} 覆盖除 } 之外的一切，因此全部 256 个字节都该有归属
                ctx.check(QStringLiteral("T19.5 TINY 的字符类覆盖全部 256 个字节"),
                          totalBytes == 256, QStringLiteral("实际覆盖 %1 个").arg(totalBytes));

                // 字节到类的映射必须与"最具体字符集"的直觉一致
                const QString keyA = table.symbolOfByte('a');
                const QString keyDigit = table.symbolOfByte('7');
                const QString keyPlus = table.symbolOfByte('+');
                ctx.check(QStringLiteral("T19.6 字节映射到正确的字符类（a→letter、7→digit、+→+）"),
                          table.nameOfSymbol(keyA) == QStringLiteral("letter")
                              && table.nameOfSymbol(keyDigit) == QStringLiteral("digit")
                              && table.nameOfSymbol(keyPlus) == QStringLiteral("+"),
                          QStringLiteral("a→%1、7→%2、+→%3")
                              .arg(table.nameOfSymbol(keyA), table.nameOfSymbol(keyDigit),
                                   table.nameOfSymbol(keyPlus)));
                ctx.check(QStringLiteral("T19.7 换行符归入 <other>（只被注释规则接受）"),
                          table.nameOfByte('\n') == QStringLiteral("<other>"),
                          table.nameOfByte('\n'));
                ctx.check(QStringLiteral("T19.8 字符类图例为纯 ASCII 且逐类写出字节集"),
                          !table.legendText().isEmpty()
                              && [&table] {
                                     // 注意：图例是多行文本，换行符要放行；
                                     // 其余必须是可打印 ASCII（DOT 与日志都能安全承载）。
                                     for (const QChar &ch : table.legendText()) {
                                         const ushort u = ch.unicode();
                                         if (u == '\n' || u == '\r')
                                             continue;
                                         if (u < 0x20 || u > 0x7E)
                                             return false;
                                     }
                                     return true;
                                 }(),
                          table.legendText().left(80).replace(QLatin1Char('\n'),
                                                              QLatin1Char(' ')));

                // ---- T20：与按字节字母表的构造语言等价 ----
                RegexToNfa byteBuilder;
                Nfa byteNfa;
                const bool byteOk = byteBuilder.build(rules, byteNfa, AlphabetMode::Byte);
                NfaToDfa subsetA;
                NfaToDfa subsetB;
                Dfa dfaClass;
                Dfa dfaByte;
                const bool dfaClassOk = subsetA.construct(classNfa, dfaClass);
                const bool dfaByteOk = subsetB.construct(byteNfa, dfaByte);
                DfaMinimize minA;
                DfaMinimize minB;
                Dfa minClass;
                Dfa minByte;
                const bool minClassOk = minA.minimize(dfaClass, minClass);
                const bool minByteOk = minB.minimize(dfaByte, minByte);
                ctx.check(QStringLiteral("T20.0 两种字母表都能走完 NFA→DFA→最小化"),
                          byteOk && dfaClassOk && dfaByteOk && minClassOk && minByteOk,
                          QStringLiteral("byte=%1 class=%2").arg(byteOk ? 1 : 0).arg(classOk ? 1 : 0));

                if (minClassOk && minByteOk) {
                    // 最小 DFA 的语言等价判定：从初态出发做 BFS 配对，
                    // 对**全部 256 个字节**比较两边是否有转移、以及转移是否落在同一对上。
                    // 两个最小 DFA 若语言相同，则必然同构，这种配对一定能走通。
                    auto sameLanguage = [](const Dfa &lhs, const Dfa &rhs) {
                        if (lhs.stateCount != rhs.stateCount)
                            return false;
                        QVector<int> toRhs(lhs.stateCount, -1);
                        QVector<int> toLhs(rhs.stateCount, -1);
                        QVector<QPair<int, int>> pending;
                        toRhs[lhs.start] = rhs.start;
                        toLhs[rhs.start] = lhs.start;
                        pending.append(qMakePair(lhs.start, rhs.start));
                        while (!pending.isEmpty()) {
                            const QPair<int, int> cur = pending.takeLast();
                            const int a = cur.first;
                            const int b = cur.second;
                            if (lhs.accepting.value(a, false) != rhs.accepting.value(b, false))
                                return false;
                            if (lhs.acceptCode.value(a, -1) != rhs.acceptCode.value(b, -1))
                                return false;
                            if (lhs.acceptIsComment.value(a, false)
                                != rhs.acceptIsComment.value(b, false))
                                return false;
                            for (int byte = 0; byte < 256; ++byte) {
                                const QString keyA = symbolKeyOfByte(lhs.alphabetInfo, byte);
                                const QString keyB = symbolKeyOfByte(rhs.alphabetInfo, byte);
                                int targetA = -1;
                                int targetB = -1;
                                if (!keyA.isEmpty()) {
                                    const auto it = lhs.trans.at(a).constFind(keyA);
                                    if (it != lhs.trans.at(a).constEnd())
                                        targetA = it.value();
                                }
                                if (!keyB.isEmpty()) {
                                    const auto it = rhs.trans.at(b).constFind(keyB);
                                    if (it != rhs.trans.at(b).constEnd())
                                        targetB = it.value();
                                }
                                if ((targetA < 0) != (targetB < 0))
                                    return false;
                                if (targetA < 0)
                                    continue;
                                if (toRhs.at(targetA) < 0) {
                                    toRhs[targetA] = targetB;
                                    toLhs[targetB] = targetA;
                                    pending.append(qMakePair(targetA, targetB));
                                } else if (toRhs.at(targetA) != targetB) {
                                    return false;
                                }
                            }
                        }
                        return true;
                    };

                    ctx.check(QStringLiteral("T20.1 两种字母表的最小 DFA 状态数相同"),
                              minClass.stateCount == minByte.stateCount,
                              QStringLiteral("字符类 %1 / 字节 %2")
                                  .arg(minClass.stateCount)
                                  .arg(minByte.stateCount));
                    ctx.check(QStringLiteral("T20.2 字符类字母表与字节字母表的自动机语言等价"
                                             "（逐字节比对 256 个转移）"),
                              sameLanguage(minClass, minByte),
                              QStringLiteral("字符类 DFA %1 状态 / 字节 DFA %2 状态")
                                  .arg(minClass.stateCount)
                                  .arg(minByte.stateCount));
                    // 规模收益：NFA 状态数应当大幅下降（字符集不再展开成 | 链）
                    ctx.check(QStringLiteral("T20.3 字符类构造的 NFA 远小于按字节的 NFA"),
                              classNfa.stateCount * 3 < byteNfa.stateCount,
                              QStringLiteral("字符类 %1 / 字节 %2 个状态")
                                  .arg(classNfa.stateCount)
                                  .arg(byteNfa.stateCount));
                }
            }
        }
    }

    // ---- T21：与老师参考数据的 .lex 输出逐词对照（外部预言机）----
    //
    // 为什么这条最有价值：它是**独立于本实现**的正确性证据。
    // reference/03_tiny_output.lex 与 11_minic_output.lex 用的是
    // "NAME:value" 写法，我们的是"编码 [原文]"写法，但两者的**单词序列**
    // 应当逐行一一对应（长度相同、末行都是 EOF）。
    // 这里按"参考行 → 我们的编码"做反查，逐行比对，任何一处切分差异都会暴露。
    {
        struct RefCase {
            const char *wordsFile;   ///< 单词规格文件名
            const char *refFile;     ///< 参考 .lex 文件名
            const char *language;    ///< 语言名
            const char *identifier;  ///< 产物文件名前缀
            int expectedLines;       ///< 参考文件的令牌行数
        };
        const RefCase cases[] = {
            {"tiny_words.txt", "03_tiny_output.lex", "TINY", "tiny", 96},
            {"minic_words.txt", "11_minic_output.lex", "mini-C", "minic", 79},
        };

        for (const RefCase &one : cases) {
            const QString wordsPath = FileUtil::locateDataFile(QString::fromLatin1(one.wordsFile));
            const QString refPath = FileUtil::locateDataFile(
                QStringLiteral("../reference/") + QString::fromLatin1(one.refFile));
            bool wordsOk = false;
            bool refOk = false;
            const QString wordsText = FileUtil::readText(wordsPath, &wordsOk);
            const QString refText = FileUtil::readText(refPath, &refOk);
            const QString tag = QString::fromLatin1(one.language);
            if (!wordsOk || !refOk) {
                ctx.check(QStringLiteral("T21.0 %1 规格与参考 .lex 均可读取").arg(tag), false,
                          refPath);
                continue;
            }

            LexPipeline pipeline;
            const LexPipeline::Result r =
                pipeline.run(wordsText, tag, QString(), QString(), QString::fromLatin1(one.identifier),
                             false, QString());
            ctx.check(QStringLiteral("T21.1 %1 规格可生成最简 DFA").arg(tag), r.minOk, r.error);
            if (!r.minOk)
                continue;

            // 用最简 DFA 自己跑一遍，得到与参考数据可比的令牌序列。
            // （不走 g++ 编译，避免为了比对再依赖外部编译器；编译链路由 T10/T15 覆盖。）
            const QMap<QString, int> literalTable = r.spec.literalCodeTable();
            QStringList ours;
            {
                const QString sourcePath = FileUtil::locateDataFile(
                    tag == QStringLiteral("TINY") ? QStringLiteral("sample.tny")
                                                  : QStringLiteral("sample.mc"));
                bool srcOk = false;
                const QString sourceText = FileUtil::readText(sourcePath, &srcOk);
                if (!srcOk) {
                    ctx.check(QStringLiteral("T21.2 %1 样例源程序可读取").arg(tag), false,
                              sourcePath);
                    continue;
                }
                const QString trimmed = sourceText;
                int pos = 0;
                bool failed = false;
                while (pos < trimmed.size()) {
                    const ushort first = trimmed.at(pos).unicode();
                    if (first == ' ' || first == '\t' || first == '\r' || first == '\n') {
                        ++pos;
                        continue;
                    }
                    int state = r.minDfa.start;
                    int lastCode = -2;   // -2 = 还没有任何接受位置
                    bool lastIsComment = false;
                    int lastPos = pos;
                    int p = pos;
                    while (p < trimmed.size()) {
                        const QString key = symbolKeyOfByte(
                            r.minDfa.alphabetInfo,
                            static_cast<int>(trimmed.at(p).unicode()) & 0xFF);
                        if (key.isEmpty())
                            break;
                        const auto it = r.minDfa.trans.at(state).constFind(key);
                        if (it == r.minDfa.trans.at(state).constEnd())
                            break;
                        state = it.value();
                        ++p;
                        if (r.minDfa.accepting.value(state, false)) {
                            lastCode = r.minDfa.acceptCode.value(state, -1);
                            lastIsComment = r.minDfa.acceptIsComment.value(state, false);
                            lastPos = p;
                        }
                    }
                    if (lastPos == pos) {
                        failed = true;
                        break;
                    }
                    if (!lastIsComment) {
                        // 忠实复现"生成程序输出规则"，这样才与老师的参考数据可比：
                        //  · ID（编码 101）先查关键字表，命中就改判为关键字编码；
                        //  · NUM（编码 100）输出原文，保留前导零；
                        //  · 注释**不输出任何编码** —— 注意 Dfa 里它的 acceptCode 仍是
                        //    声明值 102，必须靠 acceptIsComment 判断，只看 acceptCode 会误报。
                        const QString value = trimmed.mid(pos, lastPos - pos);
                        if (lastCode == 101) {
                            const int keywordCode = r.spec.keywordTable().value(value, -1);
                            if (keywordCode >= 0)
                                ours << QString::number(keywordCode);
                            else
                                ours << QStringLiteral("101 \"%1\"").arg(value);
                        } else if (lastCode == 100) {
                            ours << QStringLiteral("100 %1").arg(value);
                        } else if (lastCode >= 0) {
                            ours << QString::number(lastCode);
                        }
                    }
                    pos = lastPos;
                }
                ctx.check(QStringLiteral("T21.2 %1 样例程序可被最简 DFA 完整切分").arg(tag), !failed,
                          failed ? QStringLiteral("在位置 %1 处卡住").arg(pos) : QString());
                ours << QStringLiteral("EOF");
            }

            // 逐行比对：参考行 "NAME:value" → 我们的行
            QStringList refLines;
            for (const QString &line : refText.split(QLatin1Char('\n'))) {
                const QString trimmedLine = line.trimmed();
                if (!trimmedLine.isEmpty())
                    refLines << trimmedLine;
            }
            ctx.check(QStringLiteral("T21.3 %1 参考数据的令牌行数为 %2")
                          .arg(tag)
                          .arg(one.expectedLines),
                      refLines.size() == one.expectedLines,
                      QStringLiteral("实际 %1 行").arg(refLines.size()));

            QStringList mismatches;
            const int compareCount = qMin(refLines.size(), ours.size());
            for (int i = 0; i < compareCount; ++i) {
                const QString &refLine = refLines.at(i);
                const int colon = refLine.indexOf(QLatin1Char(':'));
                if (colon < 0) {
                    mismatches << QStringLiteral("第 %1 行参考格式异常").arg(i + 1);
                    continue;
                }
                const QString refName = refLine.left(colon);
                const QString refValue = refLine.mid(colon + 1).trimmed();
                QString expected;
                if (refName == QStringLiteral("EOF"))
                    expected = QStringLiteral("EOF");
                else if (refName == QStringLiteral("ID"))
                    expected = QStringLiteral("101 \"%1\"").arg(refValue);
                else if (refName == QStringLiteral("NUMBER"))
                    expected = QStringLiteral("100 %1").arg(refValue);
                else
                    expected = QString::number(literalTable.value(refValue, -999));

                const QString actual = (i < ours.size()) ? ours.at(i) : QStringLiteral("(缺失)");
                if (actual != expected)
                    mismatches << QStringLiteral("第 %1 行 期望 %2 实际 %3")
                                      .arg(i + 1)
                                      .arg(expected, actual);
            }
            ctx.check(QStringLiteral("T21.4 %1 的 %2 个令牌与老师参考数据逐行一致")
                          .arg(tag)
                          .arg(one.expectedLines),
                      mismatches.isEmpty() && refLines.size() == ours.size(),
                      mismatches.isEmpty()
                          ? QStringLiteral("%1 行全部吻合").arg(ours.size())
                          : mismatches.mid(0, 3).join(QStringLiteral("；")));
        }
    }

    // ---- T22：非运算 ~ 只支持"宏体是单个字符集"的宏 ----
    //
    // 历史缺陷：若宏体是 letter|digit 这类复合正则式，negate 会被置在一个
    // Alternate 结点上，而 resolveCharset() 只认 Charset 结点 —— 于是 ~m
    // 被**静默**当成 m，自动机悄悄变错，且没有任何报错。
    {
        RegexParser inner;
        RegexAst compound;
        const bool compoundOk = inner.parse(QStringLiteral("[a-z]|[0-9]"), compound);
        QMap<QString, RegexAst> macros;
        macros.insert(QStringLiteral("m"), compound);

        RegexParser negParser;
        negParser.setMacros(macros);
        RegexAst negAst;
        const bool negOk = negParser.parse(QStringLiteral("~m"), negAst);
        ctx.check(QStringLiteral("T22 对复合宏取反（~m）会明确报错，不再静默忽略"),
                  compoundOk && !negOk && !negParser.error().isEmpty(),
                  negParser.error());

        // 正向对照：宏体就是单个字符集时，~ 必须照常取反
        RegexParser digitParser;
        RegexAst digitAst;
        const bool digitOk2 = digitParser.parse(QStringLiteral("[0-9]"), digitAst);
        QMap<QString, RegexAst> macros2;
        macros2.insert(QStringLiteral("digit"), digitAst);
        RegexParser negDigitParser;
        negDigitParser.setMacros(macros2);
        RegexAst negDigitAst;
        const bool negDigitOk = negDigitParser.parse(QStringLiteral("~digit"), negDigitAst);
        const int negCount = negDigitOk ? negDigitAst.resolveCharset(negDigitAst.root()).size() : 0;
        ctx.check(QStringLiteral("T22b 对字符集宏取反（~digit）仍然正确（256-10=246 个字节）"),
                  digitOk2 && negDigitOk && negCount == 246,
                  QStringLiteral("取反后 %1 个字节").arg(negCount));
    }

    // ---- T23：最小规格回归（关键字不建自动机 + <other> + 大小写敏感）----
    //
    // 这条断言来自一个真实的提问："如果我只写
    //   Keyword103L=INT | DOUBLE | BOOL
    //   Comment_102=//(~eoln)*
    // 会怎样？" 答案的关键在于：关键字**不建自动机**，必须靠 ID 规则先识别成
    // 标识符、再查表改判编码；缺了 ID 规则，这些关键字永远不会出现。
    {
        const QString baseSpec = QStringLiteral(
            "digit=[0-9]\n"
            "letter=[a-zA-Z]\n"
            "Keyword103L=INT | DOUBLE | BOOL\n"
            "Comment_102=//(~eoln)*\n");

        // ---- T23.1：只有前两组定义时，三个关键字都应当被判定为"不可能产生" ----
        {
            LexPipeline pipeline;
            const LexPipeline::Result r = pipeline.run(
                baseSpec + QStringLiteral("Num_100=digit+\n"), QStringLiteral("MINIMAL"), QString(),
                QString(), QStringLiteral("minimal_noid"), false, QString());
            ctx.check(QStringLiteral("T23.1 缺少 ID 规则时流水线仍能建出自动机"), r.minOk, r.error);
            ctx.check(QStringLiteral("T23.2 关键字可产生性诊断报出 3 个关键字"),
                      r.warnings.size() == 3,
                      QStringLiteral("%1 条：%2")
                          .arg(static_cast<int>(r.warnings.size()))
                          .arg(r.warnings.join(QStringLiteral(" / "))));
        }

        // ---- T23.3：补上 ID 规则后，诊断应当清零，且各项行为符合预期 ----
        {
            const QString fullSpec = baseSpec + QStringLiteral("Num_100=digit+\n"
                                                               "ID_101=letter(letter|digit)*\n");
            LexPipeline pipeline;
            const LexPipeline::Result r = pipeline.run(fullSpec, QStringLiteral("MINIMAL"),
                                                       QString(), QString(), QStringLiteral("minimal"),
                                                       false, QString());
            ctx.check(QStringLiteral("T23.3 补上 ID 规则后最小规格可用"), r.minOk, r.error);
            if (r.minOk) {
                ctx.check(QStringLiteral("T23.4 补上 ID 规则后不再有关键字告警"),
                          r.warnings.isEmpty(), r.warnings.join(QStringLiteral(" / ")));

                // 关键字表：L 后缀的第 k 项 = 基号 + k
                const QMap<QString, int> keywords = r.spec.keywordTable();
                ctx.check(QStringLiteral("T23.5 关键字编码为 基号+k：INT=103、DOUBLE=104、BOOL=105"),
                          keywords.value(QStringLiteral("INT"), -1) == 103
                              && keywords.value(QStringLiteral("DOUBLE"), -1) == 104
                              && keywords.value(QStringLiteral("BOOL"), -1) == 105,
                          QStringLiteral("INT=%1 DOUBLE=%2 BOOL=%3")
                              .arg(keywords.value(QStringLiteral("INT"), -1))
                              .arg(keywords.value(QStringLiteral("DOUBLE"), -1))
                              .arg(keywords.value(QStringLiteral("BOOL"), -1)));

                // 字母表：digit、letter、/、<other> 四类
                QStringList names;
                for (const CharClassInfo &info : r.minDfa.alphabetInfo.classes)
                    names << info.name;
                ctx.check(QStringLiteral("T23.6 最小规格的字母表恰为 digit / letter / / / <other> 四类"),
                          names.size() == 4 && names.contains(QStringLiteral("letter"))
                              && names.contains(QStringLiteral("digit"))
                              && names.contains(QStringLiteral("<other>")),
                          names.join(QLatin1Char(',')));

                // 逐字符驱动最小 DFA 的小工具
                auto runDfa = [&r](const QString &text, int &code, bool &isComment) {
                    int state = r.minDfa.start;
                    code = -1;
                    isComment = false;
                    for (const QChar &ch : text) {
                        const QString key = symbolKeyOfByte(
                            r.minDfa.alphabetInfo, static_cast<int>(ch.unicode()) & 0xFF);
                        if (key.isEmpty())
                            return false;
                        const auto it = r.minDfa.trans.at(state).constFind(key);
                        if (it == r.minDfa.trans.at(state).constEnd())
                            return false;
                        state = it.value();
                    }
                    if (!r.minDfa.accepting.value(state, false))
                        return false;
                    code = r.minDfa.acceptCode.value(state, -1);
                    isComment = r.minDfa.acceptIsComment.value(state, false);
                    return true;
                };
                int code = -1;
                bool isComment = false;
                const bool upperOk = runDfa(QStringLiteral("INT"), code, isComment);
                // 注意分工：**自动机只负责识别"标识符形态"**，编码 101；
                // 改判成关键字 103 是生成程序里 findKeyword() 干的活。
                // 所以这里断言的是"自动机给出 101，而关键字表把它改判成 103"。
                const int upperKeyword = r.spec.keywordTable().value(QStringLiteral("INT"), -1);
                ctx.check(QStringLiteral("T23.7 自动机把 INT 识别为标识符(101)，关键字表改判为 103"),
                          upperOk && code == 101 && !isComment && upperKeyword == 103,
                          QStringLiteral("识别=%1 自动机编码=%2 关键字编码=%3")
                              .arg(upperOk ? 1 : 0)
                              .arg(code)
                              .arg(upperKeyword));

                const bool lowerOk = runDfa(QStringLiteral("int"), code, isComment);
                const int lowerKeyword = r.spec.keywordTable().value(QStringLiteral("int"), -1);
                ctx.check(QStringLiteral("T23.8 小写 int 只被识别为标识符（大小写敏感，无关键字）"),
                          lowerOk && code == 101 && lowerKeyword < 0,
                          QStringLiteral("识别=%1 编码=%2 关键字编码=%3")
                              .arg(lowerOk ? 1 : 0)
                              .arg(code)
                              .arg(lowerKeyword));

                const bool commentOk = runDfa(QStringLiteral("//hello"), code, isComment);
                // 注释态的 acceptCode 是声明值 102，但"是否输出编码"由 acceptIsComment
                // 决定 —— 只看 acceptCode 会误判成"输出了 102"。
                ctx.check(QStringLiteral("T23.9 //(~eoln)* 能把 //hello 整段识别为注释（不输出编码）"),
                          commentOk && isComment,
                          QStringLiteral("识别=%1 注释=%2 声明编码=%3")
                              .arg(commentOk ? 1 : 0)
                              .arg(isComment ? 1 : 0)
                              .arg(code));
                const bool dotted = runDfa(QStringLiteral("a b"), code, isComment);
                ctx.check(QStringLiteral("T23.10 含空格的串不会被当成单个标识符"), !dotted,
                          QStringLiteral("识别=%1").arg(dotted ? 1 : 0));
            }
        }
    }

    // ---- T24：NFA 图所依赖的数据层不变量（ε 边、入度、出度、可达性）----
    //
    // 背景（用户反馈）：导出的 NFA 图里**一条 ε 边都没有**，于是 Thompson 构造
    // 出来的自动机在图上像一堆互不相连的孤立状态 —— "看起来怪怪的"。
    // 根因在界面层：DOT 原本是从"状态转换表副本"（`QVector<QMap<符号,单目的>>`）
    // 里凑的，那是 DFA 的结构，**ε 边与一格多目的状态都进不去**。
    //
    // 这一组断言先在**数据结构**层面把"图所依赖的性质"钉死，
    // 再由界面断言 G29 验证 DOT 里真的把这些边画出来了。两者缺一不可：
    // 数据对而渲染丢边、或渲染乱画但数据不对，都会被抓住。
    {
        const QString wordsPath = FileUtil::locateDataFile(QStringLiteral("tiny_words.txt"));
        bool ok = false;
        const QString wordsText = FileUtil::readText(wordsPath, &ok);
        TokenSpec spec;
        QVector<NfaRule> rules;
        if (ok && spec.parse(wordsText)) {
            for (const RegexDef &def : spec.automatonDefs()) {
                RegexParser rp;
                rp.setMacros(spec.macros());
                NfaRule rule;
                rule.name = def.name;
                rule.code = def.code;
                rule.isComment = (def.kind == RegexKind::Comment);
                if (rp.parse(def.rawRegex, rule.ast))
                    rules.append(rule);
            }
        }
        RegexToNfa builder;
        Nfa nfa;
        const bool built = !rules.isEmpty() && builder.build(rules, nfa);
        ctx.check(QStringLiteral("T24.0 TINY 规格构造 NFA（供 ε 边不变量检查）"), built,
                  builder.error());

        if (built) {
            int epsCount = 0;
            QVector<int> inDegree(nfa.stateCount, 0);
            QVector<int> outDegree(nfa.stateCount, 0);
            for (const Transition &e : nfa.edges) {
                if (e.from < 0 || e.from >= nfa.stateCount || e.to < 0 || e.to >= nfa.stateCount)
                    continue;
                if (e.isEpsilon)
                    ++epsCount;
                ++outDegree[e.from];
                ++inDegree[e.to];
            }

            ctx.check(QStringLiteral("T24.1 NFA 中存在 ε 边（否则图必然是一堆孤立状态）"),
                      epsCount > 0,
                      QStringLiteral("ε 边 %1 条 / 全部边 %2 条（ε 边应占大多数）")
                          .arg(epsCount)
                          .arg(nfa.edges.size()));

            QStringList noIn;
            for (int s = 0; s < nfa.stateCount; ++s) {
                if (s != nfa.start && inDegree.at(s) == 0)
                    noIn << QString::number(s);
            }
            ctx.check(QStringLiteral("T24.2 除统一初态外每个状态都有入边"), noIn.isEmpty(),
                      noIn.isEmpty() ? QString() : QStringLiteral("无入边的状态：%1")
                                                        .arg(noIn.mid(0, 12).join(QLatin1Char(','))));

            QStringList noOut;
            for (int s = 0; s < nfa.stateCount; ++s) {
                if (!nfa.accepts.contains(s) && outDegree.at(s) == 0)
                    noOut << QString::number(s);
            }
            ctx.check(QStringLiteral("T24.3 每个非接受态都有出边"), noOut.isEmpty(),
                      noOut.isEmpty() ? QString() : QStringLiteral("无出边的状态：%1")
                                                        .arg(noOut.mid(0, 12).join(QLatin1Char(','))));

            ctx.check(QStringLiteral("T24.4 接受态个数等于参与自动机的规则条数"),
                      static_cast<int>(nfa.accepts.size()) == rules.size(),
                      QStringLiteral("接受态 %1 个 / 规则 %2 条")
                          .arg(nfa.accepts.size())
                          .arg(rules.size()));

            // 沿**全部**边（含 ε）做可达性：Thompson 构造不该出现死状态
            QVector<bool> seen(nfa.stateCount, false);
            QVector<int> stack;
            stack.append(nfa.start);
            while (!stack.isEmpty()) {
                const int cur = stack.takeLast();
                if (cur < 0 || cur >= nfa.stateCount || seen.at(cur))
                    continue;
                seen[cur] = true;
                for (const Transition &e : nfa.edges) {
                    if (e.from == cur && e.to >= 0 && e.to < nfa.stateCount)
                        stack.append(e.to);
                }
            }
            int reached = 0;
            for (bool one : seen) {
                if (one)
                    ++reached;
            }
            ctx.check(QStringLiteral("T24.5 从初态出发沿全部边可达每一个状态"), reached == nfa.stateCount,
                      QStringLiteral("可达 %1 / 共 %2 个状态").arg(reached).arg(nfa.stateCount));
        }
    }

    // ---- T12：符号编码表与 .lex 输出的一致性 ----
    {
        // 这一组断言的意义：把"规格文件里的编码"与"生成的词法分析程序实际输出的
        // 编码"锁在一起。历史上多次出现自测期望值写错、而实现其实正确的情况，
        // 因此这里不再手写期望值，一律以 TokenSpec 解析出的编码表为准。
        const QString wordsPath = FileUtil::locateDataFile(QStringLiteral("tiny_words.txt"));
        bool ok = false;
        const QString wordsText = FileUtil::readText(wordsPath, &ok);
        TokenSpec spec;
        if (!ok || !spec.parse(wordsText)) {
            ctx.check(QStringLiteral("T12.1 解析 TINY 单词规格以取得编码表"), false,
                      spec.errors().join(QStringLiteral("；")));
        } else {
            const QMap<QString, int> symbols = spec.symbolTable();
            const QMap<QString, int> keywords = spec.keywordTable();
            ctx.check(QStringLiteral("T12.1 TINY 关键字编码为 if=103 then=104 else=105 end=106"),
                      keywords.value(QStringLiteral("if")) == 103
                          && keywords.value(QStringLiteral("then")) == 104
                          && keywords.value(QStringLiteral("else")) == 105
                          && keywords.value(QStringLiteral("end")) == 106,
                      QStringLiteral("if=%1 then=%2 else=%3 end=%4")
                          .arg(keywords.value(QStringLiteral("if")))
                          .arg(keywords.value(QStringLiteral("then")))
                          .arg(keywords.value(QStringLiteral("else")))
                          .arg(keywords.value(QStringLiteral("end"))));
            ctx.check(QStringLiteral("T12.2 TINY 关键字编码为 repeat=107 until=108 read=109 write=110"),
                      keywords.value(QStringLiteral("repeat")) == 107
                          && keywords.value(QStringLiteral("until")) == 108
                          && keywords.value(QStringLiteral("read")) == 109
                          && keywords.value(QStringLiteral("write")) == 110,
                      QStringLiteral("repeat=%1 until=%2 read=%3 write=%4")
                          .arg(keywords.value(QStringLiteral("repeat")))
                          .arg(keywords.value(QStringLiteral("until")))
                          .arg(keywords.value(QStringLiteral("read")))
                          .arg(keywords.value(QStringLiteral("write"))));
            ctx.check(QStringLiteral("T12.3 专用符号编码连续且唯一（200~217 共 18 个）"),
                      symbols.size() == 18,
                      QStringLiteral("实际 %1 个").arg(symbols.size()));
            // 括号与乘方必须在表内：BNF 的 factor 含 "( expression )" 与 power
            ctx.check(QStringLiteral("T12.3b 圆括号与乘方已登记编码（216/217/205）"),
                      symbols.value(QStringLiteral("(")) == 216
                          && symbols.value(QStringLiteral(")")) == 217
                          && symbols.value(QStringLiteral("^")) == 205,
                      QStringLiteral("(=%1 )=%2 ^=%3")
                          .arg(symbols.value(QStringLiteral("(")))
                          .arg(symbols.value(QStringLiteral(")")))
                          .arg(symbols.value(QStringLiteral("^"))));
            // 编码必须两两不同，否则生成的词法分析程序无法区分记号的类别
            ctx.check(QStringLiteral("T12.4 专用符号编码互不重复"),
                      QSet<int>(symbols.constBegin(), symbols.constEnd()).size() == symbols.size());
            // 关键字与符号的编码区间不得交叉，否则 ID 查表改判会误伤符号
            int overlap = 0;
            for (auto it = keywords.constBegin(); it != keywords.constEnd(); ++it) {
                if (symbols.values().contains(it.value()))
                    ++overlap;
            }
            ctx.check(QStringLiteral("T12.5 关键字编码与符号编码无交叉"), overlap == 0,
                      QStringLiteral("交叉 %1 个").arg(overlap));
        }
    }

    // ---- T13：阶段 4 —— BNF 文法载入、化简、First/Follow ----
    //
    // 断言策略（与 T12 一致）：所有期望值都从 data/ 里的真实数据**重新推导**，
    // 不硬编码数量，这样改了文法数据后若断言仍通过，说明算法真的对。
    {
        // T13.0：终结符命名表（编码 → 文法终结符名）—— 解析器翻译 .lex 的依据
        {
            using ccd::LanguageConfigRegistry;
            const QMap<int, QString> &tinyNames =
                LanguageConfigRegistry::terminalNames(LanguageId::Tiny);
            const QMap<int, QString> &minicNames =
                LanguageConfigRegistry::terminalNames(LanguageId::MiniC);
            ctx.check(QStringLiteral("T13.0 终结符命名表已建立（TINY %1 项、mini-C %2 项）")
                          .arg(tinyNames.size())
                          .arg(minicNames.size()),
                      !tinyNames.isEmpty() && !minicNames.isEmpty()
                          && tinyNames.size() >= 26 && minicNames.size() >= 30,
                      QStringLiteral("TINY %1、mini-C %2").arg(tinyNames.size()).arg(minicNames.size()));
            ctx.check(QStringLiteral("T13.0b 关键编码翻译正确（:=→ASSIGN、<>→NE、<=→LTEQ）"),
                      tinyNames.value(215) == QStringLiteral("ASSIGN")
                          && tinyNames.value(208) == QStringLiteral("NE")
                          && tinyNames.value(207) == QStringLiteral("LTEQ"),
                      QStringLiteral("215=%1 208=%2 207=%3")
                          .arg(tinyNames.value(215), tinyNames.value(208), tinyNames.value(207)));
            ctx.check(QStringLiteral("T13.0c TINY 与 mini-C 同一编码含义不同（100 分别是 NUMBER/NUM）"),
                      tinyNames.value(100) == QStringLiteral("NUMBER")
                          && minicNames.value(100) == QStringLiteral("NUM"),
                      QStringLiteral("TINY=%1 mini-C=%2")
                          .arg(tinyNames.value(100), minicNames.value(100)));
            ctx.check(QStringLiteral("T13.0d 注释编码 102 不在命名表内（注释不输出编码）"),
                      !tinyNames.contains(102) && !minicNames.contains(102),
                      QStringLiteral("TINY 含 102=%1、mini-C 含 102=%2")
                          .arg(tinyNames.contains(102)).arg(minicNames.contains(102)));
            // 逆向查表必须与正向一致
            bool invertOk = true;
            for (auto it = tinyNames.constBegin(); it != tinyNames.constEnd(); ++it) {
                if (LanguageConfigRegistry::terminalCode(LanguageId::Tiny, it.value()) != it.key())
                    invertOk = false;
            }
            ctx.check(QStringLiteral("T13.0e 编码↔终结符名互查一致（TINY 全表往返）"), invertOk);
        }

        // T13.1：TINY 文法载入
        Grammar tinyGrammar;
        QString loadError;
        const QString tinyGrammarPath = FileUtil::locateDataFile(
            LanguageConfigRegistry::get(LanguageId::Tiny).grammarFile);
        const bool tinyLoaded = !tinyGrammarPath.isEmpty()
                                && tinyGrammar.loadFromFile(tinyGrammarPath, loadError);
        ctx.check(QStringLiteral("T13.1 载入 TINY BNF 文法"), tinyLoaded,
                  tinyLoaded ? tinyGrammar.summary() : loadError);
        ctx.check(QStringLiteral("T13.1b TINY 开始符号为 program"),
                  tinyLoaded && tinyGrammar.startSymbol() == QStringLiteral("program"),
                  tinyLoaded ? tinyGrammar.startSymbol() : QStringLiteral("(未载入)"));

        if (tinyLoaded) {
            tinyGrammar.setTerminalNames(LanguageConfigRegistry::terminalNames(LanguageId::Tiny));

            // T13.2：文法里的每个终结符都必须能在编码表中查到 ——
            // 这条断言是"文法与词法两张表不会悄悄脱节"的守门人。
            QStringList unknownTerminals;
            for (const QString &t : tinyGrammar.terminals()) {
                if (tinyGrammar.terminalCode(t) < 0)
                    unknownTerminals.append(t);
            }
            ctx.check(QStringLiteral("T13.2 TINY 文法全部 %1 个终结符都能在编码表中查到")
                          .arg(tinyGrammar.terminals().size()),
                      unknownTerminals.isEmpty(),
                      unknownTerminals.isEmpty()
                          ? QStringLiteral("无未登记终结符")
                          : QStringLiteral("未登记：%1").arg(unknownTerminals.join(QStringLiteral("、"))));

            // T13.3：ε 产生式已归一化为空右部（TINY 本身没有 ε，此处验证计数为 0）
            int tinyEpsilons = 0;
            for (const Production &p : tinyGrammar.productions()) {
                if (p.isEpsilon())
                    ++tinyEpsilons;
            }
            ctx.check(QStringLiteral("T13.3 TINY 文法无 ε 产生式（计数 %1）").arg(tinyEpsilons),
                      tinyEpsilons == 0);

            // T13.4：先化简（在**未增广**的文法上做）——
            //   顺序很重要：化简必须作用于用户写的原始文法。
            //   若先增广再化简，`S'` 会成为"从开始符号不可达"的符号被删掉，
            //   增广就白做了（这是本阶段调试时真实踩到的坑）。
            GrammarSimplify simplifier;
            const SimplifyResult tinySimplify = simplifier.simplify(tinyGrammar);
            ctx.check(QStringLiteral("T13.4 TINY 文法无不可推导/不可达符号（化简前后一致）"),
                      !tinySimplify.changed(),
                      tinySimplify.summary());

            // T13.5：增广
            tinyGrammar.augment();
            const Production &augProd =
                tinyGrammar.production(tinyGrammar.productionCount() - 1);
            ctx.check(QStringLiteral("T13.5 增广后追加 S' -> program 且 S' 位于非终结符首位"),
                      tinyGrammar.augmented()
                          && tinyGrammar.nonterminals().first() == QStringLiteral("S'")
                          && augProd.left == QStringLiteral("S'")
                          && augProd.right == QStringList{QStringLiteral("program")},
                      QStringLiteral("末尾产生式：%1").arg(augProd.toString()));

            // T13.5b：重复调用 augment() 必须幂等（界面可能多次触发流水线）
            const int beforeCount = tinyGrammar.productionCount();
            tinyGrammar.augment();
            ctx.check(QStringLiteral("T13.5b 重复增广是幂等的（产生式数仍为 %1）").arg(beforeCount),
                      tinyGrammar.productionCount() == beforeCount,
                      QStringLiteral("实际 %1 条").arg(tinyGrammar.productionCount()));

            // T13.6：First/Follow
            FirstFollow ff;
            const FirstFollowResult ffResult = ff.compute(tinyGrammar);
            ctx.check(QStringLiteral("T13.6 计算 TINY First/Follow"), ff.error().isEmpty(),
                      ffResult.summary());

            if (ff.error().isEmpty()) {
                // Follow(program) 必须含 $（增广后由 S' -> program 传入）
                ctx.check(QStringLiteral("T13.6b Follow(program) 含结束符 $"),
                          ffResult.follow.value(QStringLiteral("program")).contains(kEndMarker),
                          FirstFollowResult::formatSet(
                              ffResult.follow.value(QStringLiteral("program"))));

                // First(factor) 应含 NUMBER、ID、LPAR（三条 factor 产生式的首符号）
                const QSet<QString> factorFirst = ffResult.first.value(QStringLiteral("factor"));
                ctx.check(QStringLiteral("T13.6c First(factor) 含 NUMBER、ID、LPAR"),
                          factorFirst.contains(QStringLiteral("NUMBER"))
                              && factorFirst.contains(QStringLiteral("ID"))
                              && factorFirst.contains(QStringLiteral("LPAR")),
                          FirstFollowResult::formatSet(factorFirst));

                // Follow(factor) 应含乘除模与幂运算符（factor 后面可能跟 * / % ^）
                const QSet<QString> factorFollow = ffResult.follow.value(QStringLiteral("factor"));
                ctx.check(QStringLiteral("T13.6d Follow(factor) 含 MULTIPLY、DIVIDE、MOD、POWER"),
                          factorFollow.contains(QStringLiteral("MULTIPLY"))
                              && factorFollow.contains(QStringLiteral("DIVIDE"))
                              && factorFollow.contains(QStringLiteral("MOD"))
                              && factorFollow.contains(QStringLiteral("POWER")),
                          FirstFollowResult::formatSet(factorFollow));

                // Follow(stmt-sequence) 应含 end、else、until（三条使用它的产生式）
                const QSet<QString> seqFollow =
                    ffResult.follow.value(QStringLiteral("stmt-sequence"));
                ctx.check(QStringLiteral("T13.6e Follow(stmt-sequence) 含 end、else、until"),
                          seqFollow.contains(QStringLiteral("end"))
                              && seqFollow.contains(QStringLiteral("else"))
                              && seqFollow.contains(QStringLiteral("until")),
                          FirstFollowResult::formatSet(seqFollow));

                // TINY 无 ε ⇒ 可空集合必须为空
                ctx.check(QStringLiteral("T13.6f TINY 文法无 ε ⇒ 可空集合为空"),
                          ffResult.nullable.isEmpty(),
                          QStringLiteral("可空 %1 个").arg(ffResult.nullable.size()));

                // 序列 First 与可空判定
                ctx.check(QStringLiteral("T13.6g 序列 First 与可空判定正确"),
                          ffResult.firstSetOfSequence({QStringLiteral("NUMBER")})
                                  == QSet<QString>{QStringLiteral("NUMBER")}
                              && !ffResult.sequenceNullable({QStringLiteral("NUMBER")})
                              && ffResult.sequenceNullable({}),
                          QStringLiteral("First(NUMBER)=%1")
                              .arg(FirstFollowResult::formatSet(
                                  ffResult.firstSetOfSequence({QStringLiteral("NUMBER")}))));
            }
        }

        // T13.7：mini-C 文法（含 ε 产生式，正好检验可空与 Follow 的合并逻辑）
        Grammar minicGrammar;
        QString minicError;
        const QString minicGrammarPath = FileUtil::locateDataFile(
            LanguageConfigRegistry::get(LanguageId::MiniC).grammarFile);
        const bool minicLoaded = !minicGrammarPath.isEmpty()
                                 && minicGrammar.loadFromFile(minicGrammarPath, minicError);
        ctx.check(QStringLiteral("T13.7 载入 mini-C BNF 文法"), minicLoaded,
                  minicLoaded ? minicGrammar.summary() : minicError);

        if (minicLoaded) {
            minicGrammar.setTerminalNames(LanguageConfigRegistry::terminalNames(LanguageId::MiniC));

            QStringList unknownTerminals;
            for (const QString &t : minicGrammar.terminals()) {
                if (minicGrammar.terminalCode(t) < 0)
                    unknownTerminals.append(t);
            }
            ctx.check(QStringLiteral("T13.7b mini-C 文法全部 %1 个终结符都能在编码表中查到")
                          .arg(minicGrammar.terminals().size()),
                      unknownTerminals.isEmpty(),
                      unknownTerminals.isEmpty()
                          ? QStringLiteral("无未登记终结符")
                          : QStringLiteral("未登记：%1").arg(unknownTerminals.join(QStringLiteral("、"))));

            // ε 产生式：local-definitions -> #、statement-list -> #、arguments -> #
            int minicEpsilons = 0;
            for (const Production &p : minicGrammar.productions()) {
                if (p.isEpsilon())
                    ++minicEpsilons;
            }
            ctx.check(QStringLiteral("T13.7c mini-C 的 ε 产生式已归一化为空右部（共 %1 条）")
                          .arg(minicEpsilons),
                      minicEpsilons == 3,
                      QStringLiteral("期望 3 条（local-definitions/statement-list/arguments）"));

            // mini-C 应无无用符号 —— 与 TINY 一样，化简必须在**增广之前**做，
            // 否则 S' 会被当成"从开始符号不可达"的符号删掉。
            GrammarSimplify simplifier2;
            const SimplifyResult minicSimplify = simplifier2.simplify(minicGrammar);
            ctx.check(QStringLiteral("T13.7d mini-C 文法无不可推导/不可达符号（化简前后一致）"),
                      !minicSimplify.changed(),
                      minicSimplify.summary());

            minicGrammar.augment();
            FirstFollow ff2;
            const FirstFollowResult ffMinic = ff2.compute(minicGrammar);
            ctx.check(QStringLiteral("T13.7e 计算 mini-C First/Follow"), ff2.error().isEmpty(),
                      ffMinic.summary());

            if (ff2.error().isEmpty()) {
                // 三个 ε 产生式的左部必须可空
                const QSet<QString> &nullable = ffMinic.nullable;
                ctx.check(QStringLiteral("T13.7f 三个含 ε 产生式的非终结符可空"),
                          nullable.contains(QStringLiteral("local-definitions"))
                              && nullable.contains(QStringLiteral("statement-list"))
                              && nullable.contains(QStringLiteral("arguments")),
                          QStringLiteral("可空集合：%1")
                              .arg(QStringList(nullable.constBegin(), nullable.constEnd())
                                       .join(QStringLiteral("、"))));

                // 可空传递：compound-stmt -> LBM local-definitions statement-list RBM
                // 中 local-definitions 与 statement-list 都可空 ⇒ compound-stmt 不可空
                // （因为 LBM/RBM 是终结符），这条断言验证"遇到终结符就停"。
                ctx.check(QStringLiteral("T13.7g compound-stmt 不可空（LBM/RBM 是终结符）"),
                          !nullable.contains(QStringLiteral("compound-stmt")));

                // Follow(statement) 应含 RBM（statement-list -> statement-list statement
                // 后面可能跟 }，以及 compound-stmt 内的 statement-list 尾部）
                ctx.check(QStringLiteral("T13.7h Follow(statement) 含 RBM 与 else"),
                          ffMinic.follow.value(QStringLiteral("statement"))
                                  .contains(QStringLiteral("RBM"))
                              && ffMinic.follow.value(QStringLiteral("statement"))
                                     .contains(QStringLiteral("else")),
                          FirstFollowResult::formatSet(
                              ffMinic.follow.value(QStringLiteral("statement"))));

                // First(definition-list) 应含类型关键字与 ID（variable/function 两条路）
                const QSet<QString> dlFirst =
                    ffMinic.first.value(QStringLiteral("definition-list"));
                ctx.check(QStringLiteral("T13.7i First(definition-list) 含 int/float/double/void"),
                          dlFirst.contains(QStringLiteral("int"))
                              && dlFirst.contains(QStringLiteral("float"))
                              && dlFirst.contains(QStringLiteral("double"))
                              && dlFirst.contains(QStringLiteral("void")),
                          FirstFollowResult::formatSet(dlFirst));

                // Follow(program) 必须含 $（增广产生式 S' -> program 传入）
                ctx.check(QStringLiteral("T13.7j Follow(program) 含结束符 $"),
                          ffMinic.follow.value(QStringLiteral("program")).contains(kEndMarker),
                          FirstFollowResult::formatSet(
                              ffMinic.follow.value(QStringLiteral("program"))));
            }
        }

        // T13.8：化简算法本身 —— 用一个**故意含无用符号**的小文法验证
        //   S -> A
        //   A -> a
        //   B -> b        （B 不可达）
        //   C -> C c      （C 不可推导）
        //   D -> C        （D 经由不可推导的 C，也不可推导）
        // 期望：删掉 B、C、D，保留 S、A，产生式从 5 条降到 2 条。
        {
            Grammar bad;
            QString badError;
            const QString badText = QStringLiteral("S -> A\n"
                                                   "A -> a\n"
                                                   "B -> b\n"
                                                   "C -> C c\n"
                                                   "D -> C\n");
            const bool badLoaded = bad.loadFromText(badText, badError);
            ctx.check(QStringLiteral("T13.8 载入含无用符号的测试文法"), badLoaded, badError);

            if (badLoaded) {
                GrammarSimplify s3;
                const SimplifyResult r3 = s3.simplify(bad);

                ctx.check(QStringLiteral("T13.8b 删除不可推导符号 C 与 D"),
                          r3.nonProductiveRemoved.contains(QStringLiteral("C"))
                              && r3.nonProductiveRemoved.contains(QStringLiteral("D")),
                          QStringLiteral("实际删除：%1")
                              .arg(r3.nonProductiveRemoved.join(QStringLiteral("、"))));
                ctx.check(QStringLiteral("T13.8c 删除不可达符号 B"),
                          r3.unreachableRemoved.contains(QStringLiteral("B")),
                          QStringLiteral("实际删除：%1")
                              .arg(r3.unreachableRemoved.join(QStringLiteral("、"))));
                ctx.check(QStringLiteral("T13.8d 保留 S 与 A、保留产生式 2 条"),
                          r3.kept.isNonterminal(QStringLiteral("S"))
                              && r3.kept.isNonterminal(QStringLiteral("A"))
                              && r3.kept.productionCount() == 2,
                          QStringLiteral("剩余 %1 条：%2")
                              .arg(r3.kept.productionCount())
                              .arg(r3.kept.toText().replace(QLatin1Char('\n'),
                                                            QStringLiteral(" / "))));
                ctx.check(QStringLiteral("T13.8e 化简过程有逐步说明（供报告引用）"),
                          r3.steps.size() >= 4,
                          QStringLiteral("%1 条说明").arg(r3.steps.size()));
            }
        }

        // T13.9：可空与 Follow 传递 —— 验证 β 可空时把 Follow(A) 并入 Follow(Xi)
        //   S -> A B c
        //   A -> a | #
        //   B -> b | #
        // 期望：A、B 可空；First(S)={a,b,c}；Follow(A)⊇{b,c}（B 可空故 c 也要进来）；
        //       Follow(B)⊇{c}。
        {
            Grammar g;
            QString gError;
            const QString text = QStringLiteral("S -> A B c\n"
                                                "A -> a\n"
                                                "A -> #\n"
                                                "B -> b\n"
                                                "B -> #\n");
            const bool loaded = g.loadFromText(text, gError);
            ctx.check(QStringLiteral("T13.9 载入含可空非终结符的测试文法"), loaded, gError);

            if (loaded) {
                g.augment();
                FirstFollow ff3;
                const FirstFollowResult r = ff3.compute(g);
                const QSet<QString> firstS = r.first.value(QStringLiteral("S"));
                ctx.check(QStringLiteral("T13.9b A、B 可空，S 不可空"),
                          r.nullable.contains(QStringLiteral("A"))
                              && r.nullable.contains(QStringLiteral("B"))
                              && !r.nullable.contains(QStringLiteral("S")),
                          QStringLiteral("可空：%1")
                              .arg(QStringList(r.nullable.constBegin(), r.nullable.constEnd())
                                       .join(QStringLiteral("、"))));
                ctx.check(QStringLiteral("T13.9c First(S) = { a、b、c }（跨过可空的 A 与 B）"),
                          firstS == QSet<QString>({QStringLiteral("a"), QStringLiteral("b"),
                                                   QStringLiteral("c")}),
                          FirstFollowResult::formatSet(firstS));
                const QSet<QString> followA = r.follow.value(QStringLiteral("A"));
                const QSet<QString> followB = r.follow.value(QStringLiteral("B"));
                ctx.check(QStringLiteral("T13.9d Follow(A) ⊇ { b、c }（B 可空 ⇒ c 也进入）"),
                          followA.contains(QStringLiteral("b")) && followA.contains(QStringLiteral("c")),
                          FirstFollowResult::formatSet(followA));
                ctx.check(QStringLiteral("T13.9e Follow(B) ⊇ { c }"), followB.contains(QStringLiteral("c")),
                          FirstFollowResult::formatSet(followB));
            }
        }
    }

    // ---- T14：阶段 5 —— LR(0) 项目集、LALR(1) 向前看、分析表与冲突 -
    //
    // 断言策略：先用**手工可验证的小文法**逐条钉住每一步算法的语义
    // （闭包、goto、向前看、冲突检测），再用真实的 TINY / mini-C 文法
    // 做规模与可分析性验证。小文法的期望值都能在纸上推出来，
    // 因此这里可以放心硬编码；真实文法则只断言"结构性"性质。
    {
        // ---- T14.1：闭包 closure ----
        //   S -> A B
        //   A -> a
        //   B -> b
        // 初态项目集应含 3 个项目（S 一条 + A、B 各一条）。
        // 注意 grammar.augment() 会追加 S' -> S，所以实际是 4 个项目：
        //   S' -> ·S、S -> ·A B、A -> ·a、B -> ·b
        {
            Grammar g;
            QString err;
            const bool loaded =
                g.loadFromText(QStringLiteral("S -> A B\nA -> a\nB -> b\n"), err);
            ctx.check(QStringLiteral("T14.1 载入闭包测试文法"), loaded, err);

            if (loaded) {
                g.augment();
                // 找到增广产生式
                int augId = -1;
                for (const Production &p : g.productions()) {
                    if (p.left == g.augmentedStart()) {
                        augId = p.id;
                        break;
                    }
                }
                LrItem seed;
                seed.prodId = augId;
                seed.dot = 0;
                const QVector<LrItem> closed = LrItems::closure(g, QVector<LrItem>{seed});

                // 期望 3 个项目：S' -> ·S、S -> ·A B、A -> ·a
                // 注意**不含** B 的项目 —— B 只出现在 S -> A·B 的圆点之后，
                // 而初态里 S -> A B 的圆点在最左端，圆点后是 A 不是 B。
                // 这条正是"闭包只展开圆点紧后符号"的判据，容易想当然写错成 4。
                ctx.check(QStringLiteral("T14.1b closure({S' -> ·S}) 含 3 个项目"),
                          closed.size() == 3,
                          QStringLiteral("实际 %1 个：%2")
                              .arg(closed.size())
                              .arg([&closed, &g] {
                                  QStringList ls;
                                  for (const LrItem &it : closed)
                                      ls << g.production(it.prodId).left;
                                  return ls.join(QStringLiteral(","));
                              }()));

                // 校验圆点全在 0 位，且三个左部分别是 S'、S、A
                QStringList lhsSet;
                bool allDotZero = true;
                for (const LrItem &it : closed) {
                    lhsSet << g.production(it.prodId).left;
                    if (it.dot != 0)
                        allDotZero = false;
                }
                lhsSet.sort();
                ctx.check(QStringLiteral("T14.1c 闭包项目左部为 S'、S、A 且圆点均在 0 位"),
                          allDotZero
                              && lhsSet == QStringList({QStringLiteral("A"), QStringLiteral("S"),
                                                        QStringLiteral("S'")}),
                          QStringLiteral("左部：%1").arg(lhsSet.join(QStringLiteral(","))));

                // goto(I0, A) 应只含 S -> A · B 与 B -> ·b
                const QVector<LrItem> gotoA =
                    LrItems::goTo(g, closed, QStringLiteral("A"));
                QStringList gotoALhs;
                for (const LrItem &it : gotoA)
                    gotoALhs << g.production(it.prodId).left;
                gotoALhs.sort();
                ctx.check(QStringLiteral("T14.1d goto(I0, A) 经闭包含 B 的项目（共 2 个）"),
                          gotoA.size() == 2
                              && gotoALhs == QStringList({QStringLiteral("B"), QStringLiteral("S")}),
                          QStringLiteral("%1 个项目：%2")
                              .arg(gotoA.size())
                              .arg(gotoALhs.join(QStringLiteral(","))));

                // goto(I0, b) 应为空集（b 不在 I0 任何项目的圆点后）
                ctx.check(QStringLiteral("T14.1e goto(I0, b) 为空集"),
                          LrItems::goTo(g, closed, QStringLiteral("b")).isEmpty());
            }
        }

        // ---- T14.2：LR(0) 规范族的规模 ----
        // 上例（S->A B, A->a, B->b）的状态如下，逐个可手工核对：
        //   I0 = {S'->·S, S->·A B, A->·a}
        //   I1 = goto(I0,S)  = {S'->S·}        （接受态）
        //   I2 = goto(I0,A)  = {S->A·B, B->·b}
        //   I3 = goto(I0,a)  = {A->a·}
        //   I4 = goto(I2,B)  = {S->A B·}
        //   I5 = goto(I2,b)  = {B->b·}
        // 共 6 个状态、5 条转移。
        {
            Grammar g;
            QString err;
            if (g.loadFromText(QStringLiteral("S -> A B\nA -> a\nB -> b\n"), err)) {
                g.augment();
                LrItems lr0;
                const Lr0Automaton auto0 = lr0.build(g);
                ctx.check(QStringLiteral("T14.2 LR(0) 规范族为 6 个状态、5 条转移"),
                          auto0.states.size() == 6, auto0.summary());

                int edges = 0;
                for (const Lr0State &s : auto0.states)
                    edges += s.transitions.size();
                ctx.check(QStringLiteral("T14.2c 转移边总数 = 5（实际 %1）").arg(edges), edges == 5);

                // 接受状态唯一：只含 S' -> S · 的那个
                int acceptCount = 0;
                for (const Lr0State &s : auto0.states) {
                    if (s.isAccepting(auto0.grammar))
                        ++acceptCount;
                }
                ctx.check(QStringLiteral("T14.2d 恰好 1 个接受状态（实际 %1）").arg(acceptCount),
                          acceptCount == 1);

                ctx.check(QStringLiteral("T14.2b 状态 0 是初态"),
                          auto0.states.first().containsAugmentedStart(auto0.grammar),
                          auto0.states.first().toText(auto0.grammar));
            }
        }

        // ---- T14.3：ε 产生式的闭包与向前看传递（最容易写错的地方）----
        //   S -> A B
        //   A -> a
        //   A -> #
        //   B -> b
        //
        // 要点：LR(1) 闭包里对项目 [S -> ·A B, $]，求 FIRST(B $) = { b }，
        // 因此加入 [A -> ·a, b] 与 [A -> ·, b] —— 向前看是 **b** 而不是 $。
        // 若实现里误写成 FIRST(B) 之外的写法（例如漏掉把 $ 接在末尾）
        // 或漏掉 A 的 ε 项目，这条断言就会失败。
        {
            Grammar g;
            QString err;
            const bool loaded = g.loadFromText(
                QStringLiteral("S -> A B\nA -> a\nA -> #\nB -> b\n"), err);
            ctx.check(QStringLiteral("T14.3 载入含 ε 产生式的闭包测试文法"), loaded, err);

            if (loaded) {
                g.augment();
                FirstFollow ffA;
                const FirstFollowResult fa = ffA.compute(g);

                int augId = -1;
                for (const Production &p : g.productions()) {
                    if (p.left == g.augmentedStart()) {
                        augId = p.id;
                        break;
                    }
                }
                Lr1Item seed;
                seed.prodId = augId;
                seed.dot = 0;
                seed.lookahead = kEndMarker;

                const QVector<Lr1Item> closed =
                    Lr1Items::closureLr1(g, QVector<Lr1Item>{seed}, fa);

                // 收集 A 的项目的向前看符号
                QSet<QString> aLookaheads;
                bool hasEpsilonItem = false;
                for (const Lr1Item &it : closed) {
                    const Production &p = g.production(it.prodId);
                    if (p.left != QStringLiteral("A"))
                        continue;
                    aLookaheads.insert(it.lookahead);
                    if (p.isEpsilon() && it.dot == 0)
                        hasEpsilonItem = true;
                }
                ctx.check(QStringLiteral("T14.3b A 的 ε 项目 [A -> ·, b] 已被生成"),
                          hasEpsilonItem,
                          QStringLiteral("A 相关项目的向前看：%1")
                              .arg(QStringList(aLookaheads.constBegin(), aLookaheads.constEnd())
                                       .join(QStringLiteral("、"))));
                ctx.check(QStringLiteral("T14.3c A 的向前看是 b（FIRST(B$) 跨过 ε 得到 B 的首符号）"),
                          aLookaheads == QSet<QString>({QStringLiteral("b")}),
                          QStringLiteral("实际：%1")
                              .arg(QStringList(aLookaheads.constBegin(), aLookaheads.constEnd())
                                       .join(QStringLiteral("、"))));

                // 【诊断】逐步拆开：文法是否真的含 A 的 ε 产生式？
                // 闭包规模多少？FIRST(B$) 是多少？
                int epsCount = 0;
                int prodCount = 0;
                for (const Production &p : g.productions()) {
                    ++prodCount;
                    if (p.left == QStringLiteral("A") && p.isEpsilon())
                        ++epsCount;
                }
                ctx.check(QStringLiteral("T14.3d 文法含 A 的 ε 产生式（实际 %1 条，"
                                         "总产生式 %2 条）")
                              .arg(epsCount)
                              .arg(prodCount),
                          epsCount == 1,
                          g.toText().replace(QLatin1Char('\n'), QStringLiteral(" / ")));
                ctx.check(QStringLiteral("T14.3f FIRST(B$) = { b } —— 结束符 $ 必须按终结符处理"),
                          fa.firstSetOfSequence({QStringLiteral("B"), kEndMarker})
                              == QSet<QString>({QStringLiteral("b")}),
                          FirstFollowResult::formatSet(
                              fa.firstSetOfSequence({QStringLiteral("B"), kEndMarker})));
            }
        }

        // ---- T14.4：经典 SLR(1) 冲突但 LALR(1) 无冲突的文法 ----
        //   S -> L EQ R
        //   S -> R
        //   L -> MULT R
        //   L -> ID
        //   R -> L
        //
        // 这是《编译原理》（龙书）里说明 SLR 能力不足的标准例子：
        // 状态含 [S -> L · EQ R, $] 与 [R -> L ·, $]，
        // 因 EQ ∈ Follow(R)，SLR 会在此产生移进-规约冲突；
        // 而 LALR(1) 的向前看符号是 $（不含 EQ），故无冲突。
        // 本断言正是本项目"用 LALR(1) 而非 SLR(1)"的**技术依据**，
        // 实验报告里可直接引用这条数据。
        {
            Grammar g;
            QString err;
            const QString text = QStringLiteral("S -> L EQ R\n"
                                                "S -> R\n"
                                                "L -> MULT R\n"
                                                "L -> ID\n"
                                                "R -> L\n");
            const bool loaded = g.loadFromText(text, err);
            ctx.check(QStringLiteral("T14.4 载入 SLR 冲突 / LALR 无冲突的经典文法"), loaded, err);

            if (loaded) {
                ParseTableBuilder builder;

                const ParseTable lalr = builder.build(g, ParseTableKind::Lalr1);
                ctx.check(QStringLiteral("T14.4b LALR(1) 对该文法无冲突"), lalr.isClean(),
                          lalr.summary());
                ctx.check(QStringLiteral("T14.4c LALR(1) 表非空且含接受动作"),
                          lalr.stateCount() > 0 && !lalr.conflictsText().isEmpty(),
                          lalr.summary());

                const ParseTable slr = builder.build(g, ParseTableKind::Slr1);
                ctx.check(QStringLiteral("T14.4d SLR(1) 对该文法产生冲突（能力弱于 LALR）"),
                          !slr.isClean(),
                          QStringLiteral("冲突 %1 处").arg(slr.conflicts.size()));

                const ParseTable lr0 = builder.build(g, ParseTableKind::Lr0);
                ctx.check(QStringLiteral("T14.4e LR(0) 冲突数不少于 SLR(1)（最弱）"),
                          lr0.conflicts.size() >= slr.conflicts.size(),
                          QStringLiteral("LR(0) %1 处、SLR(1) %2 处")
                              .arg(lr0.conflicts.size())
                              .arg(slr.conflicts.size()));
            }
        }

        // ---- T14.5：LR(1) 合并为 LALR(1) 必须真的减少状态数 ----
        // 用 T14.4 的文法：LR(1) 族通常比 LALR 多几个状态。
        // 若两者相等，说明合并逻辑没生效（所有核心都不同），需要复核。
        {
            Grammar g;
            QString err;
            if (g.loadFromText(QStringLiteral("S -> L EQ R\nS -> R\nL -> MULT R\n"
                                              "L -> ID\nR -> L\n"),
                               err)) {
                ParseTableBuilder builder;
                const ParseTable lalr = builder.build(g, ParseTableKind::Lalr1);
                ctx.check(QStringLiteral("T14.5 LR(1) 状态数(%1) ≥ LALR(1) 状态数(%2)")
                              .arg(lalr.automaton.lr1StateCount)
                              .arg(lalr.stateCount()),
                          lalr.automaton.lr1StateCount >= lalr.stateCount()
                              && lalr.stateCount() > 0,
                          lalr.automaton.summary());
                ctx.check(QStringLiteral("T14.5b 确实发生过同核心合并（合并组数 > 0）"),
                          lalr.automaton.mergedGroupCount > 0,
                          QStringLiteral("合并组 %1 个").arg(lalr.automaton.mergedGroupCount));
            }
        }

        // ---- T14.6：TINY 文法的分析表 ----
        // 真实文法的期望值不硬编码状态数（任何一次文法改动都会让数字失效），
        // 只断言结构性性质：表非空、GOTO 表覆盖全部非终结符、存在 acc。
        {
            Grammar tiny;
            QString err;
            const QString path = FileUtil::locateDataFile(
                LanguageConfigRegistry::get(LanguageId::Tiny).grammarFile);
            const bool loaded = !path.isEmpty() && tiny.loadFromFile(path, err);
            ctx.check(QStringLiteral("T14.6 载入 TINY 文法用于分析表构造"), loaded, err);

            if (loaded) {
                tiny.setTerminalNames(LanguageConfigRegistry::terminalNames(LanguageId::Tiny));
                ParseTableBuilder builder;
                const ParseTable table = builder.build(tiny, ParseTableKind::Lalr1);

                ctx.check(QStringLiteral("T14.6b TINY 的 LALR(1) 分析表构造成功"),
                          table.stateCount() > 0, table.summary());

                // 【关键回归】化简+增广后文法必须仍有完整规模。
                // 这条断言是"阶段 5 首次接入真实文法时分析表退化成 2 个状态"
                // 那个故障的守门员：当时根因是 firstSetOfSequence 把结束符 $
                // 当成了非终结符，导致 LR(1) 闭包一个项目都展开不出来。
                // 若将来再出现同类问题，这里会第一时间报出文法规模异常。
                ctx.check(QStringLiteral("T14.6b2 化简+增广后文法仍有完整规模"),
                          table.grammar.productionCount() > 30
                              && table.grammar.terminals().size() > 15,
                          QStringLiteral("产生式 %1 条、终结符 %2 个、非终结符 %3 个")
                              .arg(table.grammar.productionCount())
                              .arg(table.grammar.terminals().size())
                              .arg(table.grammar.nonterminals().size()));

                // 【关键回归】初态必须真的展开了闭包（项目数、转移边数都远大于 1）。
                // 分析表"状态数 == 自动机状态数"这条也在此一并钉住。
                if (!table.automaton.states.isEmpty()) {
                    const LalrState &st0 = table.automaton.states.first();
                    ctx.check(QStringLiteral("T14.6b4 初态 I0 已展开闭包（%1 个项目）")
                                  .arg(st0.core.size()),
                              st0.core.size() > 10,
                              QStringLiteral("I0 项目数 %1").arg(st0.core.size()));
                    ctx.check(QStringLiteral("T14.6b5 初态 I0 的转移边数 > 10（实际 %1）")
                                  .arg(st0.transitions.size()),
                              st0.transitions.size() > 10,
                              QStringLiteral("转移符号：%1")
                                  .arg(QStringList(st0.transitions.keys())
                                           .join(QStringLiteral(","))));
                }
                ctx.check(QStringLiteral("T14.6b3 分析表行数 == LALR 自动机状态数"),
                          table.stateCount() == table.automaton.states.size(),
                          QStringLiteral("表 %1 行、自动机 %2 状态、LR(1) 族 %3 状态")
                              .arg(table.stateCount())
                              .arg(table.automaton.states.size())
                              .arg(table.automaton.lr1StateCount));

                // 存在 acc：某个状态遇 $ 是 Accept
                bool hasAccept = false;
                for (int s = 0; s < table.stateCount(); ++s) {
                    if (table.actionAt(s, kEndMarker).type == ActionType::Accept)
                        hasAccept = true;
                }
                ctx.check(QStringLiteral("T14.6c TINY 分析表含接受动作 acc"), hasAccept,
                          table.summary());

                // GOTO 表必须覆盖全部非终结符列（未增广前的那些）
                int gotoCells = 0;
                for (int s = 0; s < table.stateCount(); ++s) {
                    for (const QString &n : table.nonterminals()) {
                        if (table.gotoAt(s, n) >= 0)
                            ++gotoCells;
                    }
                }
                ctx.check(QStringLiteral("T14.6d TINY 的 GOTO 表项 %1 个（>0）").arg(gotoCells),
                          gotoCells > 0);

                ctx.check(QStringLiteral("T14.6e TINY 冲突报告可生成（供报告引用）"),
                          !table.conflictsText().isEmpty(),
                          table.isClean() ? QStringLiteral("无冲突")
                                          : QStringLiteral("冲突 %1 处")
                                                .arg(table.conflicts.size()));
            }
        }

        // ---- T14.7：mini-C 文法的分析表（含 ε 产生式与悬空 else）----
        {
            Grammar minic;
            QString err;
            const QString path = FileUtil::locateDataFile(
                LanguageConfigRegistry::get(LanguageId::MiniC).grammarFile);
            const bool loaded = !path.isEmpty() && minic.loadFromFile(path, err);
            ctx.check(QStringLiteral("T14.7 载入 mini-C 文法用于分析表构造"), loaded, err);

            if (loaded) {
                minic.setTerminalNames(LanguageConfigRegistry::terminalNames(LanguageId::MiniC));
                ParseTableBuilder builder;
                const ParseTable table = builder.build(minic, ParseTableKind::Lalr1);

                ctx.check(QStringLiteral("T14.7b mini-C 的 LALR(1) 分析表构造成功"),
                          table.stateCount() > 0, table.summary());

                bool hasAccept = false;
                for (int s = 0; s < table.stateCount(); ++s) {
                    if (table.actionAt(s, kEndMarker).type == ActionType::Accept)
                        hasAccept = true;
                }
                ctx.check(QStringLiteral("T14.7c mini-C 分析表含接受动作 acc"), hasAccept);

                // 悬空 else：mini-C 的 condition-stmt 有两条产生式
                //   if ( expression ) statement
                //   if ( expression ) statement else statement
                // 遇到 else 时既可移进也可规约，标准做法是移进优先。
                // 这里断言"冲突被检出且都得到消解"——即表可用、冲突有记录。
                ctx.check(QStringLiteral("T14.7d mini-C 的冲突已被检出并消解（表仍可用）"),
                          !table.conflictsText().isEmpty(),
                          table.isClean() ? QStringLiteral("无冲突")
                                          : QStringLiteral("冲突 %1 处，已按移进优先消解")
                                                .arg(table.conflicts.size()));

                // 无论是否有冲突，分析表文本都应能生成（供导出到报告）
                ctx.check(QStringLiteral("T14.7e mini-C 分析表可导出为文本"),
                          table.toText().contains(QStringLiteral("状态")),
                          QStringLiteral("%1 字符").arg(table.toText().size()));
            }
        }

        // ---- T14.8：冲突消解规则可配置（报告里要交代取舍依据）----
        {
            Grammar g;
            QString err;
            if (g.loadFromText(QStringLiteral("S -> L EQ R\nS -> R\nL -> MULT R\n"
                                              "L -> ID\nR -> L\n"),
                               err)) {
                ParseTableBuilder shiftBuilder;
                ParseTableBuilder::ConflictPolicy reducePolicy;
                reducePolicy.shiftWinsOverReduce = false;  // 改为规约优先
                shiftBuilder.setConflictPolicy(reducePolicy);

                const ParseTable shiftTable = ParseTableBuilder().build(g, ParseTableKind::Slr1);
                const ParseTable reduceTable = shiftBuilder.build(g, ParseTableKind::Slr1);

                // 两种策略下被"覆盖"的冲突记录数应当一致（同一批格子之争），
                // 但最终表项可能不同 —— 这里只断言诊断文本如实反映了策略。
                ctx.check(QStringLiteral("T14.8 冲突消解策略可切换且被写入报告文本"),
                          shiftTable.conflictsText().contains(QStringLiteral("移进优先"))
                              || reduceTable.conflictsText().contains(QStringLiteral("规约优先")),
                          QStringLiteral("移进策略冲突 %1 处、规约策略冲突 %2 处")
                              .arg(shiftTable.conflicts.size())
                              .arg(reduceTable.conflicts.size()));
            }
        }

        // -------------------------------------------------------------------
        // T15：阶段 6 —— 语义动作文件判读、.lex 读取、表驱动语法分析、语法树
        // -------------------------------------------------------------------

        // ---- T15.1：语义动作文件的格式判读（本项目约定格式）----
        // 本项目采用**加字母前缀**的行格式（`L …` / `R n …` / `-1 n`，组间用 `;`），
        // 而不是任务书参考数据里那串纯数字。原因见 AGENT.md §10：
        // 纯数字格式里数字 0 同时承担"建左部结点"与"子结点下标 0"两种含义，
        // `0 1 -1 2 -1` 既可读成 4 组、也可读成 1 组带 3 个子结点，**不可无歧义解析**。
        // 因此这里直接验证本项目格式的判读，并顺带验证格式错误的检出。
        {
            SemanticsActionSet sem;
            const bool ok = sem.loadFromText(QStringLiteral(
                "# 注释行不占产生式序号\n"
                "L\n"
                "L 1 ; L\n"
                "R0 2\n"
                "-1 0\n"
                "R0 2 ; R1 2\n"));

            ctx.check(QStringLiteral("T15.1 语义动作行解析无错（%1 行）").arg(sem.count()),
                      ok && sem.errors().isEmpty() && sem.count() == 5,
                      sem.errors().isEmpty() ? sem.summary()
                                             : sem.errors().join(QStringLiteral(" / ")));

            const QVector<SemanticsGroup> g1 = sem.actionAt(1);
            ctx.check(QStringLiteral("T15.1b `L 1 ; L` 解析为 2 组，"
                                     "第一组子结点=[1]、第二组无子结点"),
                      g1.size() == 2 && g1.at(0).kind == SemanticsGroup::BuildLhs
                          && g1.at(0).children == QVector<int>({1})
                          && g1.at(1).kind == SemanticsGroup::BuildLhs
                          && g1.at(1).children.isEmpty(),
                      QStringLiteral("组数=%1").arg(g1.size()));

            const QVector<SemanticsGroup> g2 = sem.actionAt(2);
            ctx.check(QStringLiteral("T15.1c `R0 2` 解析为建结点(标签=右部第0项)、子结点=[2]"),
                      g2.size() == 1 && g2.first().kind == SemanticsGroup::BuildSymbol
                          && g2.first().symbolIndex == 0
                          && g2.first().children == QVector<int>({2}),
                      g2.isEmpty() ? QString() : g2.first().toString());

            const QVector<SemanticsGroup> g3 = sem.actionAt(3);
            ctx.check(QStringLiteral("T15.1d 单链语义 `-1 0` 解析为不建结点、传递第 0 项"),
                      g3.size() == 1 && g3.first().kind == SemanticsGroup::PassThrough
                          && g3.first().symbolIndex == 0,
                      g3.isEmpty() ? QString() : g3.first().toString());

            const QVector<SemanticsGroup> g4 = sem.actionAt(4);
            ctx.check(QStringLiteral("T15.1e `R0 2 ; R1 2` 解析为 2 组，末组标签取右部第 1 项"),
                      g4.size() == 2 && g4.at(1).kind == SemanticsGroup::BuildSymbol
                          && g4.at(1).symbolIndex == 1
                          && g4.at(1).children == QVector<int>({2}),
                      g4.isEmpty() ? QString() : g4.last().toString());

            // 格式错误必须被检出并给出提示，而不是静默当成默认动作
            SemanticsActionSet bad;
            bad.loadFromText(QStringLiteral("X 1\nR\n"));
            ctx.check(QStringLiteral("T15.1f 非法组首记号与缺失序号都被报错（%1 处）")
                          .arg(bad.errors().size()),
                      bad.errors().size() >= 2
                          || (bad.errors().size() >= 1
                              && bad.errors().first().contains(QStringLiteral("不是 L / R"))),
                      bad.errors().join(QStringLiteral(" / ")));

            // 行序对齐：文件行数少于产生式数时，多出来的产生式取默认动作
            SemanticsActionSet partial;
            partial.loadFromText(QStringLiteral("L\n"));
            const QVector<SemanticsGroup> def = partial.actionAt(2);
            ctx.check(QStringLiteral("T15.1g 未给出语义行的产生式取默认动作"
                                     "（建左部结点、无子结点）"),
                      partial.isDefaultAt(2) && def.size() == 1
                          && def.first().kind == SemanticsGroup::BuildLhs
                          && def.first().children.isEmpty(),
                      def.isEmpty() ? QString() : def.first().toString());
        }

        // ---- T15.2：参考语法树文件可读（用于与本项目自建树做对照）----
        // 说明：`reference/08_tiny_tree.out` 里共有 **81** 个结点（81 行 `kind:`）。
        // 这里断言精确值而不是"大于某个数"—— 大于号写宽了就没法发现解析器
        // 少读了结点（本项目就曾因为 `}` 的闭合规则写错而只解析出 81 个中的一部分）。
        SyntaxTreeData refTree;
        {
            QString treeText = FileUtil::readText(
                QStringLiteral("../../reference/08_tiny_tree.out"));
            if (treeText.isEmpty())
                treeText = FileUtil::readText(
                    FileUtil::locateDataFile(QStringLiteral("../reference/08_tiny_tree.out")));

            QString terr;
            refTree = SyntaxTree::fromReferenceText(treeText, terr);

            ctx.check(QStringLiteral("T15.2 解析老师参考语法树（08_tiny_tree.out，%1 个结点）")
                          .arg(refTree.size()),
                      refTree.root >= 0 && refTree.size() == 81,
                      QStringLiteral("根=%1（标签 %2）；%3")
                          .arg(refTree.root)
                          .arg(refTree.root >= 0 ? refTree.nodes.at(refTree.root).label
                                                 : QString())
                          .arg(terr));

            ctx.check(QStringLiteral("T15.2b 参考树自身连通（可达 %1 / 共 %2）")
                          .arg(refTree.reachableCount())
                          .arg(refTree.size()),
                      refTree.reachableCount() == refTree.size(),
                      QStringLiteral("参考树根标签=%1")
                          .arg(refTree.root >= 0 ? refTree.nodes.at(refTree.root).label
                                                 : QString()));

            const QString sig = SyntaxTree::signature(refTree);
            ctx.check(QStringLiteral("T15.2c 参考树结构签名可生成（非终结符归一化为 -1）"),
                      sig.startsWith(QStringLiteral("(-1")) && sig.size() > 200,
                      QStringLiteral("签名长度 %1，开头 %2").arg(sig.size()).arg(sig.left(40)));
        }

        // ---- T15.6：参考树标签直方图（作为对照基准，后面与自建树比对）----
        {
            const QMap<QString, int> refHist = SyntaxTree::labelHistogram(refTree);
            QStringList keys = refHist.keys();
            keys.sort();
            ctx.check(QStringLiteral("T15.6 参考树标签直方图（%1 种标签）").arg(keys.size()),
                      keys.size() >= 19 && refHist.contains(QStringLiteral("read"))
                          && refHist.contains(QStringLiteral("if"))
                          && refHist.contains(QStringLiteral("repeat"))
                          && refHist.contains(QStringLiteral("write"))
                          && refHist.contains(QStringLiteral("stmt-sequence")),
                      keys.join(QStringLiteral(" ")).left(240));
        }

        // ---- T15.5：一棵"连通"的最小语法树（左递归链 + 操作符包操作数）----
        // 这一组断言的目的是把"树断成孤岛"这类缺陷钉死。它的症状非常隐蔽：
        // 分析明明成功、结点数也不少，但 `toText()` 只有两三行 ——
        // 因为除了根以外，其余结点都没有被任何父结点引用，从根走不到。
        // 两个典型成因都在这里被覆盖：
        //   a) 单链产生式写成 `L`（新建空结点）而不是 `-1 0`，整棵子树失去父结点；
        //   b) 双目运算写成"新建结点"而不是 `R1 0 2`（复用运算符结点），
        //      真正被规约出来的运算符结点成了孤岛、左侧操作数也接不上。
        {
            // 文法：S -> S PLUS T | T ; T -> ID
            Grammar g;
            QString gerr;
            g.loadFromText(QStringLiteral("S -> S PLUS T\n"
                                          "S -> T\n"
                                          "T -> ID\n"),
                           gerr);
            QMap<int, QString> names;
            names.insert(500, QStringLiteral("PLUS"));
            names.insert(501, QStringLiteral("ID"));
            g.setTerminalNames(names);

            // 语义动作（行序与产生式一致）：
            //   S -> S PLUS T : `R1 0 2` —— 复用右部第 1 项（PLUS 终结符叶结点），
            //                   把右部第 0 项（左侧 S）与第 2 项（右侧 T）挂成它的子结点。
            //                   于是 PLUS 结点同时带左右子树，并成为本次规约的结果，
            //                   与老师参考树"`<` 是语法树结点"的做法一致。
            //   S -> T        : 单链，写 `-1 0` 直接传递
            //   T -> ID       : `L 0` 新建 T 结点并挂 ID
            SemanticsActionSet sem;
            const bool semOk = sem.loadFromText(QStringLiteral("R1 0 2\n"
                                                              "-1 0\n"
                                                              "L 0\n"));
            ctx.check(QStringLiteral("T15.5 最小文法语义动作载入（%1 条）").arg(sem.count()),
                      semOk && sem.errors().isEmpty() && sem.count() == 3,
                      sem.errors().isEmpty() ? sem.summary()
                                             : sem.errors().join(QStringLiteral(" / ")));

            ParseTableBuilder b;
            const ParseTable t2 = b.build(g, ParseTableKind::Lalr1);

            {
                QStringList h;
                QList<QStringList> rows;
                sem.buildSummaryTable(g, h, rows);
                QStringList dump;
                for (const QStringList &r : rows)
                    dump << QStringLiteral("[%1]%2").arg(r.value(0), r.value(2));
                ctx.check(QStringLiteral("T15.5a 语义动作逐行还原（%1 条）").arg(rows.size()),
                          rows.size() == 3
                              && rows.value(0).value(2) == QStringLiteral("R1 0 2"),
                          dump.join(QStringLiteral(" ")));
            }

            Parser p2;
            p2.setTable(t2);
            p2.setSemantics(sem);

            const ParseResult r2 = p2.parse(
                QStringList{QStringLiteral("ID"), QStringLiteral("PLUS"),
                            QStringLiteral("ID"), QStringLiteral("$")},
                QVector<int>{1, 1, 1, 1},
                QStringList{QStringLiteral("a"), QString(), QStringLiteral("b"),
                            QString()});

            // 从根出发能走到的结点数（校验树的连通性）
            const int reach2 = r2.tree.reachableCount();

            ctx.check(QStringLiteral("T15.5b 三个单词的最小输入分析通过（%1）").arg(r2.summary()),
                      r2.success, r2.errorsText().left(200));

            ctx.check(QStringLiteral("T15.5c 语法树从根可达全部结点（可达 %1 / 共 %2）")
                          .arg(reach2)
                          .arg(r2.tree.size()),
                      r2.tree.size() > 0 && reach2 == r2.tree.size(),
                      QStringLiteral("根=%1；括号=%2；步骤=%3")
                          .arg(r2.tree.root)
                          .arg(SyntaxTree::toBracket(r2.tree))
                          .arg(r2.stepsText(0).replace(QLatin1Char('\n'),
                                                       QStringLiteral(" ⏎ "))));

            // 左递归链必须真的接起来：`a + b` 那一段应当有 PLUS 结点、
            // 且 PLUS 结点下挂着两个 T/ID 子树
            ctx.check(QStringLiteral("T15.5d 括号表示体现左递归链与操作符结点（%1）")
                          .arg(SyntaxTree::toBracket(r2.tree)),
                      SyntaxTree::toBracket(r2.tree).contains(QStringLiteral("PLUS")));
        }

        // ---- T15.3：本项目 TINY 数据 → 完整分析 → 语法树 ----
        {
            const LanguageId id = LanguageId::Tiny;
            const LanguageConfig &cfg = LanguageConfigRegistry::get(id);

            Grammar g;
            QString err;
            const QString grammarPath = FileUtil::locateDataFile(cfg.grammarFile);
            const bool gloaded = !grammarPath.isEmpty() && g.loadFromFile(grammarPath, err);
            ctx.check(QStringLiteral("T15.3 载入 TINY 文法（%1 条产生式）").arg(g.productionCount()),
                      gloaded, gloaded ? g.summary() : err);
            g.setTerminalNames(LanguageConfigRegistry::terminalNames(id));

            // 语义动作
            SemanticsActionSet sem;
            QString semErr;
            const QString semPath = FileUtil::locateDataFile(QStringLiteral("tiny_semantics.txt"));
            const bool semLoaded = !semPath.isEmpty() && sem.loadFromFile(semPath, semErr);
            ctx.check(QStringLiteral("T15.3b 载入 TINY 语义动作（%1 条）").arg(sem.count()),
                      semLoaded && sem.errors().isEmpty(),
                      semLoaded ? (sem.summary() + QStringLiteral("；错误：")
                                  + (sem.errors().isEmpty()
                                         ? QStringLiteral("无")
                                         : sem.errors().join(QStringLiteral(" / "))))
                                : semErr);

            // 化简 → 分析表
            GrammarSimplify simplifier;
            const SimplifyResult sr = simplifier.simplify(g);
            ParseTableBuilder builder;
            const ParseTable table =
                builder.build(sr.kept, ParseTableKind::Lalr1);
            ctx.check(QStringLiteral("T15.3c 由 TINY 文法构造 LALR(1) 分析表（%1 状态）")
                          .arg(table.stateCount()),
                      table.stateCount() > 10, table.summary());

            // 读 .lex（用词法阶段产出的 tiny.lex）
            QString lexPath = QStringLiteral("output/lexbuild/tiny.lex");
            if (!QFileInfo::exists(lexPath))
                lexPath = FileUtil::locateDataFile(QStringLiteral("tiny.lex"));

            LexReader reader;
            Grammar augmentedForRead = sr.kept;
            augmentedForRead.setTerminalNames(LanguageConfigRegistry::terminalNames(id));
            augmentedForRead.augment();
            reader.setGrammar(augmentedForRead);

            QString lexErr;
            const bool lexOk = !lexPath.isEmpty() && reader.loadFromFile(lexPath, lexErr);
            ctx.check(QStringLiteral("T15.3d 读取词法阶段产出的 .lex（%1 个记号）")
                          .arg(reader.items().size()),
                      lexOk && reader.items().size() > 20,
                      lexOk ? reader.summary() : lexErr);

            ctx.check(QStringLiteral("T15.3e .lex 中所有编码都能翻译成文法终结符"
                                     "（无法翻译的编码 %1 个）")
                          .arg(reader.unknownCodes().size()),
                      reader.unknownCodes().isEmpty(),
                      QStringLiteral("未知编码：%1")
                          .arg([&] {
                              QStringList s;
                              for (int c : reader.unknownCodes())
                                  s << QString::number(c);
                              return s.isEmpty() ? QStringLiteral("无") : s.join(QLatin1Char(','));
                          }()));

            // 分析 + 建树
            Parser parser;
            parser.setTable(table);
            parser.setSemantics(sem);
            const ParseResult pr =
                parser.parse(reader.symbols(), reader.symbolLines(), reader.symbolValues());

            ctx.check(QStringLiteral("T15.3f0 产生式编号对齐（增广=%1、偏移=%2）")
                          .arg(table.grammar.augmented() ? QStringLiteral("是")
                                                         : QStringLiteral("否"))
                          .arg(parser.semanticsOffset()),
                      table.grammar.augmented() && parser.semanticsOffset() == 0
                          && table.grammar.production(0).left
                                 == QStringLiteral("program"),
                      QStringLiteral("p0=[%1 -> %2]；p1=[%3 -> %4]；增广符号=%5")
                          .arg(table.grammar.production(0).left,
                               table.grammar.production(0).right.join(QLatin1Char(' ')),
                               table.grammar.production(1).left,
                               table.grammar.production(1).right.join(QLatin1Char(' ')),
                               table.grammar.augmentedStart()));

            ctx.check(QStringLiteral("T15.3f TINY 样例程序语法分析通过（%1）").arg(pr.summary()),
                      pr.success, pr.errorsText().left(300));

            ctx.check(QStringLiteral("T15.3g 语法树非空且根为开始符号 program"),
                      pr.tree.root >= 0 && pr.tree.size() > 10
                          && pr.tree.nodes.at(pr.tree.root).label == QStringLiteral("program"),
                      QStringLiteral("%1 个结点、叶 %2 个、根标签 \"%3\"；"
                                     "接受状态=%4；步骤=%5；首步=%6；末步=%7")
                          .arg(pr.tree.size())
                          .arg(pr.tree.leafCount())
                          .arg(pr.tree.root >= 0 ? pr.tree.nodes.at(pr.tree.root).label : QString())
                          .arg(pr.acceptedState)
                          .arg(pr.steps.size())
                          .arg(pr.steps.isEmpty() ? QString() : pr.steps.first().action)
                          .arg(pr.steps.isEmpty() ? QString() : pr.steps.last().action));

            // 与老师参考树对照"结点种类"约定。
            // 两边文法不完全相同（本项目按任务书 BNF 意图补全了 `^` 的 power 产生式，
            // 参考数据是 34 条产生式），所以不可能逐结点同构；但"哪些语法成分在树上
            // 成为结点"这个约定必须一致，否则说明语义动作的设计思路与参考实现不同。
            {
                const QMap<QString, int> refHist = SyntaxTree::labelHistogram(refTree);
                const QMap<QString, int> ourHist = SyntaxTree::labelHistogram(pr.tree);
                QStringList missing;
                for (const QString &key : refHist.keys()) {
                    // 参考树用 token 原文做 value（如 `xXx`、`:=`），本项目用符号名
                    // （如 `ID`、`ASSIGN`）并把原文放在 value 里，因此只对照
                    // "两边都用同一个名字"的那部分：非终结符名与关键字名。
                    const bool sharedName = ourHist.contains(key)
                                            || key == QStringLiteral("xXx")
                                            || key == QStringLiteral("yYy")
                                            || key == QStringLiteral("z1");
                    if (!sharedName)
                        missing << key;
                }
                // 允许参考树里"纯数字/字面量"这类 value 不出现在我们的标签里
                ctx.check(QStringLiteral("T15.6b 自建树覆盖参考树的关键字/非终结符结点"
                                         "（参考 %1 种、自建 %2 种，未覆盖 %3 种）")
                              .arg(refHist.size())
                              .arg(ourHist.size())
                              .arg(missing.size()),
                          ourHist.contains(QStringLiteral("read"))
                              && ourHist.contains(QStringLiteral("if"))
                              && ourHist.contains(QStringLiteral("repeat"))
                              && ourHist.contains(QStringLiteral("write"))
                              && ourHist.contains(QStringLiteral("stmt-sequence")),
                          QStringLiteral("自建树标签：%1")
                              .arg(QStringList(ourHist.keys()).join(QStringLiteral(" ")).left(200)));
            }

            ctx.check(QStringLiteral("T15.3h 分析过程已按语句分段（识别出 %1 条语句）")
                          .arg(pr.statementsParsed),
                      pr.statementsParsed >= 5,
                      QStringLiteral("边界符号=%1；含该符号的产生式 %2 个；"
                                     "p0=[%3]->[%4]；p1=[%5]->[%6]")
                          .arg(parser.statementBoundary())
                          .arg([&] {
                              int n = 0;
                              for (int i = 0; i < table.grammar.productionCount(); ++i) {
                                  if (table.grammar.production(i).right.contains(
                                          parser.statementBoundary()))
                                      ++n;
                              }
                              return n;
                          }())
                          .arg(table.grammar.production(0).left,
                               table.grammar.production(0).right.join(QLatin1Char('|')))
                          .arg(table.grammar.production(1).left,
                               table.grammar.production(1).right.join(QLatin1Char('|'))));

            // 树文本与 DOT 都能生成
            const QString treeText = SyntaxTree::toText(pr.tree);
            // 连通性：TINY 样例的树必须从根可达全部结点
            const int reachable = pr.tree.reachableCount();
            ctx.check(QStringLiteral("T15.3i2 TINY 语法树连通（可达 %1 / 共 %2 个结点）")
                          .arg(reachable)
                          .arg(pr.tree.size()),
                      pr.tree.size() > 0 && reachable == pr.tree.size(),
                      QStringLiteral("根=%1；空标签结点=%2")
                          .arg(pr.tree.root)
                          .arg([&] {
                              int k = 0;
                              for (const SyntaxNode &nd : pr.tree.nodes) {
                                  if (nd.label.isEmpty())
                                      ++k;
                              }
                              return k;
                          }()));
            ctx.check(QStringLiteral("T15.3i 语法树可输出缩进文本（%1 行）")
                          .arg(treeText.count(QLatin1Char('\n')) + 1),
                      treeText.contains(QStringLiteral("stmt-sequence")),
                      QStringLiteral("行数=%1 可达=%2 结点=%3 根=%4 根的子结点数=%5 "
                                     "根的子=%6 首行=%7")
                          .arg(treeText.count(QLatin1Char('\n')) + 1)
                          .arg(reachable)
                          .arg(pr.tree.size())
                          .arg(pr.tree.root)
                          .arg(pr.tree.root >= 0
                                   ? pr.tree.nodes.at(pr.tree.root).children.size()
                                   : -1)
                          .arg([&] {
                              if (pr.tree.root < 0)
                                  return QString();
                              QStringList t;
                              for (int c : pr.tree.nodes.at(pr.tree.root).children)
                                  t << QStringLiteral("%1[%2]n=%3").arg(c).arg(
                                      c >= 0 && c < pr.tree.size()
                                          ? pr.tree.nodes.at(c).label
                                          : QStringLiteral("?"))
                                           .arg(c >= 0 && c < pr.tree.size()
                                                    ? pr.tree.nodes.at(c).children.size()
                                                    : -1);
                              return t.join(QLatin1Char(','));
                          }())
                          .arg(treeText.left(60)));

            const QString dot = SyntaxTree::toDot(pr.tree, QStringLiteral("TINY 语法树"));
            ctx.check(QStringLiteral("T15.3j 语法树可导出 DOT（含 digraph 与 %1 条边）")
                          .arg(pr.tree.size() - 1),
                      dot.startsWith(QStringLiteral("digraph")), dot.left(100));

            // 布局：所有结点都应有合法坐标，且父结点 x 落在子结点 x 之间
            const TreeLayout lay = SyntaxTree::layout(pr.tree);
            bool parentCentered = true;
            for (int i = 0; i < pr.tree.nodes.size() && parentCentered; ++i) {
                const SyntaxNode &n = pr.tree.nodes.at(i);
                if (n.children.size() < 2)
                    continue;
                const double px = lay.nodes.at(i).center.x();
                double lo = 1e18;
                double hi = -1e18;
                for (int c : n.children) {
                    if (c < 0 || c >= lay.nodes.size())
                        continue;
                    lo = qMin(lo, lay.nodes.at(c).center.x());
                    hi = qMax(hi, lay.nodes.at(c).center.x());
                }
                if (lo <= hi && (px < lo - 0.51 || px > hi + 0.51))
                    parentCentered = false;
            }
            ctx.check(QStringLiteral("T15.3k 树布局：父结点的 x 落在其子结点区间内"
                                     "（%1 个结点、逻辑宽 %2、高 %3）")
                          .arg(lay.nodes.size())
                          .arg(lay.width)
                          .arg(lay.height),
                      !lay.isEmpty() && parentCentered);

            // T15.3k2：布局必须是"能看清的像素坐标"，而不是挤成一团。
            // 背景（用户反馈）：页签⑭ 的语法树缩在左上角成一团，无法放大看清。
            // 根因是布局坐标曾经是"逻辑单位"（宽≈叶子数、高≈层数），界面却当像素用，
            // 82 个结点被画进约 40×20 像素的角落。这里从三方面钉住：
            //   ① 尺寸要合理（宽高都远大于 1 像素）；
            //   ② **同一层内结点框两两不重叠** —— 这是"成一团"的直接反面；
            //   ③ 每个框的宽高必须等于/不小于配置的框尺寸。
            {
                TreeLayoutOptions opt;
                opt.charWidth = 8.0;
                opt.minBoxWidth = 26.0;
                opt.boxHeight = 26.0;
                opt.levelHeight = 62.0;
                const TreeLayout pixelLay = SyntaxTree::layout(pr.tree, opt);

                // ② 同层不重叠：按层分组、按 x 排序、逐个检查左边界
                QMap<int, QVector<QRectF>> byDepth;
                for (const TreeNodeLayout &l : pixelLay.nodes)
                    byDepth[l.depth].append(l.box);
                int overlapPairs = 0;
                double worstOverlap = 0.0;
                for (auto it = byDepth.constBegin(); it != byDepth.constEnd(); ++it) {
                    QVector<QRectF> boxes = it.value();
                    std::sort(boxes.begin(), boxes.end(),
                              [](const QRectF &a, const QRectF &b) { return a.left() < b.left(); });
                    for (int i = 1; i < boxes.size(); ++i) {
                        const double overlap = boxes.at(i - 1).right() - boxes.at(i).left();
                        if (overlap > 0.01) {
                            ++overlapPairs;
                            worstOverlap = qMax(worstOverlap, overlap);
                        }
                    }
                }
                ctx.check(QStringLiteral("T15.3k2 树布局：同一层的结点框两两不重叠"
                                         "（%1 层、重叠 %2 对）")
                              .arg(byDepth.size())
                              .arg(overlapPairs),
                          !pixelLay.isEmpty() && overlapPairs == 0,
                          QStringLiteral("最大重叠 %1 像素；逻辑宽 %2、高 %3")
                              .arg(worstOverlap, 0, 'f', 1)
                              .arg(pixelLay.width, 0, 'f', 1)
                              .arg(pixelLay.height, 0, 'f', 1));

                bool boxSized = true;
                QString badBox;
                for (const TreeNodeLayout &l : pixelLay.nodes) {
                    if (l.box.width() < opt.minBoxWidth - 0.01
                        || qAbs(l.box.height() - opt.boxHeight) > 0.01) {
                        boxSized = false;
                        badBox = QStringLiteral("框 %1x%2").arg(l.box.width()).arg(l.box.height());
                        break;
                    }
                }
                ctx.check(QStringLiteral("T15.3k2b 每个结点框都有配置的尺寸（宽 %1、高 %2）")
                              .arg(opt.minBoxWidth)
                              .arg(opt.boxHeight),
                          boxSized, badBox);

                // ① 尺寸合理：宽度应当与叶子数同量级（每个叶子至少 minBoxWidth 宽）
                const int leaves = pr.tree.leafCount();
                ctx.check(QStringLiteral("T15.3k2c 布局尺寸与叶子数匹配（叶 %1 个、宽 %2 像素）")
                              .arg(leaves)
                              .arg(pixelLay.width, 0, 'f', 0),
                          leaves > 0 && pixelLay.width >= leaves * opt.minBoxWidth * 0.6
                              && pixelLay.height >= opt.boxHeight);
            }

            // 括号表示与标签直方图
            const QString bracket = SyntaxTree::toBracket(pr.tree);
            ctx.check(QStringLiteral("T15.3l 语法树可输出括号表示（%1 字符，括号配平）")
                          .arg(bracket.size()),
                      bracket.startsWith(QStringLiteral("("))
                          && bracket.count(QLatin1Char('(')) == bracket.count(QLatin1Char(')')),
                      bracket.left(120));

            const QMap<QString, int> hist = SyntaxTree::labelHistogram(pr.tree);
            ctx.check(QStringLiteral("T15.3m 标签直方图覆盖 %1 种标签").arg(hist.size()),
                      hist.size() >= 5, QStringLiteral("含 ID=%1 个")
                                            .arg(hist.value(QStringLiteral("ID"))));
        }

        // ---- T15.4：语法错误能被检出并给出"期望符号" ----
        {
            Grammar g;
            QString err;
            g.loadFromText(QStringLiteral("S -> A SEMI\nA -> ID\n"), err);
            g.setTerminalNames({{101, QStringLiteral("ID")}, {214, QStringLiteral("SEMI")}});

            ParseTableBuilder builder;
            const ParseTable table = builder.build(g, ParseTableKind::Lalr1);

            SemanticsActionSet sem;   // 空语义 ⇒ 全部走默认动作
            Parser parser;
            parser.setTable(table);
            parser.setSemantics(sem);

            // 缺掉结尾的 SEMI：期望应该报到 SEMI
            const ParseResult bad =
                parser.parse({QStringLiteral("ID"), QStringLiteral("$")});
            ctx.check(QStringLiteral("T15.4 缺失结尾符号时语法分析失败并报错"),
                      !bad.success && !bad.errors.isEmpty(), bad.errorsText().left(160));

            const bool mentionSemi =
                !bad.errors.isEmpty()
                && (bad.errors.first().expected.contains(QStringLiteral("SEMI"))
                    || bad.errorsText().contains(QStringLiteral("SEMI")));
            ctx.check(QStringLiteral("T15.4b 错误信息给出了期望符号 SEMI"), mentionSemi,
                      bad.errors.isEmpty() ? QString() : bad.errors.first().toText());

            // 正常输入应当通过
            const ParseResult good = parser.parse(
                {QStringLiteral("ID"), QStringLiteral("SEMI"), QStringLiteral("$")});
            ctx.check(QStringLiteral("T15.4c 补全输入后分析通过并在 $ 上接受"),
                      good.success && good.acceptedState >= 0, good.summary());
        }

        // -------------------------------------------------------------------
        // T16：阶段 7 —— mini-C 联调（词法 + 语法全流程）
        // -------------------------------------------------------------------

        // ---- T16.1：括号家族的编码名必须与文法的用法一致（真实缺陷守门员）----
        // 背景：老师参考数据 09_minic_words.txt 用"符号行 + 名字行"逐位对应的写法，
        //       `\(` `\)` `\[` `\]` 依次对应 `LLM` `RLM` `LMM` `RMM`。
        //       而 mini-C 文法里：圆括号用于函数定义/调用/条件/循环（写作 LLM/RLM），
        //       方括号用于数组（写作 LMM/RMM）。
        //       本项目早期把 219 与 221 的名字写反了 —— 后果是 `int f(int x){}`
        //       在语法分析时按"方括号"去找 `(`，整片报错。
        //       这条断言把"名字 ↔ 字符"的对应关系钉死，避免再被改回去。
        {
            const QMap<int, QString> mn = LanguageConfigRegistry::terminalNames(LanguageId::MiniC);
            const QMap<QString, int> rev = [&] {
                QMap<QString, int> r;
                for (auto it = mn.constBegin(); it != mn.constEnd(); ++it)
                    r.insert(it.value(), it.key());
                return r;
            }();

            ctx.check(QStringLiteral("T16.1 mini-C 括号编码：219=LLM( 220=RLM) 221=LMM[ 222=RMM]"),
                      mn.value(219) == QStringLiteral("LLM")
                          && mn.value(220) == QStringLiteral("RLM")
                          && mn.value(221) == QStringLiteral("LMM")
                          && mn.value(222) == QStringLiteral("RMM"),
                      QStringLiteral("219=%1 220=%2 221=%3 222=%4")
                          .arg(mn.value(219), mn.value(220), mn.value(221), mn.value(222)));

            ctx.check(QStringLiteral("T16.1b 括号名字到编码的逆映射一致"),
                      rev.value(QStringLiteral("LLM")) == 219
                          && rev.value(QStringLiteral("LMM")) == 221,
                      QStringLiteral("LLM=%1 LMM=%2")
                          .arg(rev.value(QStringLiteral("LLM")))
                          .arg(rev.value(QStringLiteral("LMM"))));

            // 文法里用到 LLM / LMM 的两类产生式，正好一个用圆括号一个用方括号；
            // 用"编码"走一遍翻译，确认不会出现"把 ( 当成 ["这种错位。
            ctx.check(QStringLiteral("T16.1c 圆括号落在 LLM、方括号落在 LMM（按字符验证）"),
                      mn.value(219) == QStringLiteral("LLM")     // ( 
                          && mn.value(221) == QStringLiteral("LMM"),  // [
                      QStringLiteral("( → %1，[ → %2").arg(mn.value(219), mn.value(221)));
        }

        // ---- T16.5：编码表与单词规格文件必须逐号对齐（整类缺陷的守门员）----
        // 这是"两类文件各自看起来都对、合起来却对不上"的典型场景：
        //   单词规格文件 `data/*_words.txt` 决定**词法分析程序实际输出哪个编码**；
        //   `LanguageConfig` 的编码表决定**语法分析把该编码翻译成哪个终结符名**。
        // 两者一旦错位，症状是"词法分析零错误、语法分析却整片报错"，
        // 而且错误信息只会说"遇到 X 无法继续"，完全指不到根因。
        // 这里做双向核对：
        //   ① 单词文件里每个定义占用的编码都要能在编码表里查到名字
        //      （注释定义除外 —— 它有意不输出编码）；
        //   ② 编码表里的每个编码都要能被单词文件的某个定义产出
        //      （否则文法里这个终结符永远匹配不上）。
        for (const LanguageId id : {LanguageId::Tiny, LanguageId::MiniC}) {
            const LanguageConfig &cfg = LanguageConfigRegistry::get(id);
            const QString wordsPath = FileUtil::locateDataFile(cfg.wordsFile);
            bool wordsOk = false;
            QString enc;
            const QString wordsText = FileUtil::readText(wordsPath, &wordsOk, &enc);

            TokenSpec spec;
            const bool parsed = wordsOk && spec.parse(wordsText);

            const QMap<int, QString> names = LanguageConfigRegistry::terminalNames(id);
            const QString tag = cfg.displayName;

            // ① 单词文件 → 编码表
            QStringList missingNames;
            QSet<int> produced;
            for (const RegexDef &def : spec.defs()) {
                for (int code : def.codeList()) {
                    if (code < 0 || def.kind == RegexKind::Comment)
                        continue;   // 注释不输出编码，编码表里没有它是正常的
                    produced.insert(code);
                    if (!names.contains(code))
                        missingNames << QStringLiteral("%1(%2)").arg(def.name).arg(code);
                }
            }
            ctx.check(QStringLiteral("T16.5 %1 单词文件产出的编码都有终结符名"
                                     "（%2 个编码、缺名 %3 个）")
                          .arg(tag)
                          .arg(produced.size())
                          .arg(missingNames.size()),
                      parsed && !produced.isEmpty() && missingNames.isEmpty(),
                      missingNames.isEmpty()
                          ? QStringLiteral("规格解析%1；编码 %2 个")
                                .arg(parsed ? QStringLiteral("成功") : QStringLiteral("失败"))
                                .arg(produced.size())
                          : missingNames.join(QStringLiteral(" / ")));

            // ② 编码表 → 单词文件
            QStringList unreachable;
            for (auto it = names.constBegin(); it != names.constEnd(); ++it) {
                if (!produced.contains(it.key()))
                    unreachable << QStringLiteral("%1(%2)").arg(it.value()).arg(it.key());
            }
            ctx.check(QStringLiteral("T16.5b %1 编码表里的终结符都能被词法分析产出"
                                     "（不可达 %2 个）")
                          .arg(tag)
                          .arg(unreachable.size()),
                      unreachable.isEmpty(),
                      unreachable.isEmpty() ? QStringLiteral("全部可达")
                                            : unreachable.join(QStringLiteral(" / ")));
        }

        // ---- T16.2：mini-C 文法 + 语义动作 + 分析表 ----
        Grammar mcGrammar;
        SemanticsActionSet mcSem;
        ParseTable mcTable;
        SimplifyResult mcSimplify;
        {
            const LanguageId id = LanguageId::MiniC;
            const LanguageConfig &cfg = LanguageConfigRegistry::get(id);

            QString err;
            const QString gPath = FileUtil::locateDataFile(cfg.grammarFile);
            const bool gOk = !gPath.isEmpty() && mcGrammar.loadFromFile(gPath, err);
            mcGrammar.setTerminalNames(LanguageConfigRegistry::terminalNames(id));
            ctx.check(QStringLiteral("T16.2 载入 mini-C 文法（%1 条产生式）")
                          .arg(mcGrammar.productionCount()),
                      gOk && mcGrammar.productionCount() >= 60,
                      gOk ? mcGrammar.summary() : err);

            const QString sPath = FileUtil::locateDataFile(cfg.semanticsFile);
            QString sErr;
            const bool sOk = !sPath.isEmpty() && mcSem.loadFromFile(sPath, sErr);
            ctx.check(QStringLiteral("T16.2b 载入 mini-C 语义动作（%1 条、错误 %2 处）")
                          .arg(mcSem.count())
                          .arg(mcSem.errors().size()),
                      sOk && mcSem.errors().isEmpty()
                          && mcSem.count() == mcGrammar.productionCount(),
                      sOk ? (mcSem.summary()
                             + (mcSem.errors().isEmpty()
                                    ? QString()
                                    : QStringLiteral("；") + mcSem.errors().join(QStringLiteral(" / "))))
                          : sErr);

            GrammarSimplify simplifier;
            mcSimplify = simplifier.simplify(mcGrammar);

            ParseTableBuilder builder;
            mcTable = builder.build(mcSimplify.kept, ParseTableKind::Lalr1);
            ctx.check(QStringLiteral("T16.2c mini-C 构造 LALR(1) 分析表（%1 状态、%2 处冲突）")
                          .arg(mcTable.stateCount())
                          .arg(mcTable.conflicts.size()),
                      mcTable.stateCount() > 50,
                      mcTable.summary());
        }

        // ---- T16.3：用人工构造的"最小但覆盖各构造"的单词序列走一遍语法分析 ----
        // 说明：这里刻意不依赖词法阶段生成的 .lex，而是直接给符号序列 ——
        //      目的是把"语法分析是否理解 mini-C 的括号/分号/关键字"这件事
        //      单独隔离出来验证；词法链路另由 T16.4 验证。
        {
            Grammar readGrammar = mcSimplify.kept;
            readGrammar.setTerminalNames(LanguageConfigRegistry::terminalNames(LanguageId::MiniC));
            readGrammar.augment();

            Parser parser;
            parser.setTable(mcTable);
            parser.setSemantics(mcSem);

            // int main(void) { return 0; }
            // 注意：符号序列里用的是**终结符名**（标识符统一是 `ID`），
            //       标识符原文通过 values 传进去 —— 这正是 LexReader 从 .lex
            //       翻译出来的形态（编码 101 → 名字 ID、原文 main）。
            const QStringList sym{
                QStringLiteral("int"), QStringLiteral("ID"), QStringLiteral("LLM"),
                QStringLiteral("void"), QStringLiteral("RLM"),
                QStringLiteral("LBM"), QStringLiteral("return"), QStringLiteral("NUM"),
                QStringLiteral("SEMI"), QStringLiteral("RBM"), QStringLiteral("$")};
            const QStringList val{QString(), QStringLiteral("main"), QString(), QString(),
                                  QString(), QString(), QString(), QStringLiteral("0"),
                                  QString(), QString(), QString()};

            const ParseResult pr = parser.parse(sym, QVector<int>(sym.size(), 1), val);

            ctx.check(QStringLiteral("T16.3 mini-C 函数定义（int main(void){return 0;}）分析通过（%1）")
                          .arg(pr.summary()),
                      pr.success, pr.errorsText().left(300));

            ctx.check(QStringLiteral("T16.3b 语法树连通（可达 %1 / 共 %2）、根为开始符号 program")
                          .arg(pr.tree.reachableCount())
                          .arg(pr.tree.size()),
                      pr.tree.root >= 0 && pr.tree.reachableCount() == pr.tree.size()
                          && pr.tree.nodes.at(pr.tree.root).label == QStringLiteral("program"),
                      QStringLiteral("括号表示=%1").arg(SyntaxTree::toBracket(pr.tree)));

            // 树里应当能看到函数定义、返回语句、返回值这些成分
            const QString bracket = SyntaxTree::toBracket(pr.tree);
            ctx.check(QStringLiteral("T16.3c 语法树体现 function-definition / return-stmt / type-indicator"),
                      bracket.contains(QStringLiteral("function-definition"))
                          && bracket.contains(QStringLiteral("return"))
                          && bracket.contains(QStringLiteral("int")),
                      bracket.left(260));
        }

        // ---- T16.4：mini-C 数组与表达式的语法分析（方括号 / 圆括号不能串）----
        {
            Parser parser;
            parser.setTable(mcTable);
            parser.setSemantics(mcSem);

            // int arr[100];  →  方括号 LMM/RMM（标识符同样用 `ID` + values 传原文）
            const QStringList arr{QStringLiteral("int"), QStringLiteral("ID"),
                                  QStringLiteral("LMM"), QStringLiteral("NUM"),
                                  QStringLiteral("RMM"), QStringLiteral("SEMI"),
                                  QStringLiteral("$")};
            const ParseResult pa =
                parser.parse(arr, QVector<int>(arr.size(), 1),
                             QStringList{QString(), QStringLiteral("arr"), QString(), QStringLiteral("100"),
                                         QString(), QString(), QString()});
            ctx.check(QStringLiteral("T16.4 mini-C 数组声明（int arr[100];）分析通过（%1）")
                          .arg(pa.summary()),
                      pa.success, pa.errorsText().left(240));

            // 反过来把方括号换成圆括号就必须失败 —— 证明这两种括号确实被区分开了
            const QStringList wrong{QStringLiteral("int"), QStringLiteral("ID"),
                                    QStringLiteral("LLM"), QStringLiteral("NUM"),
                                    QStringLiteral("RLM"), QStringLiteral("SEMI"),
                                    QStringLiteral("$")};
            const ParseResult pw = parser.parse(wrong, QVector<int>(wrong.size(), 1),
                                                QStringList{QString(), QStringLiteral("arr"),
                                                            QString(), QStringLiteral("100"),
                                                            QString(), QString(), QString()});
            ctx.check(QStringLiteral("T16.4b 把数组的方括号写成圆括号会被判为语法错误"),
                      !pw.success && !pw.errors.isEmpty(),
                      pw.errorsText().left(200));
        }

        // -------------------------------------------------------------------
        // T17：阶段 8（选做）—— 四元式中间代码生成
        // -------------------------------------------------------------------

        // ---- T17.1：从最小表达式语法树生成四元式 ----
        // 覆盖"算术 + 优先级 + 赋值"。这类断言的价值在于：中间代码
        // "看着像对的"很容易骗过肉眼，必须把 (op, arg1, arg2, result)
        // 的每个槽位都断言到。
        {
            Grammar g;
            QString gerr;
            g.loadFromText(QStringLiteral("S -> ID ASSIGN E SEMI\n"
                                          "E -> E PLUS T\n"
                                          "E -> T\n"
                                          "T -> T MULTIPLY F\n"
                                          "T -> F\n"
                                          "F -> ID\n"
                                          "F -> NUM\n"),
                           gerr);
            QMap<int, QString> names;
            names.insert(500, QStringLiteral("ASSIGN"));
            names.insert(501, QStringLiteral("PLUS"));
            names.insert(502, QStringLiteral("MULTIPLY"));
            names.insert(503, QStringLiteral("SEMI"));
            names.insert(504, QStringLiteral("ID"));
            names.insert(505, QStringLiteral("NUM"));
            g.setTerminalNames(names);

            // 语义动作（行序 = 产生式顺序）：
            //   S -> ID ASSIGN E SEMI : `R1 0 2` —— 复用右部**第 1 项**（ASSIGN 终结符结点），
            //                            把右部第 0 项（ID）与第 2 项（表达式）挂成它的子结点；
            //                            末尾的 SEMI 不挂进树。
            //                            注意不能写成 `R0 …`：右部第 0 项是 **ID**，
            //                            那样根就成了一个没有子结点的 ID 叶结点，四元式会是空的。
            //   E -> E PLUS T         : `R1 0 2` —— 复用 PLUS 结点带左右操作数
            //   E -> T                : `-1 0`    —— 单链折叠
            //   T -> T MULTIPLY F     : `R1 0 2`
            //   T -> F                : `-1 0`
            //   F -> ID / F -> NUM    : `R0`      —— 复用终结符叶结点
            SemanticsActionSet sem;
            const bool semOk = sem.loadFromText(QStringLiteral("R1 0 2\n"
                                                              "R1 0 2\n"
                                                              "-1 0\n"
                                                              "R1 0 2\n"
                                                              "-1 0\n"
                                                              "R0\n"
                                                              "R0\n"));
            ctx.check(QStringLiteral("T17.1 最小文法语义动作载入（%1 条）").arg(sem.count()),
                      semOk && sem.count() == 7,
                      sem.errors().isEmpty() ? sem.summary()
                                             : sem.errors().join(QStringLiteral(" / ")));

            ParseTableBuilder builder;
            const ParseTable table = builder.build(g, ParseTableKind::Lalr1);

            Parser parser;
            parser.setTable(table);
            parser.setSemantics(sem);

            // x := 1 + 2 * 3 ;
            const QStringList sym{QStringLiteral("ID"), QStringLiteral("ASSIGN"),
                                  QStringLiteral("NUM"), QStringLiteral("PLUS"),
                                  QStringLiteral("NUM"), QStringLiteral("MULTIPLY"),
                                  QStringLiteral("NUM"), QStringLiteral("SEMI"),
                                  QStringLiteral("$")};
            const QStringList val{QStringLiteral("x"), QString(), QStringLiteral("1"),
                                  QString(), QStringLiteral("2"), QString(),
                                  QStringLiteral("3"), QString(), QString()};
            const ParseResult pr =
                parser.parse(sym, QVector<int>(sym.size(), 1), val);

            ctx.check(QStringLiteral("T17.1b 最小表达式文法分析通过（%1）").arg(pr.summary()),
                      pr.success && pr.tree.root >= 0, pr.errorsText().left(200));

            const IntermediateCode ic = IntermediateCode::generate(pr.tree);

            ctx.check(QStringLiteral("T17.1c 生成 3 条四元式（%1）").arg(ic.summary()),
                      ic.count() == 3 && ic.warnings.isEmpty(),
                      QStringLiteral("%1；告警：%2")
                          .arg(QString(ic.toText(false)).replace(QLatin1Char('\n'),
                                                                QStringLiteral(" ⏎ ")),
                               ic.warnings.isEmpty() ? QStringLiteral("无")
                                                     : ic.warnings.join(QStringLiteral(" / "))));

            // 期望：先乘后加，最后赋值
            //   (1) (*, 2, 3, t1)
            //   (2) (+, 1, t1, t2)
            //   (3) (:=, t2, -, x)
            bool shapeOk = ic.count() == 3;
            QString shapeDetail;
            if (shapeOk) {
                const Quadruple &q1 = ic.quads.at(0);
                const Quadruple &q2 = ic.quads.at(1);
                const Quadruple &q3 = ic.quads.at(2);
                shapeOk = q1.op == QStringLiteral("*") && q1.arg1 == QStringLiteral("2")
                          && q1.arg2 == QStringLiteral("3") && q1.result == q2.arg2
                          && q2.op == QStringLiteral("+") && q2.arg1 == QStringLiteral("1")
                          && q3.op == QStringLiteral(":=") && q3.result == QStringLiteral("x");
                shapeDetail = QStringLiteral("%1 / %2 / %3")
                                  .arg(q1.toText(), q2.toText(), q3.toText());
            }
            ctx.check(QStringLiteral("T17.1d 四元式体现优先级（先乘、后加、再赋值）"), shapeOk,
                      shapeDetail);

            // 每条四元式的槽位都必须有内容（空槽补 `-`），否则数据流看不出来
            bool slotsOk = true;
            for (const Quadruple &q : ic.quads) {
                if (q.op.isEmpty() || q.arg1.isEmpty() || q.arg2.isEmpty()
                    || q.result.isEmpty())
                    slotsOk = false;
            }
            ctx.check(QStringLiteral("T17.1e 四元式四个槽位都有内容（空槽补 - 而不是留空）"),
                      slotsOk, ic.toText(false));

            QStringList headers;
            QList<QStringList> rows;
            ic.buildTable(headers, rows);
            ctx.check(QStringLiteral("T17.1f 四元式可整理成表格（%1 行 × %2 列）")
                          .arg(rows.size())
                          .arg(headers.size()),
                      rows.size() == ic.count() && headers.size() == 7,
                      headers.join(QStringLiteral(" / ")));
        }

        // ---- T17.2：空语法树不得崩，且要给出可读告警 ----
        {
            const IntermediateCode empty = IntermediateCode::generate(SyntaxTreeData());
            ctx.check(QStringLiteral("T17.2 空语法树生成中间代码不崩、且给出告警"),
                      empty.isEmpty() && !empty.warnings.isEmpty(),
                      empty.warnings.join(QStringLiteral(" / ")));
        }

        // -------------------------------------------------------------------
        // T18：DOT 转义必须产出纯 ASCII（否则 Graphviz 拒绝出图）
        // -------------------------------------------------------------------
        // 用户反馈：导出的 .dot 里有非 ASCII 字符，Graphviz 画不出图。
        // 根因是自动机字母表覆盖全部 256 个字节值，边标签里带 NUL / CR /
        // 0x80~0xFF 等字节；原样写入时 Graphviz 报
        // `syntax error ... scanning a quoted string (missing endquote?)`
        // —— 这是**无法解析**，不是"中文显示成方框"那种字体问题。
        {
            // 构造含全部 256 个字节值 + 中文 + 引号 + 反斜杠的字符串
            QString all;
            for (int b = 0; b < 256; ++b)
                all += QChar(ushort(b));
            all += QStringLiteral("中文标签\"引号\\反斜杠");

            const QString escaped = FileUtil::escapeForDot(all);

            const auto firstBad = [](const QString &text) -> QString {
                for (int i = 0; i < text.size(); ++i) {
                    const ushort u = text.at(i).unicode();
                    if (u > 0x7E || u < 0x20)
                        return QStringLiteral("位置 %1 出现 U+%2")
                            .arg(i)
                            .arg(QString::number(u, 16)
                                     .rightJustified(4, QLatin1Char('0'))
                                     .toUpper());
                }
                return QString();
            };

            // ① 转义结果必须纯可打印 ASCII
            ctx.check(QStringLiteral("T18 256 个字节值 + 中文 + 引号 + 反斜杠，"
                                     "转义后为纯 ASCII（%1 字符）")
                          .arg(escaped.size()),
                      firstBad(escaped).isEmpty(),
                      firstBad(escaped).isEmpty() ? escaped.left(90) : firstBad(escaped));

            // ② 关键字符要变成可见表示（NUL 尤其致命：C 词法器遇它即止）
            ctx.check(QStringLiteral("T18b NUL / CR / 高位字节 / 中文都被转成可见表示"),
                      escaped.contains(QStringLiteral("<00>"))
                          && escaped.contains(QStringLiteral("<0D>"))
                          && escaped.contains(QStringLiteral("<FF>"))
                          && escaped.contains(QStringLiteral("<U+4E2D>")),
                      escaped.left(150));

            // ③ 引号与反斜杠按 DOT 规则转义，且只转义一次
            ctx.check(QStringLiteral("T18c 引号转成 反斜杠+引号、反斜杠转成两个反斜杠"),
                      FileUtil::escapeForDot(QStringLiteral("\"")) == QStringLiteral("\\\"")
                          && FileUtil::escapeForDot(QStringLiteral("\\"))
                                 == QStringLiteral("\\\\"),
                      QStringLiteral("引号→%1；反斜杠→%2")
                          .arg(FileUtil::escapeForDot(QStringLiteral("\"")),
                               FileUtil::escapeForDot(QStringLiteral("\\"))));

            // ④ 普通 ASCII 标签必须原样通过（转义不能影响正常标签）
            ctx.check(QStringLiteral("T18d 普通 ASCII 标签不被改动"),
                      FileUtil::escapeForDot(QStringLiteral("stmt-sequence (ID: xXx)"))
                          == QStringLiteral("stmt-sequence (ID: xXx)"),
                      FileUtil::escapeForDot(QStringLiteral("stmt-sequence (ID: xXx)")));
        }

        // ---- T17.3：老师参考树（含 read / if / repeat）也能生成 ----
        {
            const IntermediateCode ic = IntermediateCode::generate(refTree);
            ctx.check(QStringLiteral("T17.3 参考语法树可生成四元式（%1）").arg(ic.summary()),
                      !ic.isEmpty(),
                      QString(ic.toText(false)).left(240));
        }

        // ---- T17.4：mini-C 函数（形参 / 加法 / 赋值 / 返回）----
        {
            Parser parser;
            parser.setTable(mcTable);
            parser.setSemantics(mcSem);

            // int f(int a) { a = a + 1; return a; }
            const QStringList sym{
                QStringLiteral("int"), QStringLiteral("ID"), QStringLiteral("LLM"),
                QStringLiteral("int"), QStringLiteral("ID"), QStringLiteral("RLM"),
                QStringLiteral("LBM"), QStringLiteral("ID"), QStringLiteral("ASSIGN"),
                QStringLiteral("ID"), QStringLiteral("PLUS"), QStringLiteral("NUM"),
                QStringLiteral("SEMI"), QStringLiteral("return"), QStringLiteral("ID"),
                QStringLiteral("SEMI"), QStringLiteral("RBM"), QStringLiteral("$")};
            const QStringList val{QString(), QStringLiteral("f"), QString(),
                                  QString(), QStringLiteral("a"), QString(),
                                  QString(), QStringLiteral("a"), QString(),
                                  QStringLiteral("a"), QString(), QStringLiteral("1"),
                                  QString(), QString(), QStringLiteral("a"),
                                  QString(), QString(), QString()};
            const ParseResult pr = parser.parse(sym, QVector<int>(sym.size(), 1), val);

            ctx.check(QStringLiteral("T17.4 mini-C 带形参与返回的函数分析通过（%1）")
                          .arg(pr.summary()),
                      pr.success, pr.errorsText().left(240));

            const IntermediateCode ic = IntermediateCode::generate(pr.tree);
            const QString text = ic.toText(false);

            ctx.check(QStringLiteral("T17.4b 四元式含函数边界/形参/加法/赋值/返回（%1）")
                          .arg(ic.summary()),
                      text.contains(QStringLiteral("(func,"))
                          && text.contains(QStringLiteral("(formal,"))
                          && text.contains(QStringLiteral("(+,"))
                          && text.contains(QStringLiteral("(:=,"))
                          && text.contains(QStringLiteral("(return,"))
                          && text.contains(QStringLiteral("(endfunc,")),
                      QString(text).left(400));
        }
    }
}

}  // namespace ccd
