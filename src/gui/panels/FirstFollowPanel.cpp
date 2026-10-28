/******************************************************************************
 * (1) 版权信息 : 编译原理课程设计 - 华南师范大学计算机学院
 * (2) 文件名称 : FirstFollowPanel.cpp
 *     标识符   : FirstFollowPanel
 *     摘要     : "⑨ First/Follow"功能面板的实现（任务书要求 2.3）。
 * (3) 版本号   : 1.0.0       作者/修改者 : <学号 姓名>   完成日期 : 2026-XX-XX
 * (4) 版本历史 : 1.0.0  2026-XX-XX  首次实现（阶段 4 界面）
 *****************************************************************************/

#include "gui/panels/FirstFollowPanel.h"

#include <QGroupBox>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QLineEdit>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QRegularExpression>
#include <QSplitter>
#include <QTableWidget>
#include <QTableWidgetItem>
#include <QVBoxLayout>

namespace ccd {

namespace {

/** @brief 结果表的列号，避免散落的魔法数字 */
enum Column {
    ColNonterminal = 0,
    ColNullable = 1,
    ColFirst = 2,
    ColFollow = 3,
    ColumnCount = 4
};

}  // namespace

/** 功能：构造面板。输入：parent。输出：无。 */
FirstFollowPanel::FirstFollowPanel(QWidget *parent)
    : QWidget(parent)
{
    buildUi();
}

/**
 * 功能：构建界面控件与布局。
 * 输入：无。输出：无。
 * 注意：查询框接受"以空格或逗号分隔的符号串"，而不是单个非终结符 ——
 *       LR(1) 闭包要用的是 FIRST(βa) 这种**序列**的 First 集，
 *       把这个能力暴露到界面上，正好能直观演示 LR(1) 向前看符号是怎么算出来的。
 */
void FirstFollowPanel::buildUi()
{
    auto *root = new QVBoxLayout(this);

    // ---- 顶部 ----
    auto *top = new QHBoxLayout();
    m_recomputeButton = new QPushButton(QStringLiteral("重新计算"), this);
    top->addWidget(m_recomputeButton);

    m_summaryLabel = new QLabel(this);
    m_summaryLabel->setWordWrap(true);
    top->addWidget(m_summaryLabel, 1);
    root->addLayout(top);

    // ---- 中部：结果表 + 推导过程 ----
    auto *splitter = new QSplitter(Qt::Vertical, this);

    m_table = new QTableWidget(splitter);
    m_table->setColumnCount(ColumnCount);
    m_table->setHorizontalHeaderLabels({QStringLiteral("非终结符"), QStringLiteral("是否可空"),
                                        QStringLiteral("First 集"), QStringLiteral("Follow 集")});
    m_table->horizontalHeader()->setSectionResizeMode(ColNonterminal,
                                                      QHeaderView::ResizeToContents);
    m_table->horizontalHeader()->setSectionResizeMode(ColNullable,
                                                      QHeaderView::ResizeToContents);
    m_table->horizontalHeader()->setSectionResizeMode(ColFirst, QHeaderView::Stretch);
    m_table->horizontalHeader()->setSectionResizeMode(ColFollow, QHeaderView::Stretch);
    m_table->verticalHeader()->setVisible(false);
    m_table->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_table->setSelectionBehavior(QAbstractItemView::SelectRows);
    splitter->addWidget(m_table);

    auto *stepsBox = new QGroupBox(QStringLiteral("推导过程"), splitter);
    auto *stepsLayout = new QVBoxLayout(stepsBox);
    m_stepsView = new QPlainTextEdit(stepsBox);
    m_stepsView->setReadOnly(true);
    stepsLayout->addWidget(m_stepsView);
    splitter->addWidget(stepsBox);
    splitter->setStretchFactor(0, 3);
    splitter->setStretchFactor(1, 2);
    root->addWidget(splitter, 1);

    // ---- 底部：符号串 First 查询 ----
    auto *queryBox = new QGroupBox(QStringLiteral("符号串 First 查询（用于理解 LR(1) 的向前看符号）"),
                                   this);
    auto *queryLayout = new QHBoxLayout(queryBox);
    queryLayout->addWidget(new QLabel(QStringLiteral("符号串："), queryBox));

    m_queryEdit = new QLineEdit(queryBox);
    m_queryEdit->setPlaceholderText(
        QStringLiteral("以空格或逗号分隔，例如：B $   或   comparison-op simple-exp"));
    queryLayout->addWidget(m_queryEdit, 1);

    m_queryButton = new QPushButton(QStringLiteral("查询"), queryBox);
    queryLayout->addWidget(m_queryButton);

    root->addWidget(queryBox);

    m_queryResult = new QLabel(this);
    m_queryResult->setWordWrap(true);
    m_queryResult->setTextInteractionFlags(Qt::TextSelectableByMouse);
    root->addWidget(m_queryResult);

    connect(m_recomputeButton, &QPushButton::clicked, this,
            &FirstFollowPanel::onRecomputeRequested);
    connect(m_queryButton, &QPushButton::clicked, this, &FirstFollowPanel::onQueryRequested);
    connect(m_queryEdit, &QLineEdit::returnPressed, this, &FirstFollowPanel::onQueryRequested);
}

/**
 * 功能：注入文法并立即计算。
 * 输入：grammar。
 * 输出：无。
 * 注意：内部先复制再增广 —— 增广是计算 Follow(开始符号) 必须的（`$` 来自
 *       增广产生式 S' → S），但调用方持有的文法不应该被本面板改掉。
 */
void FirstFollowPanel::setGrammar(const Grammar &grammar)
{
    m_grammar = grammar;
    recompute();
}

/** 功能：执行计算并刷新显示。输入：无。输出：无。 */
void FirstFollowPanel::recompute()
{
    m_valid = false;
    m_result = FirstFollowResult();

    if (m_grammar.productionCount() == 0) {
        clearAll();
        m_summaryLabel->setText(QStringLiteral("文法为空，无法计算 First/Follow。"));
        return;
    }

    Grammar work = m_grammar;
    work.setTerminalNames(m_grammar.terminalNames());
    work.augment();

    FirstFollow calculator;
    m_result = calculator.compute(work);
    m_valid = true;

    refreshTable();
    m_stepsView->setPlainText(m_result.steps.join(QLatin1Char('\n')));
    m_summaryLabel->setText(m_result.summary());

    emit firstFollowReady();
}

/**
 * 功能：刷新结果表格。
 * 输入：无。输出：无。
 * 注意：表格的行对应**未增广**文法的非终结符 —— 增广符号 S' 是构造过程
 *       的内部产物，列在 First/Follow 表里会让使用者困惑（它的 First/Follow
 *       没有教学意义）。因此这里用 m_grammar.nonterminals() 而不是
 *       m_result.grammar.nonterminals()。
 */
void FirstFollowPanel::refreshTable()
{
    const QStringList nonterminals = m_grammar.nonterminals();
    m_table->setRowCount(nonterminals.size());

    for (int i = 0; i < nonterminals.size(); ++i) {
        const QString &nt = nonterminals.at(i);

        const QStringList cells = {
            nt,
            m_result.nullable.contains(nt) ? QStringLiteral("是") : QStringLiteral("否"),
            FirstFollowResult::formatSet(firstOf(nt)),
            FirstFollowResult::formatSet(followOf(nt))};

        for (int c = 0; c < cells.size(); ++c)
            m_table->setItem(i, c, new QTableWidgetItem(cells.at(c)));
    }
}

/** 功能：按名字取 First 集。输入：nonterminal。输出：集合。 */
QSet<QString> FirstFollowPanel::firstOf(const QString &nonterminal) const
{
    return m_result.first.value(nonterminal);
}

/** 功能：按名字取 Follow 集。输入：nonterminal。输出：集合。 */
QSet<QString> FirstFollowPanel::followOf(const QString &nonterminal) const
{
    return m_result.follow.value(nonterminal);
}

/** 功能：重新计算按钮。输入：无。输出：无。 */
void FirstFollowPanel::onRecomputeRequested()
{
    recompute();
}

/**
 * 功能：执行一次符号串 First 查询。
 * 输入：无。输出：无。
 * 注意：分隔符同时接受半角空格、逗号与中文顿号 —— 用户从表格里复制集合时
 *       拿到的是带"、"的文本，若只认空格会让人以为查询坏了。
 */
void FirstFollowPanel::onQueryRequested()
{
    if (!m_valid) {
        m_queryResult->setText(QStringLiteral("请先载入并计算一个文法。"));
        return;
    }

    const QString raw = m_queryEdit->text().trimmed();
    if (raw.isEmpty()) {
        m_queryResult->setText(QStringLiteral("请输入要查询的符号串。"));
        return;
    }

    QString normalized = raw;
    normalized.replace(QLatin1Char(','), QLatin1Char(' '));
    normalized.replace(QStringLiteral("、"), QStringLiteral(" "));
    normalized.replace(QStringLiteral("{"), QStringLiteral(" "));
    normalized.replace(QStringLiteral("}"), QStringLiteral(" "));

    const QStringList sequence =
        normalized.split(QRegularExpression(QStringLiteral("\\s+")), Qt::SkipEmptyParts);

    if (sequence.isEmpty()) {
        m_queryResult->setText(QStringLiteral("未能从输入中解析出任何符号。"));
        return;
    }

    const QSet<QString> firstSet = m_result.firstSetOfSequence(sequence);
    const bool nullable = m_result.sequenceNullable(sequence);

    m_queryResult->setText(
        QStringLiteral("符号串 [ %1 ]　→　First = %2　｜　是否可空：%3\n"
                       "（可空表示该符号串能推出空串；此时按定义还要继续看它后面的符号）")
            .arg(sequence.join(QLatin1Char(' ')),
                 FirstFollowResult::formatSet(firstSet),
                 nullable ? QStringLiteral("是") : QStringLiteral("否")));
}

/** 功能：清空全部显示。输入：无。输出：无。 */
void FirstFollowPanel::clearAll()
{
    m_valid = false;
    m_table->setRowCount(0);
    m_stepsView->clear();
    m_summaryLabel->clear();
    m_queryResult->clear();
}

/** 功能：结果表行数（自测用）。输入：无。输出：行数。 */
int FirstFollowPanel::tableRowCount() const
{
    return m_table->rowCount();
}

/** 功能：摘要标签文本（自测用）。输入：无。输出：文本。 */
QString FirstFollowPanel::summaryText() const
{
    return m_summaryLabel->text();
}

/** 功能：推导过程文本（自测用）。输入：无。输出：文本。 */
QString FirstFollowPanel::stepsText() const
{
    return m_stepsView->toPlainText();
}

/** 功能：某非终结符的 First 集显示文本（自测用）。输入：nonterminal。输出：文本。 */
QString FirstFollowPanel::firstSetText(const QString &nonterminal) const
{
    return FirstFollowResult::formatSet(firstOf(nonterminal));
}

/** 功能：某非终结符的 Follow 集显示文本（自测用）。输入：nonterminal。输出：文本。 */
QString FirstFollowPanel::followSetText(const QString &nonterminal) const
{
    return FirstFollowResult::formatSet(followOf(nonterminal));
}

}  // namespace ccd
