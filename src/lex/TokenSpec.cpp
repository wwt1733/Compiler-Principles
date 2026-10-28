/******************************************************************************
 * (1) 版权信息 : 编译原理课程设计 - 华南师范大学计算机学院
 * (2) 文件名称 : TokenSpec.cpp
 *     标识符   : TokenSpec
 *     摘要     : 单词规格文件解析器的实现。逐行解析 "名字=正则式"，识别命名
 *                中的编码与 L 后缀，区分宏定义/普通单词/注释/关键字，并把宏
 *                预先解析为语法树。
 * (3) 版本号   : 1.0.0       作者/修改者 : <学号 姓名>   完成日期 : 2026-XX-XX
 * (4) 版本历史 : 1.0.0  2026-XX-XX  首次实现
 *****************************************************************************/

#include "lex/TokenSpec.h"

#include <QRegularExpression>

#include <functional>

#include "common/FileUtil.h"
#include "common/Logger.h"

namespace ccd {

namespace {

/**
 * 功能：判断一个名字是否属于关键字类定义。
 * 输入：name —— 定义名。
 * 输出：是关键字类时为 true。
 * 注意：任务书样例写的是 "Keyword103L"，其中 Keyword 之后直接跟数字（无下划线）；
 *       为容错，这里不区分大小写，并同时接受 Keyword_103L 的写法。
 */
bool isKeywordName(const QString &name)
{
    return name.startsWith(QStringLiteral("keyword"), Qt::CaseInsensitive)
           || name.startsWith(QStringLiteral("reserved"), Qt::CaseInsensitive);
}

/**
 * 功能：判断一个名字是否属于注释类定义。
 * 输入：name —— 定义名。
 * 输出：是注释类时为 true。
 */
bool isCommentName(const QString &name)
{
    return name.startsWith(QStringLiteral("comment"), Qt::CaseInsensitive);
}

/**
 * 功能：按**顶层** '|' 把定义右部拆分为若干选择项。
 * 输入：text —— 定义右部原文。
 * 输出：各选择项（已去首尾空白，保持原有顺序）。
 * 注意：
 *  1. 只按括号/方括号**深度为 0** 处的 '|' 拆分，因此 (a|b) | c 会正确地
 *     拆成 "(a|b)" 与 "c" 两项，而不会把括号内的 | 也当成顶层分隔符；
 *  2. 反斜线转义的 '|'（写作 \|）不视为分隔符；
 *  3. L 后缀的语义正是"右部的每个选择项各占一个连续编码"，因此本函数是
 *     Keyword103L / Symbol200L 展开的基础。
 */
QStringList splitTopLevelAlternatives(const QString &text)
{
    QStringList parts;
    QString current;
    int depth = 0;
    for (int i = 0; i < text.size(); ++i) {
        const QChar ch = text.at(i);
        if (ch == QLatin1Char('\\') && i + 1 < text.size()) {
            // 转义序列整体保留（含反斜线），避免把 \| 当成分隔符
            current += ch;
            current += text.at(++i);
            continue;
        }
        if (ch == QLatin1Char('(') || ch == QLatin1Char('[')) {
            ++depth;
        } else if (ch == QLatin1Char(')') || ch == QLatin1Char(']')) {
            if (depth > 0)
                --depth;
        } else if (ch == QLatin1Char('|') && depth == 0) {
            const QString trimmed = current.trimmed();
            if (!trimmed.isEmpty())
                parts.append(trimmed);
            current.clear();
            continue;
        }
        current += ch;
    }
    const QString trimmed = current.trimmed();
    if (!trimmed.isEmpty())
        parts.append(trimmed);
    return parts;
}

/**
 * 功能：把语法树还原为"纯字面单词"；若含运算符或字符集则返回空串。
 * 输入：ast —— 已解析的正则语法树。
 * 输出：字面单词（如 "+"、":="、"if"）；不是纯字面量时返回空 QString。
 * 注意：仅用于关键字与专用符号的常量表（生成的词法程序需要用 strcmp 查表），
 *       因此 '+*' 这类真正的正则式返回空串是正确的——它们不需要字面单词。
 */
QString astToLiteral(const RegexAst &ast)
{
    if (ast.isEmpty())
        return QString();

    QString result;
    // 只有"连接"或"单个原子"才可能是字面单词；选择/闭包等一律返回空
    std::function<bool(int)> walk = [&](int index) -> bool {
        if (index < 0 || index >= ast.size())
            return false;
        const RegexNode &node = ast.node(index);
        switch (node.kind) {
        case RegexNodeKind::Concat:
            for (int child : node.children) {
                if (!walk(child))
                    return false;
            }
            return true;
        case RegexNodeKind::Charset: {
            const QStringList chars = ast.resolveCharset(index);
            if (chars.size() != 1)
                return false;
            result += chars.first();
            return true;
        }
        case RegexNodeKind::Empty:
            return true;  // 空串不贡献字符（如关键字两侧的空白已被裁掉）
        default:
            return false;  // Alternate / Star / Plus / Optional / Epsilon 均非字面量
        }
    };

    if (!walk(ast.root()))
        return QString();
    return result;
}

/**
 * 功能：把名字拆分为"基名 + 编码 + Launch 标志"三部分。
 * 输入：rawName —— 原始名字，形如 digit / Num_100 / Symbol200L / Keyword103L。
 * 输出：baseName —— 去掉编码后的基名；
 *       code —— 编码，无编码时为 -1；
 *       launch —— 是否带 L 后缀；
 *       hasCode —— 是否成功识别出编码。
 * 注意：
 *  1. 有下划线的形式（Num_100）与无下划线的形式（Keyword103L、Symbol200L）
 *     都要支持，因为任务书样例同时出现了这两种写法；
 *  2. 只有"末尾的连续数字（可带一个 L）"才被当作编码，避免把 ID_101A 这类
 *     名字误拆。
 */
void splitName(const QString &rawName, QString &baseName, int &code, bool &launch, bool &hasCode)
{
    baseName = rawName;
    code = -1;
    launch = false;
    hasCode = false;

    // 先尝试去掉尾部的一个 L（Launch）
    static const QRegularExpression re(QStringLiteral("^(.*?)_?(\\d+)(L?)$"));
    const QRegularExpressionMatch match = re.match(rawName);
    if (!match.hasMatch())
        return;

    const QString base = match.captured(1);
    if (base.isEmpty())
        return;  // 名字不能只有数字编码

    baseName = base;
    code = match.captured(2).toInt();
    launch = (match.captured(3) == QStringLiteral("L"));
    hasCode = true;
}

}  // namespace

/**
 * 功能：解析规格文件全文。
 * 输入：text —— 文件内容。
 * 输出：无致命错误时为 true。
 * 注意：
 *  1. 以 '#' 或 "//" 开头的行视为注释行直接跳过（注意 data 下的 words 文件用 '#'）；
 *  2. 一行的解析失败只影响该行，其余定义继续解析，便于一次性暴露全部问题；
 *  3. 宏定义（无编码的 digit=、letter= 等）不加入 m_defs 之外还额外进入 m_macros。
 */
bool TokenSpec::parse(const QString &text)
{
    m_defs.clear();
    m_macros.clear();
    m_errors.clear();

    const QStringList lines = FileUtil::normalizeNewlines(text).split(QLatin1Char('\n'));
    int order = 0;
    bool allOk = true;

    for (int i = 0; i < lines.size(); ++i) {
        const QString line = lines.at(i).trimmed();
        if (line.isEmpty())
            continue;
        // 注释行：'#'（本项目的规格文件约定）与 "//"（兼容任务书样例的写法）
        if (line.startsWith(QLatin1Char('#')) || line.startsWith(QStringLiteral("//")))
            continue;

        if (!parseDefinitionLine(line, order)) {
            allOk = false;
            m_errors.append(QStringLiteral("第 %1 行：%2").arg(i + 1).arg(m_errors.takeLast()));
            continue;
        }
        ++order;
    }

    LOG_INFO(QStringLiteral("单词规格解析完成：共 %1 个定义（其中宏 %2 个），错误 %3 条。")
                 .arg(m_defs.size())
                 .arg(m_macros.size())
                 .arg(m_errors.size()));
    return allOk;
}

/**
 * 功能：解析一条定义行。
 * 输入：line —— 已去空白的定义行；order —— 行序号。
 * 输出：成功为 true；失败时把原因追加到 m_errors。
 * 注意：
 *  1. 行格式必须是 "名字=正则式"，缺少 '=' 或名字为空均报错；
 *  2. 关键字类定义的右部是"用 | 分隔的多个关键字"，只登记编码不建自动机；
 *  3. 其它定义的右部是正则式，会被完整解析并做宏展开（通过 RegexParser 的宏表）。
 */
bool TokenSpec::parseDefinitionLine(const QString &line, int order)
{
    const int eq = line.indexOf(QLatin1Char('='));
    if (eq < 0) {
        m_errors.append(QStringLiteral("缺少 '='，无法识别为定义行：%1").arg(line));
        return false;
    }

    const QString rawName = line.left(eq).trimmed();
    const QString rawValue = line.mid(eq + 1).trimmed();
    if (rawName.isEmpty()) {
        m_errors.append(QStringLiteral("定义名为空：%1").arg(line));
        return false;
    }
    if (rawValue.isEmpty()) {
        m_errors.append(QStringLiteral("定义 %1 的正则式为空").arg(rawName));
        return false;
    }

    RegexDef def;
    def.rawRegex = rawValue;
    def.order = order;

    QString baseName;
    int code = -1;
    bool launch = false;
    bool hasCode = false;
    splitName(rawName, baseName, code, launch, hasCode);

    def.name = baseName;
    def.code = code;
    def.launch = launch;
    def.needsDfa = hasCode;  // 名字中带编码即需要生成 DFA 图（任务书 p.2 说明 1）

    // ---- 类别判定 ----
    if (!hasCode) {
        def.kind = RegexKind::Definition;
    } else if (isKeywordName(baseName)) {
        def.kind = RegexKind::Keyword;
    } else if (isCommentName(baseName)) {
        def.kind = RegexKind::Comment;
    } else {
        def.kind = RegexKind::Normal;
    }

    // ---- 关键字类：右部是关键字列表，逐个登记编码 ----
    //
    // 设计要点（对应任务书 p.2 说明 2 与 AGENT.md §6）：
    //   "Keyword103L=if | then | else | end" 这类定义带 L 后缀，表示"该命名对应
    //   多个单词、编码从 103 起连续分配"。因此这里**不构造自动机**，而是为每个
    //   关键字各生成一条 RegexKind::Keyword 条目，编码依次为 103、104、105、106。
    //
    //   这样做的原因：关键字与标识符的正则式完全重合，若把关键字也建成自动机
    //   规则，就会出现"同一个字符串同时匹配 ID 与关键字"的竞争，必须依赖长度
    //   或声明顺序裁定，语义含糊。正确做法（与本项目参考实现一致）是：自动机只管
    //   ID 的形态，识别出标识符后再查关键字常量表决定是否改判为关键字编码。
    if (def.kind == RegexKind::Keyword) {
        const QStringList parts = splitTopLevelAlternatives(rawValue);
        if (parts.isEmpty()) {
            m_errors.append(QStringLiteral("关键字定义 %1 未给出任何关键字").arg(rawName));
            return false;
        }
        // 去掉关键字两侧可能存在的转义反斜线：关键字是字面单词，不是正则式
        auto unescapeKeyword = [](const QString &token) {
            QString result;
            for (int i = 0; i < token.size(); ++i) {
                if (token.at(i) == QLatin1Char('\\') && i + 1 < token.size()) {
                    result += token.at(++i);
                } else {
                    result += token.at(i);
                }
            }
            return result;
        };

        for (int k = 0; k < parts.size(); ++k) {
            RegexDef kw = def;
            kw.keywordList = QStringList{unescapeKeyword(parts.at(k))};
            kw.tokenText = kw.keywordList.first();
            // 非 L 型关键字定义只允许有一个单词
            kw.code = def.launch ? (def.code + k) : def.code;
            if (!def.launch && k > 0) {
                m_errors.append(QStringLiteral("关键字定义 %1 未带 L 后缀却写了多个关键字")
                                    .arg(rawName));
                return false;
            }
            kw.launch = false;
            kw.needsDfa = false;
            kw.order = order + k;
            if (kw.code > 999) {
                m_errors.append(QStringLiteral("关键字 %1 的编码 %2 超出 3 位范围")
                                    .arg(kw.tokenText)
                                    .arg(kw.code));
                return false;
            }
            m_defs.append(kw);
        }
        return true;
    }

    // ---- 普通单词（含符号）带 L 后缀：右部各选择项各自分配连续编码 ----
    //
    // 例："Symbol200L= \+ | - | \* | ..." 共 16 个选择项 → 依次得到
    //     200(+) 201(-) 202(*) … 215(:=)，每个符号各成一条自动机规则。
    // 说明：参考实现同样为每个符号单独维护一个"字符串 → 编码"的映射，
    //       因此这里按选择项拆分是符合任务书语义的做法。
    if (def.kind == RegexKind::Normal && def.launch) {
        const QStringList parts = splitTopLevelAlternatives(rawValue);
        if (parts.isEmpty()) {
            m_errors.append(QStringLiteral("定义 %1 的右部为空").arg(rawName));
            return false;
        }
        for (int k = 0; k < parts.size(); ++k) {
            const QString part = parts.at(k);
            RegexParser parser;
            parser.setMacros(m_macros);
            RegexAst ast;
            if (!parser.parse(part, ast)) {
                m_errors.append(QStringLiteral("定义 %1 的第 %2 个选择项“%3”解析失败：%4")
                                    .arg(rawName)
                                    .arg(k + 1)
                                    .arg(part, parser.error()));
                return false;
            }
            RegexDef item = def;
            item.launch = false;
            item.code = def.code + k;
            item.rawRegex = part;
            item.expandedRegex = ast.toInfix();
            // 仅当选择项本身是"纯字面量"时登记字面单词（供生成的常量表使用）
            item.tokenText = astToLiteral(ast);
            item.order = order + k;
            if (item.code > 999) {
                m_errors.append(QStringLiteral("定义 %1 的第 %2 个选择项编码 %3 超出 3 位范围")
                                    .arg(rawName)
                                    .arg(k + 1)
                                    .arg(item.code));
                return false;
            }
            m_defs.append(item);
        }
        return true;
    }

    // ---- 其余类别：解析正则式 ----
    RegexParser parser;
    parser.setMacros(m_macros);
    RegexAst ast;
    if (!parser.parse(rawValue, ast)) {
        m_errors.append(QStringLiteral("定义 %1 的正则式解析失败：%2")
                            .arg(rawName, parser.error()));
        return false;
    }
    def.expandedRegex = ast.toInfix();
    def.tokenText = astToLiteral(ast);

    // 宏定义额外登记到宏表，供后续定义引用。
    //
    // 同时把**宏名**记到宏体的根结点上（仅当宏体恰好就是一个字符集）：
    //     letter=[A-Za-z]   →  根结点的原子名由 "[A-Za-z]" 改成 "letter"
    // 这样任务一的字符类字母表在命名时，用户看到的是 letter，而不是它展开后的
    // 那 52 个字母（见 lex/CharClass.h）。宏体若不是简单字符集（例如
    // m=letter|digit），则保留内部各原子自己的名字，不做改名。
    if (def.kind == RegexKind::Definition) {
        if (!ast.isEmpty() && ast.node(ast.root()).kind == RegexNodeKind::Charset) {
            ast.node(ast.root()).atomName = def.name;
            ast.node(ast.root()).atomFromMacro = true;
        }
        m_macros.insert(def.name, ast);
    }

    m_defs.append(def);
    return true;
}

/**
 * 功能：取需要构造自动机的定义（普通单词 + 注释）。
 * 输入：无。
 * 输出：定义列表（保持文件中的声明顺序）。
 * 注意：声明顺序很重要——多个接受状态同时成立时，按声明先后决定编码归属
 *       （即"先声明者优先"），因此这里不能重排。
 */
QVector<RegexDef> TokenSpec::automatonDefs() const
{
    QVector<RegexDef> result;
    for (const RegexDef &def : m_defs) {
        if (def.kind == RegexKind::Normal || def.kind == RegexKind::Comment)
            result.append(def);
    }
    return result;
}

/** 功能：取关键字定义。输入：无。输出：关键字定义列表。 */
QVector<RegexDef> TokenSpec::keywordDefs() const
{
    QVector<RegexDef> result;
    for (const RegexDef &def : m_defs) {
        if (def.kind == RegexKind::Keyword)
            result.append(def);
    }
    return result;
}

/**
 * 功能：取关键字表（单词 → 编码）。
 * 输入：无。
 * 输出：有序映射。
 * 注意：展开后每条关键字定义只含 1 个单词，因此直接以 tokenText 为键。
 */
QMap<QString, int> TokenSpec::keywordTable() const
{
    QMap<QString, int> table;
    for (const RegexDef &def : m_defs) {
        if (def.kind != RegexKind::Keyword)
            continue;
        for (const QString &word : def.keywordList) {
            if (!word.isEmpty())
                table.insert(word, def.code);
        }
    }
    return table;
}

/**
 * 功能：取专用符号表（符号原文 → 编码）。
 * 输入：无。
 * 输出：有序映射。
 * 注意：只登记 tokenText 非空（即纯字面量）的普通单词定义，
 *       因此 Num/ID/Comment 之类不会被误登记。
 */
QMap<QString, int> TokenSpec::symbolTable() const
{
    QMap<QString, int> table;
    for (const RegexDef &def : m_defs) {
        if (def.kind != RegexKind::Normal || def.tokenText.isEmpty())
            continue;
        table.insert(def.tokenText, def.code);
    }
    return table;
}

/**
 * 功能：取"字面单词 → 编码"的完整常量表。
 * 输入：无。
 * 输出：有序映射（关键字 + 专用符号）。
 * 注意：若同一单词在两张表中都出现，以符号表为准（先插关键字再插符号）。
 */
QMap<QString, int> TokenSpec::literalCodeTable() const
{
    QMap<QString, int> table = keywordTable();
    const QMap<QString, int> symbols = symbolTable();
    for (auto it = symbols.constBegin(); it != symbols.constEnd(); ++it)
        table.insert(it.key(), it.value());
    return table;
}

/**
 * 功能：生成规格汇总表（供界面①显示）。
 * 输入：headers、rows —— 输出参数。
 * 输出：无。
 * 注意：编码列对 Launch 型定义显示为 "start~end" 的区间，便于核对连续编码。
 */
void TokenSpec::buildSummaryTable(QStringList &headers, QList<QStringList> &rows) const
{
    headers = QStringList{QStringLiteral("序号"), QStringLiteral("名称"), QStringLiteral("编码"),
                          QStringLiteral("类别"),     QStringLiteral("正则式（原始）"),
                          QStringLiteral("逆波兰式 / 关键字")};
    rows.clear();

    for (int i = 0; i < m_defs.size(); ++i) {
        const RegexDef &def = m_defs.at(i);

        // 编码列
        QString codeText;
        if (def.code < 0) {
            codeText = QStringLiteral("—（宏）");
        } else if (def.launch) {
            const QVector<int> codes = def.codeList();
            if (codes.isEmpty())
                codeText = QString::number(def.code);
            else
                codeText = QStringLiteral("%1 ~ %2（L）").arg(codes.first()).arg(codes.last());
        } else {
            codeText = QString::number(def.code);
        }

        // 类别列
        QString kindText;
        switch (def.kind) {
        case RegexKind::Definition: kindText = QStringLiteral("宏定义"); break;
        case RegexKind::Normal:     kindText = QStringLiteral("普通单词"); break;
        case RegexKind::Comment:    kindText = QStringLiteral("注释（不输出）"); break;
        case RegexKind::Keyword:    kindText = QStringLiteral("关键字（不建自动机）"); break;
        }

        // 末列：关键字/符号的字面单词，或逆波兰式
        QString extra;
        if (def.kind == RegexKind::Keyword) {
            extra = QStringLiteral("关键字 %1 → 编码 %2")
                        .arg(def.tokenText.isEmpty() ? def.rawRegex : def.tokenText)
                        .arg(def.code);
        } else if (!def.tokenText.isEmpty()) {
            extra = QStringLiteral("字面量 %1　%2").arg(def.tokenText, def.expandedRegex);
        } else {
            extra = def.expandedRegex;
        }

        rows.append(QStringList{QString::number(i + 1), def.name, codeText, kindText,
                                def.rawRegex, extra});
    }
}

}  // namespace ccd
