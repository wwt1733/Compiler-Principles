/******************************************************************************
 * (1) 版权信息 : 编译原理课程设计 - 华南师范大学计算机学院
 * (2) 文件名称 : ParseTablePanel.cpp
 *     标识符   : ParseTablePanel
 *     摘要     : "⑫ LALR(1) 分析表"功能面板的实现（任务书要求 2.5）。
 * (3) 版本号   : 1.0.0       作者/修改者 : <学号 姓名>   完成日期 : 2026-XX-XX
 * (4) 版本历史 : 1.0.0  2026-XX-XX  首次实现（阶段 5 界面）
 *****************************************************************************/

#include "gui/panels/ParseTablePanel.h"

#include <QCheckBox>
#include <QColor>
#include <QComboBox>
#include <QFileDialog>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QMessageBox>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QSplitter>
#include <QTabWidget>
#include <QTableWidget>
#include <QTableWidgetItem>
#include <QVBoxLayout>

#include "common/FileUtil.h"

namespace ccd {

namespace {

/** 表种类下拉框索引：与条目顺序严格一致 */
enum KindIndex {
    KindLalr1 = 0,
    KindSlr1 = 1,
    KindLr0 = 2
};

/** @brief 把下拉框索引转为表种类枚举 */
ParseTableKind kindOfIndex(int index)
{
    switch (index) {
    case KindSlr1:
        return ParseTableKind::Slr1;
    case KindLr0:
        return ParseTableKind::Lr0;
    case KindLalr1:
    default:
        return ParseTableKind::Lalr1;
    }
}

/** @brief 把表种类枚举转为下拉框索引 */
int indexOfKind(ParseTableKind kind)
{
    switch (kind) {
    case ParseTableKind::Slr1:
        return KindSlr1;
    case ParseTableKind::Lr0:
        return KindLr0;
    case ParseTableKind::Lalr1:
    default:
        return KindLalr1;
    }
}

/** @brief 动作类型的着色方案（浅色底，保证黑字可读） */
QColor colorOfAction(ActionType type)
{
    switch (type) {
    case ActionType::Shift:
        return QColor(219, 234, 254);  // 浅蓝：移进
    case ActionType::Reduce:
        return QColor(220, 252, 231);  // 浅绿：规约
    case ActionType::Accept:
        return QColor(254, 249, 195);  // 浅黄：接受
    case ActionType::Error:
    default:
        return QColor(Qt::white);
    }
}

}  // namespace

/** 功能：构造面板。输入：parent。输出：无。 */
ParseTablePanel::ParseTablePanel(QWidget *parent)
    : QWidget(parent)
{
    buildUi();
}

/**
 * 功能：构建界面控件与布局。
 * 输入：无。输出：无。
 * 注意：ACTION 表与 GOTO 表放在**同一个** QTableWidget 里（终结符列在前、
 *       非终结符列在后），这样一行就是一个状态的全部动作，查看时不必左右对照 ——
 *       分析器运行时也是"同一个状态同时查 ACTION 与 GOTO"。
 */
void ParseTablePanel::buildUi()
{
    auto *root = new QVBoxLayout(this);

    // ---- 顶部控制条 ----
    auto *top = new QHBoxLayout();
    top->addWidget(new QLabel(QStringLiteral("分析表种类："), this));

    m_kindCombo = new QComboBox(this);
    m_kindCombo->addItem(QStringLiteral("LALR(1)"));
    m_kindCombo->addItem(QStringLiteral("SLR(1)（对比用）"));
    m_kindCombo->addItem(QStringLiteral("LR(0)（对比用）"));
    m_kindCombo->setCurrentIndex(KindLalr1);
    top->addWidget(m_kindCombo);

    m_shiftWinsBox = new QCheckBox(QStringLiteral("移进优先"), this);
    m_shiftWinsBox->setChecked(true);
    m_shiftWinsBox->setToolTip(
        QStringLiteral("消解移进-规约冲突：保留移进（与 yacc/bison 默认一致，"
                       "可自动处理悬空 else）"));
    top->addWidget(m_shiftWinsBox);

    m_lowerProdBox = new QCheckBox(QStringLiteral("产生式序号小者优先"), this);
    m_lowerProdBox->setChecked(true);
    m_lowerProdBox->setToolTip(
        QStringLiteral("消解规约-规约冲突：保留序号较小的产生式，"
                       "保证结果确定且可复现"));
    top->addWidget(m_lowerProdBox);

    m_rebuildButton = new QPushButton(QStringLiteral("重新构造"), this);
    top->addWidget(m_rebuildButton);

    m_exportButton = new QPushButton(QStringLiteral("导出分析表"), this);
    top->addWidget(m_exportButton);

    root->addLayout(top);

    m_summaryLabel = new QLabel(this);
    m_summaryLabel->setWordWrap(true);
    root->addWidget(m_summaryLabel);

    // ---- 中部：分析表本体 ----
    // 用 QTabWidget 让"表 / 冲突 / 过程"三块各自独立，避免同屏争空间
    auto *tabs = new QTabWidget(this);

    m_tableWidget = new QTableWidget(tabs);
    m_tableWidget->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_tableWidget->setSelectionMode(QAbstractItemView::SingleSelection);
    tabs->addTab(m_tableWidget, QStringLiteral("ACTION / GOTO 分析表"));

    m_conflictView = new QPlainTextEdit(tabs);
    m_conflictView->setReadOnly(true);
    tabs->addTab(m_conflictView, QStringLiteral("冲突报告"));

    m_stepsView = new QPlainTextEdit(tabs);
    m_stepsView->setReadOnly(true);
    tabs->addTab(m_stepsView, QStringLiteral("构造过程"));

    root->addWidget(tabs, 1);

    connect(m_kindCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), this,
            &ParseTablePanel::onKindChanged);
    connect(m_shiftWinsBox, &QCheckBox::toggled, this, &ParseTablePanel::onPolicyChanged);
    connect(m_lowerProdBox, &QCheckBox::toggled, this, &ParseTablePanel::onPolicyChanged);
    connect(m_rebuildButton, &QPushButton::clicked, this, &ParseTablePanel::onRebuildRequested);
    connect(m_exportButton, &QPushButton::clicked, this, &ParseTablePanel::onExportRequested);
}

/** 功能：注入文法并构造分析表。输入：grammar。输出：无。 */
void ParseTablePanel::setGrammar(const Grammar &grammar)
{
    m_grammar = grammar;
    m_valid = false;
    m_table = ParseTable();

    if (grammar.productionCount() == 0) {
        clearAll();
        m_summaryLabel->setText(QStringLiteral("文法为空，无法构造分析表。"));
        return;
    }

    rebuild();
}

/** 功能：取当前冲突消解策略。输入：无。输出：策略。 */
ParseTableBuilder::ConflictPolicy ParseTablePanel::currentPolicy() const
{
    ParseTableBuilder::ConflictPolicy policy;
    policy.shiftWinsOverReduce = m_shiftWinsBox->isChecked();
    policy.lowerProductionWins = m_lowerProdBox->isChecked();
    return policy;
}

/**
 * 功能：执行一次构造并刷新全部显示。
 * 输入：无。输出：无。
 * 注意：文法化简与增广都由 ParseTableBuilder 内部完成（顺序为"先化简再增广"），
 *       本类不再重复做，避免两处顺序不一致导致结果差异。
 */
void ParseTablePanel::rebuild()
{
    if (m_grammar.productionCount() == 0)
        return;

    ParseTableBuilder builder;
    builder.setConflictPolicy(currentPolicy());

    m_table = builder.build(m_grammar, kind());

    if (!builder.error().isEmpty()) {
        m_valid = false;
        m_summaryLabel->setText(QStringLiteral("构造失败：%1").arg(builder.error()));
        return;
    }

    m_valid = true;
    refreshTableWidget();

    m_summaryLabel->setText(m_table.summary());
    m_conflictView->setPlainText(m_table.conflictsText());
    m_stepsView->setPlainText(m_table.steps.join(QLatin1Char('\n')));

    emit parseTableReady();
}

/**
 * 功能：按分析表刷新 ACTION/GOTO 表格控件。
 * 输入：无。输出：无。
 * 注意：
 *  1. 列头顺序为"状态 | 全部终结符 | 全部非终结符"，与 ParseTable::toText() 一致；
 *  2. 空表项（Error）显示为空串而不是 "err" —— 表里绝大多数格子是空的，
 *     全填上文字会变成一片噪声，反而看不出动作在哪；
 *  3. 接受动作填在结束符 `$` 列上（`kEndMarker`），这与分析器读入结束符后
 *     查表的行为一致。
 */
void ParseTablePanel::refreshTableWidget()
{
    const QStringList terminals = m_table.terminals();
    const QStringList nonterminals = m_table.nonterminals();
    const int stateCount = m_table.stateCount();

    const int columnCount = 1 + terminals.size() + nonterminals.size();
    m_tableWidget->clear();
    m_tableWidget->setColumnCount(columnCount);
    m_tableWidget->setRowCount(stateCount);

    QStringList headers;
    headers << QStringLiteral("状态");
    for (const QString &t : terminals)
        headers << t;
    for (const QString &n : nonterminals)
        headers << n;
    m_tableWidget->setHorizontalHeaderLabels(headers);

    for (int s = 0; s < stateCount; ++s) {
        const QStringList cells = [&] {
            QStringList row;
            row << QStringLiteral("I%1").arg(s);
            for (const QString &t : terminals) {
                const ParseAction a = m_table.actionAt(s, t);
                row << (a.type == ActionType::Error ? QString() : a.toString());
            }
            for (const QString &n : nonterminals) {
                const int target = m_table.gotoAt(s, n);
                row << (target < 0 ? QString() : QString::number(target));
            }
            return row;
        }();

        for (int c = 0; c < cells.size(); ++c) {
            auto *item = new QTableWidgetItem(cells.at(c));

            // 着色：只有终结符列是动作，按动作类型上色
            if (c >= 1 && c <= terminals.size()) {
                const ParseAction a = m_table.actionAt(s, terminals.at(c - 1));
                item->setBackground(colorOfAction(a.type));
                if (a.type != ActionType::Error)
                    item->setTextAlignment(Qt::AlignCenter);
            }

            m_tableWidget->setItem(s, c, item);
        }
    }

    m_tableWidget->resizeColumnsToContents();

    // 状态数多时行高压缩一点，尽量一屏看到更多行
    if (stateCount > 40)
        m_tableWidget->verticalHeader()->setDefaultSectionSize(20);
}

/** 功能：表种类变化。输入：index。输出：无。 */
void ParseTablePanel::onKindChanged(int index)
{
    Q_UNUSED(index);
    if (m_grammar.productionCount() == 0)
        return;
    rebuild();
}

/** 功能：冲突消解策略变化。输入：无。输出：无。 */
void ParseTablePanel::onPolicyChanged()
{
    if (m_grammar.productionCount() == 0)
        return;
    rebuild();
}

/** 功能：重新构造。输入：无。输出：无。 */
void ParseTablePanel::onRebuildRequested()
{
    if (m_grammar.productionCount() == 0)
        return;
    rebuild();
}

/**
 * 功能：导出分析表为文本文件。
 * 输入：无。输出：无。
 * 注意：导出内容为 ParseTable::toText()（制表符分隔），可直接粘进实验报告。
 *       文件写失败必须弹框告知 —— 静默失败会让人以为"导出成功了但文件是空的"。
 */
void ParseTablePanel::onExportRequested()
{
    if (!m_valid) {
        QMessageBox::information(this, QStringLiteral("导出分析表"),
                                 QStringLiteral("当前还没有可导出的分析表，请先构造。"));
        return;
    }

    const QString suggested =
        QStringLiteral("%1_%2_table.txt")
            .arg(parseTableKindName(kind()).replace(QLatin1Char('('), QLatin1Char('_'))
                     .replace(QLatin1Char(')'), QLatin1Char('_')))
            .arg(m_table.stateCount());
    const QString path = QFileDialog::getSaveFileName(
        this, QStringLiteral("导出分析表"), suggested,
        QStringLiteral("文本文件 (*.txt);;所有文件 (*)"));
    if (path.isEmpty())
        return;

    // 用 UTF-8 无 BOM 写出：分析表是给程序/报告用的，
    // 带 BOM 会让某些解析工具把首字符读成 \ufeff
    if (!FileUtil::writeText(path, m_table.toText(), false)) {
        QMessageBox::warning(this, QStringLiteral("导出分析表"),
                             QStringLiteral("写入失败：%1").arg(path));
        return;
    }

    QMessageBox::information(this, QStringLiteral("导出分析表"),
                             QStringLiteral("已导出到：\n%1").arg(path));
}

/** 功能：设置表种类。输入：kind。输出：无。 */
void ParseTablePanel::setKind(ParseTableKind kind)
{
    const int index = indexOfKind(kind);
    if (m_kindCombo->currentIndex() == index) {
        onKindChanged(index);  // 索引未变则信号不触发，需要手动重建
        return;
    }
    m_kindCombo->setCurrentIndex(index);  // 索引变化会触发 onKindChanged
}

/** 功能：取当前表种类。输入：无。输出：表种类。 */
ParseTableKind ParseTablePanel::kind() const
{
    return kindOfIndex(m_kindCombo->currentIndex());
}

/** 功能：清空全部显示。输入：无。输出：无。 */
void ParseTablePanel::clearAll()
{
    m_valid = false;
    m_tableWidget->clear();
    m_tableWidget->setRowCount(0);
    m_tableWidget->setColumnCount(0);
    m_conflictView->clear();
    m_stepsView->clear();
    m_summaryLabel->clear();
}

/** 功能：分析表表格行数（自测用）。输入：无。输出：行数。 */
int ParseTablePanel::tableRowCount() const
{
    return m_tableWidget->rowCount();
}

/** 功能：分析表表格列数（自测用）。输入：无。输出：列数。 */
int ParseTablePanel::tableColumnCount() const
{
    return m_tableWidget->columnCount();
}

/** 功能：某单元格文本（自测用）。输入：row、column。输出：文本（越界为空串）。 */
QString ParseTablePanel::cellText(int row, int column) const
{
    const QTableWidgetItem *item = m_tableWidget->item(row, column);
    return item == nullptr ? QString() : item->text();
}

/** 功能：摘要标签文本（自测用）。输入：无。输出：文本。 */
QString ParseTablePanel::summaryText() const
{
    return m_summaryLabel->text();
}

/** 功能：冲突明细文本（自测用）。输入：无。输出：文本。 */
QString ParseTablePanel::conflictsText() const
{
    return m_conflictView->toPlainText();
}

}  // namespace ccd
