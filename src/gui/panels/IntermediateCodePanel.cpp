/******************************************************************************
 * (1) 版权信息 : 编译原理课程设计 - 华南师范大学计算机学院
 * (2) 文件名称 : IntermediateCodePanel.cpp
 *     标识符   : IntermediateCodePanel
 *     摘要     : "⑮ 中间代码（选做）"功能面板的实现（任务书要求 2.11）。
 * (3) 版本号   : 1.0.0       作者/修改者 : <学号 姓名>   完成日期 : 2026-XX-XX
 * (4) 版本历史 : 1.0.0  2026-XX-XX  首次实现（阶段 8）
 *****************************************************************************/

#include "gui/panels/IntermediateCodePanel.h"

#include <QCheckBox>
#include <QFontDatabase>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QSplitter>
#include <QTableWidget>
#include <QVBoxLayout>

namespace ccd {

/** 功能：构造面板。输入：parent。输出：无。 */
IntermediateCodePanel::IntermediateCodePanel(QWidget *parent)
    : QWidget(parent)
{
    buildUi();
}

/**
 * 功能：构建界面控件与布局。
 * 输入：无。输出：无。
 * 注意：四元式表用固定列宽 + 等宽字体，因为四元式是"位置敏感"的 ——
 *       同一列上下对齐才容易看出数据流。
 */
void IntermediateCodePanel::buildUi()
{
    auto *root = new QVBoxLayout(this);

    auto *top = new QHBoxLayout;
    m_exportButton = new QPushButton(QStringLiteral("导出四元式…"), this);
    top->addWidget(m_exportButton);

    auto *noteBox = new QCheckBox(QStringLiteral("显示说明列"), this);
    noteBox->setChecked(true);
    top->addWidget(noteBox);
    top->addStretch(1);

    m_summaryLabel = new QLabel(QStringLiteral("（还没有中间代码）"), this);
    m_summaryLabel->setWordWrap(true);
    top->addWidget(m_summaryLabel, 3);
    root->addLayout(top);

    m_quadTable = new QTableWidget(this);
    m_quadTable->setColumnCount(7);
    m_quadTable->setHorizontalHeaderLabels(
        {QStringLiteral("序号"), QStringLiteral("运算符"),
         QStringLiteral("运算对象1"), QStringLiteral("运算对象2"),
         QStringLiteral("结果"), QStringLiteral("源程序行"), QStringLiteral("说明")});
    m_quadTable->horizontalHeader()->setStretchLastSection(true);
    m_quadTable->verticalHeader()->setVisible(false);
    m_quadTable->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_quadTable->setFont(QFontDatabase::systemFont(QFontDatabase::FixedFont));

    m_textView = new QPlainTextEdit(this);
    m_textView->setReadOnly(true);
    m_textView->setFont(QFontDatabase::systemFont(QFontDatabase::FixedFont));
    m_textView->setPlaceholderText(
        QStringLiteral("四元式的纯文本形式（可直接粘贴进实验报告）。"));

    m_warnView = new QPlainTextEdit(this);
    m_warnView->setReadOnly(true);
    m_warnView->setFont(QFontDatabase::systemFont(QFontDatabase::FixedFont));
    m_warnView->setPlaceholderText(
        QStringLiteral("若某些语法树结点没有对应的翻译规则，告警会显示在这里。"));

    auto *lower = new QSplitter(Qt::Horizontal, this);
    auto *textBox = new QGroupBox(QStringLiteral("纯文本形式"), this);
    auto *textLayout = new QVBoxLayout(textBox);
    textLayout->addWidget(m_textView);
    auto *warnBox = new QGroupBox(QStringLiteral("生成告警"), this);
    auto *warnLayout = new QVBoxLayout(warnBox);
    warnLayout->addWidget(m_warnView);
    lower->addWidget(textBox);
    lower->addWidget(warnBox);

    auto *splitter = new QSplitter(Qt::Vertical, this);
    auto *tableBox = new QGroupBox(QStringLiteral("四元式序列"), this);
    auto *tableLayout = new QVBoxLayout(tableBox);
    tableLayout->addWidget(m_quadTable);
    splitter->addWidget(tableBox);
    splitter->addWidget(lower);
    splitter->setStretchFactor(0, 3);
    splitter->setStretchFactor(1, 1);
    root->addWidget(splitter, 1);

    connect(m_exportButton, &QPushButton::clicked, this,
            &IntermediateCodePanel::exportRequested);
    connect(noteBox, &QCheckBox::toggled, this, &IntermediateCodePanel::onToggleNote);
}

/**
 * 功能：推入一次生成结果并刷新显示。
 * 输入：code、title。输出：无。
 * 注意：语法树为空时 `generate()` 会返回空结果 + 一条告警，
 *       这里把告警也显示出来，让"为什么没有四元式"一目了然。
 */
void IntermediateCodePanel::showCode(const IntermediateCode &code, const QString &title)
{
    m_code = code;
    m_title = title;
    m_hasCode = true;
    refresh();
}

/** 功能：清空全部显示。输入：无。输出：无。 */
void IntermediateCodePanel::clearAll()
{
    m_code = IntermediateCode();
    m_title.clear();
    m_hasCode = false;
    m_quadTable->setRowCount(0);
    m_textView->clear();
    m_warnView->clear();
    m_summaryLabel->setText(QStringLiteral("（还没有中间代码）"));
}

/** 功能：刷新表格与文本。输入：无。输出：无。 */
void IntermediateCodePanel::refresh()
{
    QStringList headers;
    QList<QStringList> rows;
    m_code.buildTable(headers, rows);

    m_quadTable->setColumnCount(headers.size());
    m_quadTable->setHorizontalHeaderLabels(headers);
    m_quadTable->setRowCount(rows.size());
    for (int r = 0; r < rows.size(); ++r) {
        const QStringList &row = rows.at(r);
        for (int c = 0; c < row.size() && c < m_quadTable->columnCount(); ++c)
            m_quadTable->setItem(r, c, new QTableWidgetItem(row.at(c)));
    }
    m_quadTable->resizeColumnsToContents();
    m_quadTable->horizontalHeader()->setStretchLastSection(true);

    m_textView->setPlainText(m_code.toText(true));
    m_warnView->setPlainText(m_code.warnings.isEmpty()
                                 ? QStringLiteral("（没有告警：语法树上的结点都有对应的翻译规则）")
                                 : m_code.warnings.join(QLatin1Char('\n')));

    m_summaryLabel->setText(QStringLiteral("%1　|　%2")
                                .arg(m_title.isEmpty() ? QStringLiteral("(未指定)")
                                                       : m_title,
                                     m_code.summary()));
}

/** 功能：显示/隐藏说明列。输入：on。输出：无。 */
void IntermediateCodePanel::onToggleNote(bool on)
{
    // 说明列固定是最后一列
    const int last = m_quadTable->columnCount() - 1;
    if (last >= 0)
        m_quadTable->setColumnHidden(last, !on);
}

/** 功能：四元式表行数。输入：无。输出：行数。 */
int IntermediateCodePanel::quadRowCount() const
{
    return m_quadTable->rowCount();
}

/** 功能：某单元格文本。输入：row、column。输出：文本。 */
QString IntermediateCodePanel::quadCellText(int row, int column) const
{
    const QTableWidgetItem *item = m_quadTable->item(row, column);
    return item == nullptr ? QString() : item->text();
}

/** 功能：摘要标签文本。输入：无。输出：文本。 */
QString IntermediateCodePanel::summaryText() const
{
    return m_summaryLabel->text();
}

/** 功能：纯文本形式。输入：无。输出：文本。 */
QString IntermediateCodePanel::textContent() const
{
    return m_textView->toPlainText();
}

}  // namespace ccd
