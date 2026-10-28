/******************************************************************************
 * (1) 版权信息 : 编译原理课程设计 - 华南师范大学计算机学院
 * (2) 文件名称 : StateTableView.cpp
 *     标识符   : StateTableView
 *     摘要     : 自动机状态转换表视图的实现。把 NFA/DFA 的邻接表整理成
 *                "状态 × 输入符号"的二维状态转换表呈现，并支持导出为
 *                Graphviz DOT 文件与 PNG 图片。
 * (3) 版本号   : 1.0.0       作者/修改者 : <学号 姓名>   完成日期 : 2026-XX-XX
 * (4) 版本历史 : 1.0.0  2026-XX-XX  首次实现
 *****************************************************************************/

#include "gui/widgets/StateTableView.h"

#include <QColor>
#include <QDir>
#include <QFileDialog>
#include <QFileInfo>
#include <QFont>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QMessageBox>
#include <QProcess>
#include <QPushButton>
#include <QTableWidget>
#include <QVBoxLayout>

#include "common/FileUtil.h"
#include "common/Logger.h"
#include "lex/CharClass.h"

namespace ccd {

namespace {

/**
 * 把符号显示成人类可读的**可见 ASCII** 形式。
 *
 * 注意两点：
 *  1. 这里**只做"字节 → 可见 ASCII"的映射，不做 DOT 转义**。
 *     转义统一在拼接标签时由 `FileUtil::escapeForDot()` 做一次 ——
 *     若此处也转义，引号与反斜杠会被转义两遍，图上会多出可见的反斜杠
 *     （本项目实际踩过：标签里出现 `\"` 而不是 `"`）。
 *  2. 不能"不认识的符号就原样返回"。按字节模式的字母表可能覆盖全部 256 个
 *     字节值（AGENT.md §13.11），原样写入会让 .dot 里出现 NUL / CR / 0x0B 等
 *     控制字节与 0x80~0xFF 的高位字节，Graphviz 直接报
 *     `syntax error ... scanning a quoted string` 并且整张图画不出来。
 *     具体映射由 `CharClassBuilder::visibleByte()` 统一实现（AGENT.md §13.29）。
 */
QString displaySymbol(const QString &symbol)
{
    if (symbol.isEmpty())
        return QStringLiteral("<eps>");

    QString out;
    out.reserve(symbol.size() + 8);
    for (int i = 0; i < symbol.size(); ++i) {
        const ushort u = symbol.at(i).unicode();
        if (u > 0xFF)
            out += QStringLiteral("<U+%1>")
                       .arg(QString::number(u, 16).rightJustified(4, QLatin1Char('0')).toUpper());
        else
            out += CharClassBuilder::visibleByte(u);
    }
    return out;
}

}  // namespace

/**
 * 功能：构造状态转换表视图。
 * 输入：parent —— 父控件。
 * 输出：无。
 * 注意：构造后表格为空，需调用 setNfa() / setDfa() 填充数据。
 */
StateTableView::StateTableView(QWidget *parent) : QWidget(parent)
{
    buildUi();
}

/**
 * 功能：构建界面控件与布局。
 * 输入：无。
 * 输出：无。
 * 注意：表格使用"选择整行 + 不可编辑"，接受态行用浅黄色高亮以便肉眼定位。
 */
void StateTableView::buildUi()
{
    auto *rootLayout = new QVBoxLayout(this);
    rootLayout->setContentsMargins(8, 8, 8, 8);
    rootLayout->setSpacing(6);

    m_captionLabel = new QLabel(this);
    m_captionLabel->setWordWrap(true);
    QFont captionFont = m_captionLabel->font();
    captionFont.setBold(true);
    m_captionLabel->setFont(captionFont);
    rootLayout->addWidget(m_captionLabel);

    auto *buttonBar = new QHBoxLayout();
    buttonBar->setSpacing(6);
    m_exportDotButton = new QPushButton(QStringLiteral("导出 DOT 文件…"), this);
    m_exportPngButton = new QPushButton(QStringLiteral("导出 PNG 图片…"), this);
    m_exportDotButton->setToolTip(QStringLiteral(
        "导出 Graphviz DOT 文本，可用 Graphviz/在线工具渲染为状态转换图"));
    m_exportPngButton->setToolTip(QStringLiteral(
        "自动调用 Graphviz 的 dot.exe 把当前图渲染为 PNG；需系统已安装 Graphviz"));
    buttonBar->addWidget(m_exportDotButton);
    buttonBar->addWidget(m_exportPngButton);
    buttonBar->addStretch(1);
    rootLayout->addLayout(buttonBar);

    m_table = new QTableWidget(this);
    m_table->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_table->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_table->setAlternatingRowColors(true);
    m_table->verticalHeader()->setVisible(false);
    rootLayout->addWidget(m_table, 1);

    // "字符类对照"图例：默认收起。
    // 为什么需要它：按字符类字母表时，列标题是 letter / digit / <other> 这样的名字，
    // 光看图看不出 <other> 到底包含哪些字节，这里给出每一类的精确字节集。
    m_legendGroup = new QGroupBox(QStringLiteral("字符类对照（点击展开）"), this);
    m_legendGroup->setCheckable(true);
    m_legendGroup->setChecked(false);
    auto *legendLayout = new QVBoxLayout(m_legendGroup);
    legendLayout->setContentsMargins(8, 4, 8, 4);
    m_legendLabel = new QLabel(m_legendGroup);
    m_legendLabel->setWordWrap(true);
    m_legendLabel->setTextInteractionFlags(Qt::TextSelectableByMouse);
    QFont legendFont = m_legendLabel->font();
    legendFont.setStyleHint(QFont::Monospace);
    legendFont.setFamily(QStringLiteral("Consolas"));
    m_legendLabel->setFont(legendFont);
    m_legendLabel->setStyleSheet(QStringLiteral("color:#333333;"));
    legendLayout->addWidget(m_legendLabel);
    m_legendGroup->setVisible(false);
    // 展开/收起时把内容一起显示/隐藏（QGroupBox 的 checkable 只切换标题外观）
    connect(m_legendGroup, &QGroupBox::toggled, m_legendLabel, &QLabel::setVisible);
    rootLayout->addWidget(m_legendGroup);

    m_hintLabel = new QLabel(
        QStringLiteral("说明：单元格中的数字为目的状态编号；ε 列表示空串转移（仅 NFA 有）。"),
        this);
    m_hintLabel->setStyleSheet(QStringLiteral("color:#555555;"));
    m_hintLabel->setWordWrap(true);
    rootLayout->addWidget(m_hintLabel);

    connect(m_exportDotButton, &QPushButton::clicked, this, &StateTableView::exportDot);
    connect(m_exportPngButton, &QPushButton::clicked, this, &StateTableView::exportPng);
}

// ---------------------------------------------------------------------------
// 数据填充
// ---------------------------------------------------------------------------

/**
 * 功能：用 DFA 数据填充表格。
 * 输入：dfa —— DFA 数据；caption —— 表格上方说明文字。
 * 输出：无。
 * 注意：DFA 无 ε 边，故列标题只含字母表；接受态行标注其单词编码。
 */
void StateTableView::setDfa(const Dfa &dfa, const QString &caption)
{
    m_alphabet = dfa.alphabet;
    m_alphabetInfo = dfa.alphabetInfo;
    m_startState = dfa.start;
    m_stateCount = dfa.stateCount;

    // ---- 供 DOT 用的边列表与接受态标签 ----
    // DFA 没有 ε 边，所以这里只会产生实线边；ε 的处理见 setNfa()。
    m_dotEdges.clear();
    for (int state = 0; state < dfa.stateCount; ++state) {
        const QMap<QString, int> &row = dfa.trans.at(state);
        for (auto it = row.constBegin(); it != row.constEnd(); ++it) {
            if (it.value() < 0)
                continue;
            addDotEdge(state, it.value(), false, symbolText(it.key()));
        }
    }
    m_acceptLabel.clear();
    m_acceptLabel.resize(dfa.stateCount);
    for (int state = 0; state < dfa.stateCount; ++state) {
        if (!dfa.accepting.value(state, false))
            continue;
        // 第二行仍以"编码"为主（与改造前的 DFA 图外观保持一致）；
        // 注释态的 acceptCode 是声明值，这里不特殊处理 —— 表格的"说明"列
        // 已经把"注释不输出编码"讲清楚了，图上保持简洁。
        const int code = dfa.acceptCode.value(state, -1);
        m_acceptLabel[state] = (code >= 0) ? QString::number(code)
                                           : QStringLiteral("no-code");
    }

    QStringList headers;
    headers << QStringLiteral("状态") << QStringLiteral("说明");
    for (const QString &symbol : m_alphabet)
        headers << symbolText(symbol);
    resetTable(headers);

    int acceptCount = 0;
    for (int state = 0; state < dfa.stateCount; ++state) {
        QStringList cells;
        cells << QString::number(state);

        // 说明列：初态 / 接受态 / 对应的 NFA 状态集合
        // 注意：是否接受态一律以 dfa.accepting 标志为准；acceptCode 为 -1 只表示
        //       "该接受态不输出编码"（单正则式 NFA 或注释类规则），而非非接受态。
        QStringList notes;
        if (state == dfa.start)
            notes << QStringLiteral("初态");
        const bool isAccept = (state < dfa.accepting.size() && dfa.accepting.at(state));
        if (isAccept) {
            const int code = dfa.acceptCode.value(state, -1);
            // 注释类的 acceptCode 是**声明值**（如 102），但它实际上不输出编码，
            // 只能靠 acceptIsComment 区分。若只看 acceptCode，界面上会把注释态
            // 显示成"接受态(编码 102)"，与真实行为相反。
            const bool isComment = dfa.acceptIsComment.value(state, false);
            if (isComment)
                notes << QStringLiteral("接受态(注释，不输出编码)");
            else if (code >= 0)
                notes << QStringLiteral("接受态(编码 %1)").arg(code);
            else
                notes << QStringLiteral("接受态(不输出编码)");
            ++acceptCount;
        }
        if (state < dfa.subsets.size() && !dfa.subsets.at(state).isEmpty()) {
            QList<int> subset = dfa.subsets.at(state).values();
            std::sort(subset.begin(), subset.end());
            QStringList parts;
            for (int s : subset)
                parts << QString::number(s);
            notes << QStringLiteral("NFA{%1}").arg(parts.join(QLatin1Char(',')));
        }
        cells << notes.join(QStringLiteral(" "));

        for (const QString &symbol : m_alphabet) {
            const auto it = dfa.trans.at(state).constFind(symbol);
            cells << (it == dfa.trans.at(state).constEnd() ? QString() : QString::number(it.value()));
        }
        appendRow(cells, isAccept);
    }

    m_dotText = buildDotText();
    setCaption(caption);
    updateLegend();
    emit statisticsChanged(
        QStringLiteral("DFA：%1 个状态（其中接受态 %2 个），字母表 %3 个%4")
            .arg(dfa.stateCount)
            .arg(acceptCount)
            .arg(m_alphabet.size())
            .arg(m_alphabetInfo.isEmpty() ? QStringLiteral("符号") : QStringLiteral("字符类")));
}

/**
 * 功能：用 NFA 数据填充表格。
 * 输入：nfa —— NFA 数据；caption —— 表格上方说明文字。
 * 输出：无。
 * 注意：ε 作为独立的一列出现，且放在第一列位置之前（紧随说明列）以便阅读。
 */
void StateTableView::setNfa(const Nfa &nfa, const QString &caption)
{
    // 按字符类模式时字母表已经由构造器按"列序"填好（nfa.alphabet），
    // 只有按字节模式才需要现场收集。
    m_alphabet = nfa.alphabet.isEmpty() ? nfa.collectAlphabet() : nfa.alphabet;
    m_alphabetInfo = nfa.alphabetInfo;
    m_startState = nfa.start;
    m_stateCount = nfa.stateCount;

    // 建立状态转换表：trans[from][symbol] —— 同一格可能有多个目的状态，用逗号连接
    QVector<QMap<QString, QStringList>> table;
    table.resize(nfa.stateCount);
    for (const Transition &e : nfa.edges) {
        if (e.from < 0 || e.from >= nfa.stateCount)
            continue;
        const QString key = e.isEpsilon ? QStringLiteral("ε") : e.symbol;
        table[e.from][key].append(QString::number(e.to));
    }

    // ---- 供 DOT 用的边列表：**逐条照抄 nfa.edges，ε 边也算** ----
    // 这是本次修复的核心：过去 DOT 是从 m_trans（"一符号一目的"的 DFA 结构）里凑的，
    // ε 边与"一格多目的"根本没进去，于是 Thompson NFA 的图里一条 ε 边都没有，
    // 大半状态看着是孤立的（用户反馈"怪怪的"）。
    m_dotEdges.clear();
    for (const Transition &e : nfa.edges) {
        if (e.from < 0 || e.from >= nfa.stateCount || e.to < 0)
            continue;
        addDotEdge(e.from, e.to, e.isEpsilon, e.isEpsilon ? QString() : symbolText(e.symbol));
    }

    // ---- 接受态标签：用**规则名**（如 ID、Comment），比裸编码更适合 NFA 图 ----
    // 判据必须是 nfa.accepts 里有这个状态（= accepting），而不是"有没有编码"：
    // 注释规则的接受态 acceptCode 是 -1，用"有编码"判它会把它画成单圈（§13.33）。
    m_acceptLabel.clear();
    m_acceptLabel.resize(nfa.stateCount);
    for (auto it = nfa.accepts.constBegin(); it != nfa.accepts.constEnd(); ++it) {
        const int state = it.key();
        if (state < 0 || state >= nfa.stateCount)
            continue;
        QStringList names;
        for (const AcceptTag &tag : it.value()) {
            if (tag.defName.isEmpty() || names.contains(tag.defName))
                continue;
            names << tag.defName;
        }
        m_acceptLabel[state] = names.join(QLatin1Char('/'));
    }

    QStringList headers;
    headers << QStringLiteral("状态") << QStringLiteral("说明") << QStringLiteral("ε");
    for (const QString &symbol : m_alphabet)
        headers << symbolText(symbol);
    resetTable(headers);

    int acceptCount = 0;
    for (int state = 0; state < nfa.stateCount; ++state) {
        QStringList cells;
        cells << QString::number(state);

        QStringList notes;
        if (state == nfa.start)
            notes << QStringLiteral("初态");
        if (nfa.accepts.contains(state)) {
            const QVector<AcceptTag> &tags = nfa.accepts.value(state);
            QStringList tagTexts;
            for (const AcceptTag &tag : tags) {
                tagTexts << (tag.isComment
                                 ? QStringLiteral("%1(注释)").arg(tag.defName)
                                 : QStringLiteral("%1(编码 %2)").arg(tag.defName).arg(tag.code));
            }
            notes << QStringLiteral("接受态 ") + tagTexts.join(QStringLiteral("/"));
            ++acceptCount;
        }
        cells << (notes.isEmpty() ? QString() : notes.join(QStringLiteral(" ")));

        const QStringList eps = table.at(state).value(QStringLiteral("ε"));
        cells << eps.join(QStringLiteral(","));
        for (const QString &symbol : m_alphabet) {
            const QStringList dests = table.at(state).value(symbol);
            cells << dests.join(QStringLiteral(","));
        }
        appendRow(cells, nfa.accepts.contains(state));
    }

    m_dotText = buildDotText();
    setCaption(caption);
    updateLegend();
    emit statisticsChanged(QStringLiteral("NFA：%1 个状态（其中接受态 %2 个），字母表 %3 个%4")
                               .arg(nfa.stateCount)
                               .arg(acceptCount)
                               .arg(m_alphabet.size())
                               .arg(m_alphabetInfo.isEmpty() ? QStringLiteral("符号")
                                                             : QStringLiteral("字符类")));
}

/**
 * 功能：清空表格与标题。
 * 输入：无。
 * 输出：无。
 * 注意：同时清空内部缓存的字母表与转换表，避免导出旧图。
 */
void StateTableView::clearAll()
{
    m_table->clear();
    m_table->setRowCount(0);
    m_table->setColumnCount(0);
    m_alphabet.clear();
    m_alphabetInfo = CharClassAlphabet();
    m_stateCount = 0;
    m_dotEdges.clear();
    m_acceptLabel.clear();
    m_dotText.clear();
    m_lastDotPath.clear();
    m_captionLabel->clear();
    if (m_legendGroup != nullptr) {
        m_legendLabel->clear();
        m_legendGroup->setVisible(false);
    }
}

/** 功能：设置说明文字。输入：caption —— 说明。输出：无。 */
void StateTableView::setCaption(const QString &caption)
{
    m_captionLabel->setText(caption);
}

/** 功能：【自测用】表格列数。输入：无。输出：列数。 */
int StateTableView::columnCount() const
{
    return (m_table != nullptr) ? m_table->columnCount() : 0;
}

/** 功能：【自测用】字符类图例文本。输入：无。输出：图例文本（无类表时为空串）。 */
QString StateTableView::charClassLegend() const
{
    return m_alphabetInfo.isEmpty() ? QString() : m_alphabetInfo.legendText();
}

/**
 * 功能：把字母表符号显示成列标题 / 图上的边标签。
 * 输入：symbol —— 字母表符号。
 * 输出：字符类模式下的类名，或按字节模式下的可见 ASCII 记号。
 * 注意：类名可能含 `<`、`>`、引号、反斜杠（如 `<other>`、`"`），
 *       它们是**普通字符**，DOT 转义由调用处统一处理一次（escapeForDot）。
 */
QString StateTableView::symbolText(const QString &symbol) const
{
    if (!m_alphabetInfo.isEmpty()) {
        const QString name = m_alphabetInfo.nameOfSymbol(symbol);
        if (!name.isEmpty())
            return name;
    }
    return displaySymbol(symbol);
}

/**
 * 功能：刷新"字符类对照"图例。
 * 输入：无（使用 m_alphabetInfo）。
 * 输出：无。
 * 注意：无字符类字母表（按字节模式）时整块隐藏 —— 那时列标题本身就是字节，
 *       再来一份"每类 = 哪些字节"的对照反而没有意义。
 */
void StateTableView::updateLegend()
{
    if (m_legendGroup == nullptr || m_legendLabel == nullptr)
        return;
    if (m_alphabetInfo.isEmpty()) {
        m_legendLabel->clear();
        m_legendGroup->setVisible(false);
        return;
    }
    m_legendGroup->setTitle(
        QStringLiteral("字符类对照（%1 个字符类，点击展开）").arg(m_alphabetInfo.classes.size()));
    m_legendLabel->setText(m_alphabetInfo.legendText());
    m_legendGroup->setVisible(true);
    m_legendLabel->setVisible(m_legendGroup->isChecked());
}

// ---------------------------------------------------------------------------
// 内部工具
// ---------------------------------------------------------------------------

/** 功能：按列标题重建表格骨架。输入：headers —— 列标题。输出：无。 */
void StateTableView::resetTable(const QStringList &headers)
{
    m_table->clear();
    m_table->setColumnCount(headers.size());
    m_table->setHorizontalHeaderLabels(headers);
    m_table->setRowCount(0);
}

/**
 * 功能：追加一行数据。
 * 输入：cells —— 各列文本；highlight —— 是否高亮（接受态用浅黄色）。
 * 输出：无。
 * 注意：用 QTableWidgetItem 的 setData 保存原始状态号，便于后续排序扩展。
 */
void StateTableView::appendRow(const QStringList &cells, bool highlight)
{
    const int row = m_table->rowCount();
    m_table->insertRow(row);
    for (int col = 0; col < m_table->columnCount(); ++col) {
        const QString text = (col < cells.size()) ? cells.at(col) : QString();
        auto *item = new QTableWidgetItem(text);
        if (highlight) {
            item->setBackground(QColor(255, 250, 205));  // 浅黄：接受态
        }
        m_table->setItem(row, col, item);
    }
}

/**
 * 功能：ε 边的标签文本。
 * 输入：无。
 * 输出：`ε`（U+03B5）。
 * 注意：**全项目 DOT 里唯一允许出现的非 ASCII 字符**，所以它不过
 *       `FileUtil::escapeForDot()` —— 那会把它转成 `<U+03B5>`，图上就成了
 *       一串没有意义的尖括号。其余标签仍然必须是可打印 ASCII（AGENT.md §13.29）。
 *       万一某台机器上 Graphviz 把这个字渲染成方框，把返回值改成 `"eps"` 即可，
 *       别处（包括自测断言）都是取本函数的返回值来比对的。
 */
QString StateTableView::epsilonDotLabel()
{
    return QStringLiteral("ε");
}

/**
 * 功能：把一条边加入 DOT 用的边列表。
 * 输入：from/to —— 起止状态；epsilon —— 是否 ε 边；label —— 标签（ε 边传空串）。
 * 输出：无。
 * 注意：同一 (源,目的,是否ε) 上的标签会合并去重并排序，保证：
 *  1. 图上不会出现两条 (源,目的) 完全相同的边（那样会糊在一起）；
 *  2. ε 边与符号边即使连接同一对状态也各自成一条（一条虚线、一条实线）；
 *  3. 输出稳定可复现（同一份数据多次导出逐字节相同）。
 */
void StateTableView::addDotEdge(int from, int to, bool epsilon, const QString &label)
{
    for (DotEdge &edge : m_dotEdges) {
        if (edge.from != from || edge.to != to || edge.epsilon != epsilon)
            continue;
        if (!label.isEmpty() && !edge.labels.contains(label))
            edge.labels.append(label);
        return;
    }
    DotEdge edge;
    edge.from = from;
    edge.to = to;
    edge.epsilon = epsilon;
    if (!label.isEmpty())
        edge.labels.append(label);
    m_dotEdges.append(edge);
}

/**
 * 功能：生成 Graphviz DOT 文本。
 * 输入：无（使用 m_dotEdges / m_acceptLabel / m_stateCount / m_startState）。
 * 输出：DOT 文本；没有状态时返回空串。
 * 注意：
 *  1. 边的数据源是 **m_dotEdges**，不是状态转换表 —— 后者是"一符号一目的"的
 *     DFA 结构，ε 边与多目的格都进不去。过去正是因为这个，导出的 NFA 图里
 *     一条 ε 边都没有，Thompson NFA 的图看起来像一堆孤立的状态（用户反馈）。
 *  2. ε 边画**虚线**并标 `ε`，与"消耗一个字符"的实线边区分开（教科书画法）。
 *  3. 接受态画双圈（shape=doublecircle），第二行写编码（DFA）或规则名（NFA）；
 *     判据是 m_acceptLabel 是否为空，即数据里的 accepting 标志 —— **不能**用
 *     "有没有编码"，注释规则的接受态编码是 -1（§13.33）。
 */
QString StateTableView::buildDotText() const
{
    if (m_stateCount <= 0)
        return QString();

    QString dot;
    dot += QStringLiteral("digraph Automaton {\n");
    dot += QStringLiteral("  rankdir=LR;\n");
    // 标签已由 symbolText()/escapeForDot() 保证是纯可打印 ASCII（唯一的例外是
    // ε 边标签），因此用通用字体即可，不再依赖中文字体（换机后不会因缺字体变形）。
    dot += QStringLiteral("  node [shape=circle, fontname=\"Helvetica\"];\n");
    dot += QStringLiteral("  edge [fontname=\"Helvetica\"];\n");
    dot += QStringLiteral("  __start [shape=point, width=0.1];\n");

    for (int state = 0; state < m_stateCount; ++state) {
        const QString label = (state < m_acceptLabel.size()) ? m_acceptLabel.at(state) : QString();
        if (label.isEmpty()) {
            dot += QStringLiteral("  q%1 [label=\"%1\"];\n").arg(state);
        } else {
            dot += QStringLiteral("  q%1 [shape=doublecircle, label=\"%1\\n(%2)\"];\n")
                       .arg(state)
                       .arg(FileUtil::escapeForDot(label));
        }
    }
    dot += QStringLiteral("  __start -> q%1;\n").arg(m_startState);

    // 顺序由 m_dotEdges 的插入顺序决定（构建时按 nfa.edges / dfa.trans 的顺序），
    // 因此同一份数据多次导出得到的 DOT 逐字节相同，便于比对与写进报告。
    for (const DotEdge &edge : m_dotEdges) {
        if (edge.epsilon) {
            // ε 边：虚线 + ε 标签。注意这里**不**做 escapeForDot（见 epsilonDotLabel）。
            dot += QStringLiteral("  q%1 -> q%2 [label=\"%3\", style=dashed];\n")
                       .arg(edge.from)
                       .arg(edge.to)
                       .arg(epsilonDotLabel());
            continue;
        }
        if (edge.labels.isEmpty())
            continue;
        dot += QStringLiteral("  q%1 -> q%2 [label=\"%3\"];\n")
                   .arg(edge.from)
                   .arg(edge.to)
                   .arg(FileUtil::escapeForDot(edge.labels.join(QStringLiteral(","))));
    }
    dot += QStringLiteral("}\n");
    return dot;
}

// ---------------------------------------------------------------------------
// 导出
// ---------------------------------------------------------------------------

/**
 * 功能：导出 Graphviz DOT 文件。
 * 输入：无（弹出保存对话框）。
 * 输出：无。
 * 注意：DOT 文本已随数据填充生成，此处只负责落盘。
 */
void StateTableView::exportDot()
{
    if (m_dotText.isEmpty()) {
        QMessageBox::information(this, QStringLiteral("无数据"),
                                 QStringLiteral("当前没有可导出的自动机数据。"));
        return;
    }
    FileUtil::ensureDir(FileUtil::outputDir());
    const QString path = QFileDialog::getSaveFileName(
        this, QStringLiteral("导出 Graphviz DOT 文件"),
        FileUtil::outputDir() + QStringLiteral("/automaton.dot"),
        QStringLiteral("DOT 文件 (*.dot);;所有文件 (*.*)"));
    if (path.isEmpty())
        return;

    if (!FileUtil::writeText(path, m_dotText, false)) {
        QMessageBox::warning(this, QStringLiteral("导出失败"),
                             QStringLiteral("无法写入文件：\n%1").arg(FileUtil::toNativePath(path)));
        return;
    }
    m_lastDotPath = path;
    m_hintLabel->setText(QStringLiteral("已导出 DOT：%1").arg(FileUtil::toNativePath(path)));
    LOG_INFO(QStringLiteral("导出自动机 DOT 文件：%1").arg(path));
}

/**
 * 功能：调用 Graphviz 把 DOT 渲染为 PNG。
 * 输入：无（弹出保存对话框）。
 * 输出：无。
 * 注意：
 *  1. 若尚未导出 DOT，会先提示用户导出；
 *  2. 未安装 Graphviz（找不到 dot.exe）时给出明确提示而不报错退出；
 *  3. dot.exe 路径优先取环境变量 GRAPHVIZ_DOT，其次在 PATH 中查找。
 */
void StateTableView::exportPng()
{
    if (m_lastDotPath.isEmpty()) {
        QMessageBox::information(
            this, QStringLiteral("请先导出 DOT"),
            QStringLiteral("请先用“导出 DOT 文件…”保存 DOT 文件，再渲染为 PNG。\n"
                           "（需要系统已安装 Graphviz）"));
        return;
    }

    const QString path = QFileDialog::getSaveFileName(
        this, QStringLiteral("导出 PNG 图片"),
        FileUtil::outputDir() + QStringLiteral("/automaton.png"),
        QStringLiteral("PNG 图片 (*.png);;所有文件 (*.*)"));
    if (path.isEmpty())
        return;

    const QString dotExe = qEnvironmentVariable("GRAPHVIZ_DOT", QStringLiteral("dot"));
    QProcess process;
    process.setProgram(dotExe);
    process.setArguments({QStringLiteral("-Tpng"), m_lastDotPath, QStringLiteral("-o"), path});
    process.start();
    if (!process.waitForStarted(3000)) {
        QMessageBox::warning(
            this, QStringLiteral("未找到 Graphviz"),
            QStringLiteral("无法启动 “%1”。\n请安装 Graphviz 并加入 PATH，"
                           "或设置环境变量 GRAPHVIZ_DOT 指向 dot.exe。\n"
                           "（DOT 文件仍然可用，可在其他工具中打开）")
                .arg(dotExe));
        LOG_WARN(QStringLiteral("未找到 Graphviz 的 dot 可执行文件：%1").arg(dotExe));
        return;
    }
    process.waitForFinished(30000);
    if (process.exitStatus() != QProcess::NormalExit || process.exitCode() != 0) {
        QMessageBox::warning(this, QStringLiteral("渲染失败"),
                             QStringLiteral("Graphviz 返回错误：\n%1")
                                 .arg(QString::fromLocal8Bit(process.readAllStandardError())));
        return;
    }
    m_hintLabel->setText(QStringLiteral("已导出 PNG：%1").arg(FileUtil::toNativePath(path)));
    LOG_INFO(QStringLiteral("导出自动机 PNG 图片：%1").arg(path));
}

}  // namespace ccd
