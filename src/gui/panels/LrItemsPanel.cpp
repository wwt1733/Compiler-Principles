/******************************************************************************
 * (1) 版权信息 : 编译原理课程设计 - 华南师范大学计算机学院
 * (2) 文件名称 : LrItemsPanel.cpp
 *     标识符   : LrItemsPanel
 *     摘要     : "⑩ LR(0) DFA / ⑪ LALR(1) 项目集"功能面板的实现（要求 2.4）。
 * (3) 版本号   : 1.0.0       作者/修改者 : <学号 姓名>   完成日期 : 2026-XX-XX
 * (4) 版本历史 : 1.0.0  2026-XX-XX  首次实现（阶段 5 界面）
 *****************************************************************************/

#include "gui/panels/LrItemsPanel.h"

#include <QComboBox>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QSplitter>
#include <QTableWidget>
#include <QTableWidgetItem>
#include <QVBoxLayout>

namespace ccd {

namespace {

/** 模式索引：与下拉框条目顺序严格一致 */
enum ModeIndex {
    ModeLr0 = 0,   ///< LR(0) 项目集规范族
    ModeLalr1 = 1  ///< LALR(1) 项目集（LR(1) 同心集合并）
};

}  // namespace

/** 功能：构造面板。输入：parent。输出：无。 */
LrItemsPanel::LrItemsPanel(QWidget *parent)
    : QWidget(parent)
{
    buildUi();
}

/**
 * 功能：构建界面控件与布局。
 * 输入：无。输出：无。
 * 注意：布局用 QSplitter 让用户能自由调整"状态列表 / 项目集 / 构造过程"的比例 ——
 *       项目集文本可能很长，固定宽度会难以阅读。
 */
void LrItemsPanel::buildUi()
{
    auto *root = new QVBoxLayout(this);

    // ---- 顶部控制条 ----
    auto *top = new QHBoxLayout();
    top->addWidget(new QLabel(QStringLiteral("项目集类型："), this));

    m_modeCombo = new QComboBox(this);
    m_modeCombo->addItem(QStringLiteral("LR(0) 项目集规范族"));
    m_modeCombo->addItem(QStringLiteral("LALR(1) 项目集（LR(1) 同心集合并）"));
    m_modeCombo->setCurrentIndex(ModeLr0);
    top->addWidget(m_modeCombo);

    m_rebuildButton = new QPushButton(QStringLiteral("重新构造"), this);
    top->addWidget(m_rebuildButton);

    m_summaryLabel = new QLabel(this);
    m_summaryLabel->setWordWrap(true);
    top->addWidget(m_summaryLabel, 1);

    root->addLayout(top);

    // ---- 中部：左状态表、右项目集 ----
    auto *splitter = new QSplitter(Qt::Horizontal, this);

    m_stateTable = new QTableWidget(splitter);
    m_stateTable->setColumnCount(4);
    m_stateTable->setHorizontalHeaderLabels(
        {QStringLiteral("状态"), QStringLiteral("项目数"), QStringLiteral("接受态"),
         QStringLiteral("转移")});
    m_stateTable->horizontalHeader()->setStretchLastSection(true);
    m_stateTable->verticalHeader()->setVisible(false);
    m_stateTable->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_stateTable->setSelectionMode(QAbstractItemView::SingleSelection);
    m_stateTable->setEditTriggers(QAbstractItemView::NoEditTriggers);
    splitter->addWidget(m_stateTable);

    m_itemsView = new QPlainTextEdit(splitter);
    m_itemsView->setReadOnly(true);
    m_itemsView->setLineWrapMode(QPlainTextEdit::NoWrap);
    m_itemsView->setPlaceholderText(
        QStringLiteral("在上表中选择一个状态，这里显示它的项目集。\n"
                       "LALR(1) 模式下每个项目后面附带向前看符号集合。"));
    splitter->addWidget(m_itemsView);

    splitter->setStretchFactor(0, 1);
    splitter->setStretchFactor(1, 2);
    root->addWidget(splitter, 1);

    // ---- 底部：构造过程 ----
    auto *stepsBox = new QGroupBox(QStringLiteral("构造过程"), this);
    auto *stepsLayout = new QVBoxLayout(stepsBox);
    m_stepsView = new QPlainTextEdit(stepsBox);
    m_stepsView->setReadOnly(true);
    m_stepsView->setMaximumHeight(150);
    stepsLayout->addWidget(m_stepsView);
    root->addWidget(stepsBox);

    connect(m_modeCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), this,
            &LrItemsPanel::onModeChanged);
    connect(m_rebuildButton, &QPushButton::clicked, this, [this] {
        if (!m_grammar.productionCount())
            return;
        setGrammar(m_grammar);  // 重新走一遍完整构造
    });
    connect(m_stateTable, &QTableWidget::itemSelectionChanged, this,
            &LrItemsPanel::onStateSelectionChanged);
}

/**
 * 功能：注入文法并重新构造 LR(0) 与 LALR(1) 项目集。
 * 输入：grammar —— 文法。
 * 输出：无。
 * 注意：
 *  1. 本方法**不修改**传入的文法：内部复制一份再增广。这一点很重要 ——
 *     调用方（MainWindow）还要把同一份文法交给分析表面板，
 *     若在此处就地增广，调用方看到的文法会多出 S' 而产生歧义；
 *  2. LR(0) 与 LALR(1) 都算一遍并缓存，切换模式时不再重算；
 *  3. 化简在 MainWindow 已完成，这里只做项目集构造。
 */
void LrItemsPanel::setGrammar(const Grammar &grammar)
{
    m_grammar = grammar;
    m_valid = false;
    m_lr0 = Lr0Automaton();
    m_lalr = LalrAutomaton();
    m_firstFollow = FirstFollowResult();

    if (grammar.productionCount() == 0) {
        clearAll();
        m_summaryLabel->setText(QStringLiteral("文法为空，无法构造项目集。"));
        return;
    }

    // LR(0)：build() 内部自行增广且不修改入参，直接调用即可
    LrItems lr0Builder;
    m_lr0 = lr0Builder.build(grammar);

    // LALR(1)：LR(1) 闭包要用 FIRST(βa)，因此先算 First/Follow。
    // 注意 First/Follow 与 LR(1) 构造都必须作用在**已增广**的文法上
    // （初态是 closure({[S' -> ·S, $]})，没有 S' 就找不到初态），
    // 这里用副本 work 增广，避免污染 m_grammar 与调用方的文法。
    Grammar work = grammar;
    work.setTerminalNames(grammar.terminalNames());
    work.augment();

    FirstFollow ff;
    m_firstFollow = ff.compute(work);

    Lr1Items lr1Builder;
    m_lalr = lr1Builder.compute(work, m_firstFollow);

    const QString lr0Error = lr0Builder.error();
    const QString lr1Error = lr1Builder.error();
    if (!lr0Error.isEmpty() || !lr1Error.isEmpty()) {
        m_summaryLabel->setText(QStringLiteral("构造失败：%1 %2")
                                    .arg(lr0Error, lr1Error)
                                    .trimmed());
        return;
    }

    m_valid = true;
    refreshStateTable();
    updateSummary();

    // 构造过程：两种模式的说明都展示出来，便于报告里叙述
    QStringList steps;
    steps << QStringLiteral("【LR(0) 项目集规范族】");
    steps << m_lr0.steps;
    steps << QString();
    steps << QStringLiteral("【LALR(1) 项目集】");
    steps << m_lalr.steps;
    m_stepsView->setPlainText(steps.join(QLatin1Char('\n')));

    emit lrAutomatonReady();
}

/** 功能：清空全部显示。输入：无。输出：无。 */
void LrItemsPanel::clearAll()
{
    m_valid = false;
    m_stateTable->setRowCount(0);
    m_itemsView->clear();
    m_stepsView->clear();
    m_summaryLabel->clear();
}

/** 功能：当前是否 LALR(1) 模式。输入：无。输出：是/否。 */
bool LrItemsPanel::isLalrMode() const
{
    return m_modeCombo->currentIndex() == ModeLalr1;
}

/** 功能：取当前模式的状态数。输入：无。输出：状态数。 */
int LrItemsPanel::activeStateCount() const
{
    if (!m_valid)
        return 0;
    return isLalrMode() ? m_lalr.states.size() : m_lr0.states.size();
}

/** 功能：当前模式名。输入：无。输出："LR(0)" 或 "LALR(1)"。 */
QString LrItemsPanel::modeName() const
{
    return isLalrMode() ? QStringLiteral("LALR(1)") : QStringLiteral("LR(0)");
}

/**
 * 功能：设置模式并按该模式重建。
 * 输入：index —— 0 = LR(0)、1 = LALR(1)。
 * 输出：无。
 * 注意：下拉框的 currentIndexChanged 已连到 onModeChanged（内部会重建），
 *       所以这里只改下拉框；若目标下标恰好等于当前值、信号不发，则显式调一次
 *       onModeChanged 兜底（与 initialize() 载入语言数据时同样处理）。
 */
void LrItemsPanel::setModeIndex(int index)
{
    if (m_modeCombo == nullptr)
        return;
    const int target = qBound(0, index, m_modeCombo->count() - 1);
    if (m_modeCombo->currentIndex() == target) {
        onModeChanged(target);
        return;
    }
    m_modeCombo->setCurrentIndex(target);
}

/**
 * 功能：取当前模式下某状态的项目集显示文本。
 * 输入：state —— 状态号。
 * 输出：一行一个项目的文本；越界返回空串。
 * 注意：LALR(1) 模式下把向前看符号附在每个项目后面，
 *       格式为 `左部 -> 右部 , { 符号、符号 }`，与讲稿和报告的写法一致。
 */
QString LrItemsPanel::activeStateItems(int state) const
{
    if (!m_valid || state < 0)
        return QString();

    if (!isLalrMode()) {
        return m_lr0.stateItemsText(state);
    }

    if (state >= m_lalr.states.size())
        return QString();

    const LalrState &st = m_lalr.states.at(state);
    const Grammar &g = m_lalr.grammar;

    QStringList lines;
    for (int i = 0; i < st.core.size(); ++i) {
        const LrItem &item = st.core.at(i);
        const Production &p = g.production(item.prodId);

        QStringList parts;
        for (int k = 0; k < p.right.size(); ++k) {
            if (k == item.dot)
                parts << QStringLiteral("·");
            parts << p.right.at(k);
        }
        if (item.dot >= p.right.size())
            parts << QStringLiteral("·");

        const QSet<QString> la = i < st.lookahead.size() ? st.lookahead.at(i) : QSet<QString>();
        lines << QStringLiteral("%1 -> %2 , %3")
                     .arg(p.left, parts.join(QLatin1Char(' ')), FirstFollowResult::formatSet(la));
    }

    if (st.isAccepting(g)) {
        lines << QString();
        lines << QStringLiteral("※ 该状态为接受状态（含 %1 -> %2 ·）")
                     .arg(g.augmentedStart(), g.startSymbol());
    }

    return lines.join(QLatin1Char('\n'));
}

/**
 * 功能：用当前模式的结果刷新状态表。
 * 输入：无。输出：无。
 * 注意：刷新前先断开选中信号再恢复的方式容易出错，这里改为：
 *       填充数据时不关心选中变化（onStateSelectionChanged 会因表格为空而安全返回），
 *       填完后再显式 selectState(0)。
 */
void LrItemsPanel::refreshStateTable()
{
    const int count = activeStateCount();
    m_stateTable->setRowCount(count);

    for (int s = 0; s < count; ++s) {
        QStringList transitions;
        int itemCount = 0;
        bool accepting = false;

        if (isLalrMode()) {
            const LalrState &st = m_lalr.states.at(s);
            itemCount = st.core.size();
            accepting = st.isAccepting(m_lalr.grammar);
            for (auto it = st.transitions.constBegin(); it != st.transitions.constEnd(); ++it)
                transitions << QStringLiteral("%1→I%2").arg(it.key()).arg(it.value());
        } else {
            const Lr0State &st = m_lr0.states.at(s);
            itemCount = st.items.size();
            accepting = st.isAccepting(m_lr0.grammar);
            for (auto it = st.transitions.constBegin(); it != st.transitions.constEnd(); ++it)
                transitions << QStringLiteral("%1→I%2").arg(it.key()).arg(it.value());
        }

        const QStringList cells = {QStringLiteral("I%1").arg(s),
                                   QString::number(itemCount),
                                   accepting ? QStringLiteral("是") : QString(),
                                   transitions.join(QStringLiteral("  "))};
        for (int c = 0; c < cells.size(); ++c) {
            m_stateTable->setItem(s, c, new QTableWidgetItem(cells.at(c)));
        }
    }

    m_stateTable->resizeColumnsToContents();
    if (count > 0)
        selectState(0);
    else
        m_itemsView->clear();
}

/**
 * 功能：刷新摘要标签。
 * 输入：无。输出：无。
 * 注意：LALR(1) 模式下必须把"LR(1) 规范族状态数 → 合并后状态数 → 合并组数"
 *       三个数一起显示。这是本项目相对参考项目（只做 SLR(1)）的加分点，
 *       实验报告里要引用这组数据说明同心集合并的效果，界面上不能只给一个总数。
 */
void LrItemsPanel::updateSummary()
{
    if (!m_valid) {
        m_summaryLabel->clear();
        return;
    }

    if (isLalrMode()) {
        m_summaryLabel->setText(
            QStringLiteral("%1　｜　LR(1) 规范族 %2 个状态 → 合并同核心后 %3 个状态"
                           "（%4 个核心组发生合并）")
                .arg(m_lalr.summary())
                .arg(m_lalr.lr1StateCount)
                .arg(m_lalr.states.size())
                .arg(m_lalr.mergedGroupCount));
    } else {
        m_summaryLabel->setText(m_lr0.summary());
    }
}

/** 功能：模式切换。输入：index。输出：无。 */
void LrItemsPanel::onModeChanged(int index)
{
    Q_UNUSED(index);
    if (!m_valid)
        return;
    refreshStateTable();
    updateSummary();
}

/** 功能：表格选中行变化。输入：无。输出：无。 */
void LrItemsPanel::onStateSelectionChanged()
{
    const int state = currentState();
    if (state < 0) {
        m_itemsView->clear();
        return;
    }

    const QString text = activeStateItems(state);
    const QString header = QStringLiteral("状态 I%1（%2）\n%3\n\n")
                               .arg(state)
                               .arg(modeName())
                               .arg(QString(60, QLatin1Char('-')));
    m_itemsView->setPlainText(header + text);
}

/** 功能：当前选中状态号。输入：无。输出：状态号（无选中为 -1）。 */
int LrItemsPanel::currentState() const
{
    const int row = m_stateTable->currentRow();
    if (row < 0 || row >= m_stateTable->rowCount())
        return -1;
    return row;
}

/** 功能：选择指定状态。输入：state。输出：无。 */
void LrItemsPanel::selectState(int state)
{
    if (state < 0 || state >= m_stateTable->rowCount())
        return;
    m_stateTable->selectRow(state);
    m_stateTable->setCurrentCell(state, 0);
    onStateSelectionChanged();
}

/** 功能：当前项目集文本（自测用）。输入：无。输出：文本。 */
QString LrItemsPanel::itemsText() const
{
    return m_itemsView->toPlainText();
}

/** 功能：状态表行数（自测用）。输入：无。输出：行数。 */
int LrItemsPanel::stateRowCount() const
{
    return m_stateTable->rowCount();
}

/** 功能：摘要标签文本（自测用）。输入：无。输出：文本。 */
QString LrItemsPanel::summaryText() const
{
    return m_summaryLabel->text();
}

}  // namespace ccd
