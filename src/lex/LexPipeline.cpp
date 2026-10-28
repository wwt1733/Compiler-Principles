/******************************************************************************
 * (1) 版权信息 : 编译原理课程设计 - 华南师范大学计算机学院
 * (2) 文件名称 : LexPipeline.cpp
 *     标识符   : LexPipeline
 *     摘要     : 第一项任务完整流水线的实现。逐阶段调用各算法模块，
 *                任一阶段失败即记录 failedStage 并返回，已完成的阶段结果保留。
 * (3) 版本号   : 1.0.0       作者/修改者 : <学号 姓名>   完成日期 : 2026-XX-XX
 * (4) 版本历史 : 1.0.0  2026-XX-XX  首次实现
 *****************************************************************************/

#include "lex/LexPipeline.h"

#include "common/Logger.h"
#include "lex/DfaMinimize.h"
#include "lex/NfaToDfa.h"
#include "lex/RegexAst.h"
#include "lex/RegexToNfa.h"

namespace ccd {

namespace {

/** ID 的编码约定：100=Num、101=ID、102=Comment（见 AGENT.md §6.1） */
const int kIdCode = 101;

/**
 * 功能：把单个字节映射成自动机字母表里的符号键。
 * 输入：info —— 字符类字母表；byte —— 字节值。
 * 输出：符号键；按字节模式（info 为空）时返回该字节的单字符文本。
 * 注意：按字符类模式构造的 DFA，转移表的键是 "@0" 这样的类符号键，
 *       直接拿字符去查会永远查不到，必须先做这个映射。
 */
QString symbolKeyOfByte(const CharClassAlphabet &info, int byte)
{
    if (info.isEmpty())
        return QString(QChar(static_cast<ushort>(byte & 0xFF)));
    return info.symbolOfByte(byte);
}

/**
 * 功能：检查规格里声明的每个关键字是否**真的可能被识别出来**。
 * 输入：spec —— 已解析的单词规格；dfa —— 最简 DFA。
 * 输出：告警列表（空表示全部关键字都能产生）。
 * 注意：为什么需要这个检查 —— 关键字与 ID 的正则式完全重合，因此关键字
 *       **不建自动机**，只能先由 ID 规则识别出标识符、再查关键字表改判编码。
 *       所以规格里若没有能吃下"INT"这类字母串的规则（通常就是漏了 ID 规则），
 *       关键字就永远不会出现在 .lex 里，而解析阶段不会报任何错 ——
 *       表现为"词法分析时每个字母都被报成无法识别的字符"，极难定位。
 */
QStringList checkKeywordProducibility(const TokenSpec &spec, const Dfa &dfa)
{
    QStringList warnings;
    const QMap<QString, int> keywords = spec.keywordTable();
    if (keywords.isEmpty() || dfa.stateCount <= 0)
        return warnings;

    for (auto it = keywords.constBegin(); it != keywords.constEnd(); ++it) {
        const QString &word = it.key();
        int state = dfa.start;
        bool stuck = false;
        for (const QChar &ch : word) {
            const QString symbol =
                symbolKeyOfByte(dfa.alphabetInfo, static_cast<int>(ch.unicode()) & 0xFF);
            if (symbol.isEmpty()) {
                stuck = true;
                break;
            }
            const auto next = dfa.trans.at(state).constFind(symbol);
            if (next == dfa.trans.at(state).constEnd()) {
                stuck = true;
                break;
            }
            state = next.value();
        }
        const bool producible = !stuck && dfa.accepting.value(state, false)
                                && dfa.acceptCode.value(state, -1) == kIdCode;
        if (producible)
            continue;

        warnings << QStringLiteral("关键字「%1」（编码 %2）永远不会被识别：规格里没有能把"
                                   "「%1」识别成标识符（编码 %3）的自动机规则，"
                                   "通常是缺少 ID 规则（如 ID_%3=letter(letter|digit)*）")
                        .arg(word)
                        .arg(it.value())
                        .arg(kIdCode);
    }
    return warnings;
}

}  // namespace

/**
 * 功能：生成流水线结果的一句话摘要。
 * 输入：无（使用本对象）。
 * 输出：摘要文本。
 * 注意：按阶段拼接规模数字，便于在日志中一眼看出每一步是否合理。
 */
QString LexPipeline::Result::summary() const
{
    if (!error.isEmpty())
        return QStringLiteral("流水线在【%1】阶段失败：%2").arg(failedStage, error);

    QStringList parts;
    parts << QStringLiteral("定义 %1 条").arg(spec.defs().size());
    parts << QStringLiteral("NFA %1 状态").arg(nfa.stateCount);
    parts << QStringLiteral("DFA %1 状态").arg(dfa.stateCount);
    parts << QStringLiteral("最小化 %1 状态").arg(minDfa.stateCount);
    if (codeOk)
        parts << QStringLiteral("生成源码 %1 行").arg(generatedCode.count(QLatin1Char('\n')) + 1);
    if (build.compiled)
        parts << QStringLiteral("g++ 编译通过");
    if (build.ran)
        parts << QStringLiteral(".lex %1 行").arg(build.lexContent.count(QLatin1Char('\n')));
    return parts.join(QStringLiteral("；"));
}

/**
 * 功能：执行完整流水线。
 * 输入：见头文件说明。
 * 输出：结果结构。
 * 注意：
 *  1. 每个阶段的失败都**不抛异常也不中断进程**，而是记录到 failedStage/error，
 *     这样界面可以显示"做到了哪一步"，用户也能据此定位问题；
 *  2. 关键字不参与自动机构造，而是取 keywordTable() 交给生成的词法程序查表改判；
 *  3. 生成源码时把"最小化 DFA"作为依据（任务书要求 1.7 明确要求）。
 */
LexPipeline::Result LexPipeline::run(const QString &wordsText, const QString &programName,
                                     const QString &sourceFile, const QString &outputDir,
                                     const QString &identifier, bool runBuild,
                                     const QString &compilerPath)
{
    Result result;

    // ---- 阶段 1：解析单词规格 ----
    if (!result.spec.parse(wordsText)) {
        result.failedStage = QStringLiteral("解析单词规格");
        result.error = result.spec.errors().join(QStringLiteral("；"));
        LOG_ERROR(QStringLiteral("流水线中止：%1 —— %2").arg(result.failedStage, result.error));
        return result;
    }
    result.specOk = true;

    // ---- 阶段 2：逐条定义解析正则式，构造 NFA ----
    for (const RegexDef &def : result.spec.automatonDefs()) {
        RegexParser parser;
        parser.setMacros(result.spec.macros());

        NfaRule rule;
        rule.name = def.name;
        rule.code = def.code;
        rule.isComment = (def.kind == RegexKind::Comment);

        if (!parser.parse(def.rawRegex, rule.ast)) {
            result.failedStage = QStringLiteral("解析正则式");
            result.error = QStringLiteral("定义 %1：%2").arg(def.name, parser.error());
            LOG_ERROR(QStringLiteral("流水线中止：%1 —— %2").arg(result.failedStage, result.error));
            return result;
        }

        result.postfixText += QStringLiteral("[%1]  编码 %2  类别 %3\n    中缀式  ： %4\n"
                                             "    逆波兰式： %5\n\n")
                                  .arg(def.name)
                                  .arg(def.code)
                                  .arg(def.kind == RegexKind::Comment
                                           ? QStringLiteral("注释（不输出编码）")
                                           : (def.tokenText.isEmpty()
                                                  ? QStringLiteral("普通单词")
                                                  : QStringLiteral("专用符号 %1").arg(def.tokenText)))
                                  .arg(def.rawRegex, rule.ast.toPostfix());
        result.rules.append(rule);
    }

    if (result.rules.isEmpty()) {
        result.failedStage = QStringLiteral("构造 NFA");
        result.error = QStringLiteral("没有任何需要构造自动机的定义。");
        return result;
    }

    RegexToNfa nfaBuilder;
    if (!nfaBuilder.build(result.rules, result.nfa)) {
        result.failedStage = QStringLiteral("构造 NFA");
        result.error = nfaBuilder.error();
        LOG_ERROR(QStringLiteral("流水线中止：%1 —— %2").arg(result.failedStage, result.error));
        return result;
    }
    result.nfaOk = true;
    result.postfixNotes = QStringLiteral("由 %1 条规则合并构造（规则之间为“或”关系），"
                                         "统一初态为状态 0")
                              .arg(result.rules.size());

    // ---- 阶段 3：子集构造 NFA → DFA ----
    NfaToDfa subset;
    if (!subset.construct(result.nfa, result.dfa)) {
        result.failedStage = QStringLiteral("子集构造 NFA → DFA");
        result.error = subset.error();
        LOG_ERROR(QStringLiteral("流水线中止：%1 —— %2").arg(result.failedStage, result.error));
        return result;
    }
    result.dfaOk = true;

    // ---- 阶段 4：划分法最小化 DFA ----
    if (!result.minimizeDetail.minimize(result.dfa, result.minDfa)) {
        result.failedStage = QStringLiteral("最小化 DFA");
        result.error = result.minimizeDetail.error();
        LOG_ERROR(QStringLiteral("流水线中止：%1 —— %2").arg(result.failedStage, result.error));
        return result;
    }
    result.minOk = true;

    // ---- 阶段 4.5：关键字可产生性检查（非致命告警，不中止流水线）----
    result.warnings = checkKeywordProducibility(result.spec, result.minDfa);
    for (const QString &warning : result.warnings)
        LOG_WARN(QStringLiteral("词法规格告警：%1").arg(warning));

    // ---- 阶段 5：生成词法分析源程序 ----
    LexCodeGen generator;
    result.methodName = (m_method == LexCodeGen::Method::JudgeChar)
                            ? QStringLiteral("方法一：Judgechar/GetNext/GetToken")
                            : QStringLiteral("方法二：switch(state)");
    result.generatedCode =
        generator.generate(result.minDfa, result.spec.literalCodeTable(),
                           result.spec.keywordTable(), kIdCode, m_method, m_layout, programName);
    if (result.generatedCode.isEmpty()) {
        result.failedStage = QStringLiteral("生成词法分析源程序");
        result.error = generator.error();
        LOG_ERROR(QStringLiteral("流水线中止：%1 —— %2").arg(result.failedStage, result.error));
        return result;
    }
    result.codeOk = true;

    // ---- 阶段 6：编译并运行（可选）----
    if (runBuild) {
        LexBuild builder;
        if (!compilerPath.isEmpty())
            builder.setCompiler(compilerPath);
        // 透传事件泵：等待 g++ 与生成的词法程序期间让界面保持响应
        builder.setEventPump(m_eventPump);
        result.build = builder.run(result.generatedCode, identifier, outputDir, sourceFile,
                                   m_layout == LexCodeGen::Layout::TaskbookSample);
        if (!result.build.ran) {
            result.failedStage = QStringLiteral("编译并运行词法分析程序");
            result.error = builder.error();
            LOG_ERROR(QStringLiteral("流水线中止：%1 —— %2").arg(result.failedStage, result.error));
            return result;
        }
    }

    LOG_SUCCESS(QStringLiteral("词法流水线完成：%1").arg(result.summary()));
    return result;
}

}  // namespace ccd
