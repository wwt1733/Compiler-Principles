/******************************************************************************
 * (1) 版权信息 : 编译原理课程设计 - 华南师范大学计算机学院
 * (2) 文件名称 : Grammar.cpp
 *     标识符   : Grammar
 *     摘要     : BNF 文法载入与管理的实现（第二项任务 2.1）。
 * (3) 版本号   : 1.0.0       作者/修改者 : <学号 姓名>   完成日期 : 2026-XX-XX
 * (4) 版本历史 : 1.0.0  2026-XX-XX  首次实现（阶段 4）
 *****************************************************************************/

#include "syntax/Grammar.h"

#include <QRegularExpression>
#include <QSet>

#include "common/FileUtil.h"

namespace ccd {

const QString Grammar::kAugmentedStart = QStringLiteral("S'");

/**
 * 功能：从文本载入文法。
 * 输入：text —— BNF 文本；error —— 输出失败原因。
 * 输出：成功为 true。
 * 注意（解析规则，与任务书 p.3 对齐）：
 *  1. 以 `#` 开头的整行是注释，直接跳过（因此空串必须写 `#` 出现在右部，
 *     不能写一整行 `#` 当产生式 —— 那种写法按注释处理）；
 *  2. 分隔符接受 `->` 与 `→` 两种写法（任务书 PDF 里是 `->`，
 *     老师讲稿里出现过全角箭头，一并兼容）；
 *  3. 右部用 `|` 分隔候选式；每个候选式内以空白切分符号；
 *  4. 候选式里若出现 `#`，既可能是"空串"也可能是"竖线后面什么都没写"，
 *     统一归一化为**空右部**（Production::right 为空），
 *     这样 later 的 FIRST/FOLLOW 与 LR 构造只需处理一种形式；
 *  5. 第一个**非终结符**即开始符号（不是第一条产生式左部）。
 */
bool Grammar::loadFromText(const QString &text, QString &error)
{
    clear();
    error.clear();

    if (text.trimmed().isEmpty()) {
        error = QStringLiteral("文法内容为空。");
        return false;
    }

    const QStringList lines = FileUtil::splitMeaningfulLines(text);
    int lineNo = 0;
    int firstNonterminalLine = -1;

    for (const QString &rawLine : lines) {
        ++lineNo;
        const QString line = rawLine.trimmed();
        if (line.isEmpty() || line.startsWith(QLatin1Char('#')))
            continue;

        // 定位分隔符：优先 `->`，其次全角 `→`
        int arrow = line.indexOf(QStringLiteral("->"));
        int arrowLen = 2;
        if (arrow < 0) {
            arrow = line.indexOf(QStringLiteral("→"));
            arrowLen = 1;
        }
        if (arrow < 0) {
            error = QStringLiteral("第 %1 行：缺少产生式分隔符 \"->\"：%2").arg(lineNo).arg(line);
            return false;
        }

        const QString lhs = line.left(arrow).trimmed();
        const QString rhsText = line.mid(arrow + arrowLen).trimmed();
        if (lhs.isEmpty()) {
            error = QStringLiteral("第 %1 行：产生式左部为空。").arg(lineNo);
            return false;
        }
        if (lhs.contains(QLatin1Char(' ')) || lhs.contains(QLatin1Char('\t'))) {
            error = QStringLiteral("第 %1 行：产生式左部不能含空白（左部必须是单个非终结符）：%2")
                        .arg(lineNo)
                        .arg(lhs);
            return false;
        }

        if (firstNonterminalLine < 0) {
            firstNonterminalLine = lineNo;
            m_startSymbol = lhs;
        }

        // 按顶层 `|` 切分候选式（这里没有括号，直接切即可）
        const QStringList alternatives = rhsText.split(QLatin1Char('|'));
        for (const QString &altRaw : alternatives) {
            Production prod;
            prod.left = lhs;

            const QString alt = altRaw.trimmed();
            // 空候选式或单个 `#` 一律视为 ε 产生式（right 保持为空）
            if (!alt.isEmpty() && alt != QStringLiteral("#")) {
                const QStringList syms = alt.split(QRegularExpression(QStringLiteral("\\s+")),
                                                  Qt::SkipEmptyParts);
                for (const QString &s : syms) {
                    // 候选式内部也可能混入 `#`（如 "A -> B #"），按"忽略多余 ε"处理
                    if (s == QStringLiteral("#"))
                        continue;
                    prod.right.append(s);
                }
            }
            prod.id = m_productions.size();
            m_productions.append(prod);
        }
    }

    if (m_productions.isEmpty()) {
        error = QStringLiteral("未解析出任何产生式，请检查文法格式（应为 `左部 -> 右部`）。");
        return false;
    }

    rebuildSymbolLists();

    // 开始符号必须是非终结符（它必然出现在左部，因为 firstNonterminalLine 记录的是左部）
    if (!isNonterminal(m_startSymbol)) {
        error = QStringLiteral("第 %1 行的左部 %2 不是非终结符，无法作为开始符号。")
                    .arg(firstNonterminalLine)
                    .arg(m_startSymbol);
        return false;
    }

    return true;
}

/**
 * 功能：从文件载入文法。
 * 输入：path —— 文件路径；error —— 输出失败原因。
 * 输出：成功为 true。
 * 注意：读文件统一走 FileUtil::readText（内部已处理 BOM 与换行差异）。
 */
bool Grammar::loadFromFile(const QString &path, QString &error)
{
    bool ok = false;
    const QString text = FileUtil::readText(path, &ok);
    if (!ok) {
        error = QStringLiteral("无法读取文法文件：%1").arg(path);
        return false;
    }
    return loadFromText(text, error);
}

/**
 * 功能：重建非终结符与终结符列表。
 * 输入：无。
 * 输出：无。
 * 注意：必须以**首次出现顺序**填充，因为后续界面的分析表、First/Follow 表
 *       都按这个顺序展示，顺序稳定才能让截图与文档对得上。
 */
void Grammar::rebuildSymbolLists()
{
    m_nonterminals.clear();
    m_terminals.clear();
    QSet<QString> seenNonterminal;
    QSet<QString> seenTerminal;

    // 第一遍：所有左部都是非终结符
    for (const Production &p : m_productions) {
        if (!seenNonterminal.contains(p.left)) {
            seenNonterminal.insert(p.left);
            m_nonterminals.append(p.left);
        }
    }
    // 第二遍：右部中不是非终结符的符号就是终结符
    for (const Production &p : m_productions) {
        for (const QString &s : p.right) {
            if (seenNonterminal.contains(s) || seenTerminal.contains(s))
                continue;
            seenTerminal.insert(s);
            m_terminals.append(s);
        }
    }
}

/** 功能：取第 index 条产生式。输入：index。输出：产生式常引用（越界返回静态空对象）。 */
const Production &Grammar::production(int index) const
{
    static const Production empty;
    if (index < 0 || index >= m_productions.size())
        return empty;
    return m_productions.at(index);
}

/** 功能：判断是否非终结符。输入：symbol。输出：是则为 true。 */
bool Grammar::isNonterminal(const QString &symbol) const
{
    return m_nonterminals.contains(symbol);
}

/** 功能：判断是否终结符。输入：symbol。输出：是则为 true。 */
bool Grammar::isTerminal(const QString &symbol) const
{
    return m_terminals.contains(symbol);
}

/** 功能：判断是否为文法符号。输入：symbol。输出：是则为 true。 */
bool Grammar::isSymbol(const QString &symbol) const
{
    return isNonterminal(symbol) || isTerminal(symbol);
}

/** 功能：按左部取产生式序号。输入：lhs。输出：序号列表（有序）。 */
QVector<int> Grammar::productionsOf(const QString &lhs) const
{
    QVector<int> result;
    for (const Production &p : m_productions) {
        if (p.left == lhs)
            result.append(p.id);
    }
    return result;
}

/**
 * 功能：取终结符对应的单词编码。
 * 输入：symbol —— 终结符名。
 * 输出：编码；未登记时返回 -1。
 * 注意：逆表**惰性构建** —— 只在第一次查询时把 m_terminalNames 翻转一次。
 *       若返回 -1，说明文法里的终结符名与编码表不一致（多半是拼写错误），
 *       语法分析阶段应当把它当成"不该出现的符号"报错而不是静默接受。
 */
int Grammar::terminalCode(const QString &symbol) const
{
    if (!m_codesBuilt) {
        m_terminalCodes.clear();
        for (auto it = m_terminalNames.constBegin(); it != m_terminalNames.constEnd(); ++it)
            m_terminalCodes.insert(it.value(), it.key());
        m_codesBuilt = true;
    }
    return m_terminalCodes.value(symbol, -1);
}

/** 功能：取"终结符名 → 编码"表。输入：无。输出：映射常引用（首次调用时构建）。 */
const QMap<QString, int> &Grammar::terminalCodes() const
{
    // 借 terminalCode() 触发惰性构建（传入空名不会命中任何表项，仅驱动构建）
    terminalCode(QString());
    return m_terminalCodes;
}

/**
 * 功能：注入"单词编码 → 终结符名"表。
 * 输入：names —— 编码到终结符名的映射。
 * 输出：无。
 * 注意：注入后必须把 m_codesBuilt 置为 false，否则逆表会继续使用旧内容 ——
 *       那是"切换语言后编码翻译错乱"的经典成因。
 */
void Grammar::setTerminalNames(const QMap<int, QString> &names)
{
    m_terminalNames = names;
    m_terminalCodes.clear();
    m_codesBuilt = false;
}

/** 功能：取带编码的符号显示文本。输入：symbol。输出：形如 "SEMI(214)" 或原符号。 */
QString Grammar::symbolWithCode(const QString &symbol) const
{
    const int code = terminalCode(symbol);
    if (code < 0)
        return symbol;
    return QStringLiteral("%1(%2)").arg(symbol).arg(code);
}

/**
 * 功能：建立增广文法，追加 `S' -> S`。
 * 输入：prime —— 增广符号名。
 * 输出：无。
 * 注意：
 *  1. **幂等**：若文法里已存在左部为 prime 的产生式（无论它是不是被追加的），
 *     就直接认定已增广并返回。不能只靠 m_augmented 标志判断 ——
 *     一个原本就含 `S' -> program` 的文法（例如化简流程往返过一次）
 *     载入后标志为 false，再加一条就出现两条 S' 产生式，
 *     LR(0) 初态会同时含两个项目，分析表随之整个错乱；
 *  2. 增广产生式**追加在末尾**，然后统一重编全部产生式 id；
 *     不插在开头是为了让"前 N 条 = 用户写的产生式"这个对应关系
 *     在语义动作文件（AGENT.md §10，按产生式顺序一行一条）里保持成立；
 *  3. `S'` 要插入非终结符列表**首位**，使 LR(0) 初态从它开始。
 */
void Grammar::augment(const QString &prime)
{
    if (m_augmented || m_startSymbol.isEmpty())
        return;

    // 幂等检查：已存在左部为 prime 的产生式就认为已增广
    for (const Production &p : m_productions) {
        if (p.left == prime) {
            m_augmentedStart = prime;
            m_augmented = true;
            m_nonterminals.removeAll(prime);
            m_nonterminals.prepend(prime);
            return;
        }
    }

    Production prod;
    prod.left = prime;
    prod.right.append(m_startSymbol);
    m_productions.append(prod);

    // 统一重编 id：后续 LR 项目、分析表、语义动作文件都以下标为准
    for (int i = 0; i < m_productions.size(); ++i)
        m_productions[i].id = i;

    m_augmentedStart = prime;
    m_augmented = true;

    rebuildSymbolLists();
    // 把 S' 提到最前面
    m_nonterminals.removeAll(prime);
    m_nonterminals.prepend(prime);
}

/** 功能：用另一个文法整体替换当前内容。输入：other。输出：无。 */
void Grammar::assignFrom(const Grammar &other)
{
    m_productions = other.m_productions;
    m_nonterminals = other.m_nonterminals;
    m_terminals = other.m_terminals;
    m_startSymbol = other.m_startSymbol;
    m_augmentedStart = other.m_augmentedStart;
    m_augmented = other.m_augmented;
    m_terminalNames = other.m_terminalNames;
    m_terminalCodes = other.m_terminalCodes;
    m_codesBuilt = other.m_codesBuilt;
}

/** 功能：清空全部内容。输入：无。输出：无。 */
void Grammar::clear()
{
    m_productions.clear();
    m_nonterminals.clear();
    m_terminals.clear();
    m_startSymbol.clear();
    m_augmentedStart.clear();
    m_augmented = false;
    // 注意：**不清空** m_terminalNames / m_terminalCodes —— 编码表属于语言配置，
    //       与文法内容无关；清空会导致重新载入文法后解析器丢失
    //       "编码 → 终结符名"的翻译能力。
}

/**
 * 功能：输出文法全文。
 * 输入：无。
 * 输出：每条产生式一行；ε 产生式显示为 `A -> #`。
 * 注意：同一左部的多条产生式**不合并**成带 `|` 的一行，
 *       因为产生式序号（id）是 LR 构造与分析表的关键，合并会打乱对应关系。
 */
QString Grammar::toText() const
{
    QStringList lines;
    for (const Production &p : m_productions) {
        if (p.isEpsilon()) {
            lines << QStringLiteral("%1 -> #").arg(p.left);
        } else {
            lines << QStringLiteral("%1 -> %2").arg(p.left, p.right.join(QLatin1Char(' ')));
        }
    }
    return lines.join(QLatin1Char('\n'));
}

/** 功能：一句话摘要。输入：无。输出：摘要文本。 */
QString Grammar::summary() const
{
    return QStringLiteral("产生式 %1 条；非终结符 %2 个；终结符 %3 个；开始符号 %4%5")
        .arg(m_productions.size())
        .arg(m_nonterminals.size())
        .arg(m_terminals.size())
        .arg(m_startSymbol.isEmpty() ? QStringLiteral("(未设置)") : m_startSymbol)
        .arg(m_augmented ? QStringLiteral("；已增广（%1 -> %2）")
                               .arg(m_augmentedStart, m_startSymbol)
                         : QString());
}

}  // namespace ccd
