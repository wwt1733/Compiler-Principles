/******************************************************************************
 * (1) 版权信息 : 编译原理课程设计 - 华南师范大学计算机学院
 * (2) 文件名称 : DfaPanel.cpp
 *     标识符   : DfaPanel
 *     摘要     : DFA 展示面板的实现。上半部分复用 StateTableView 显示状态转换表，
 *                下半部分为说明区（最小化页签用来显示合并关系与划分过程）。
 * (3) 版本号   : 1.0.0       作者/修改者 : <学号 姓名>   完成日期 : 2026-XX-XX
 * (4) 版本历史 : 1.0.0  2026-XX-XX  首次实现
 *****************************************************************************/

#include "gui/panels/DfaPanel.h"

#include <QFont>
#include <QFontDatabase>
#include <QHBoxLayout>
#include <QLabel>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QSplitter>
#include <QVBoxLayout>

#include "gui/widgets/StateTableView.h"

namespace ccd {

/**
 * 功能：构造 DFA 面板。
 * 输入：title / requirement —— 标题与要求编号；buttonText —— 按钮文字（可为空）；
 *       parent —— 父控件。
 * 输出：无。
 * 注意：buttonText 为空时不创建按钮，页签完全由外部驱动刷新。
 */
DfaPanel::DfaPanel(const QString &title, const QString &requirement, const QString &buttonText,
                   QWidget *parent)
    : QWidget(parent), m_title(title), m_requirement(requirement), m_buttonText(buttonText)
{
    buildUi();
}

/**
 * 功能：构建界面控件与布局。
 * 输入：无。
 * 输出：无。
 * 注意：说明区默认隐藏——页签③（DFA）不需要它，页签④（最小化）由外部调用
 *       setDetailVisible(true) 打开。
 */
void DfaPanel::buildUi()
{
    auto *rootLayout = new QVBoxLayout(this);
    rootLayout->setContentsMargins(8, 8, 8, 8);
    rootLayout->setSpacing(6);

    auto *titleLabel = new QLabel(m_title, this);
    QFont titleFont = titleLabel->font();
    titleFont.setBold(true);
    titleFont.setPointSize(titleFont.pointSize() + 2);
    titleLabel->setFont(titleFont);
    rootLayout->addWidget(titleLabel);

    if (!m_requirement.isEmpty()) {
        auto *requirementLabel = new QLabel(m_requirement, this);
        requirementLabel->setStyleSheet(QStringLiteral("color:#1a5fb4;"));
        rootLayout->addWidget(requirementLabel);
    }

    if (!m_buttonText.isEmpty()) {
        auto *buttonBar = new QHBoxLayout();
        buttonBar->setSpacing(6);
        m_actionButton = new QPushButton(m_buttonText, this);
        buttonBar->addWidget(m_actionButton);
        buttonBar->addStretch(1);
        rootLayout->addLayout(buttonBar);
        connect(m_actionButton, &QPushButton::clicked, this, &DfaPanel::actionRequested);
    }

    m_splitter = new QSplitter(Qt::Vertical, this);

    m_stateTable = new StateTableView(m_splitter);
    m_splitter->addWidget(m_stateTable);

    m_detailContainer = new QWidget(m_splitter);
    auto *detailLayout = new QVBoxLayout(m_detailContainer);
    detailLayout->setContentsMargins(0, 0, 0, 0);
    detailLayout->setSpacing(2);

    auto *detailTitle = new QLabel(QStringLiteral("状态合并关系与划分过程"), m_detailContainer);
    detailTitle->setObjectName(QStringLiteral("detailTitle"));
    detailTitle->setStyleSheet(QStringLiteral("color:#1a5fb4; font-weight:bold;"));
    detailLayout->addWidget(detailTitle);

    m_detailView = new QPlainTextEdit(m_detailContainer);
    m_detailView->setReadOnly(true);
    m_detailView->setFont(QFontDatabase::systemFont(QFontDatabase::FixedFont));
    detailLayout->addWidget(m_detailView, 1);

    m_splitter->addWidget(m_detailContainer);
    m_splitter->setStretchFactor(0, 3);
    m_splitter->setStretchFactor(1, 1);
    rootLayout->addWidget(m_splitter, 1);

    m_hintLabel = new QLabel(this);
    m_hintLabel->setWordWrap(true);
    m_hintLabel->setStyleSheet(QStringLiteral("color:#555555;"));
    rootLayout->addWidget(m_hintLabel);

    setDetailVisible(false);
}

/**
 * 功能：显示 DFA 状态转换表。
 * 输入：dfa —— DFA 数据；caption —— 表格上方说明。
 * 输出：无。
 * 注意：接受态个数以 dfa.accepting 标志统计（编码为 -1 的接受态也要算进去）。
 */
void DfaPanel::setDfa(const Dfa &dfa, const QString &caption)
{
    m_stateTable->setDfa(dfa, caption);

    int acceptCount = 0;
    int mergedCount = 0;
    for (int s = 0; s < dfa.stateCount; ++s) {
        if (dfa.accepting.value(s, false))
            ++acceptCount;
    }
    for (const QSet<int> &subset : dfa.subsets)
        mergedCount += subset.size();

    int edgeCount = 0;
    for (const QMap<QString, int> &row : dfa.trans)
        edgeCount += row.size();

    m_hintLabel->setText(QStringLiteral("共 %1 个状态（接受态 %2 个）、%3 条转移、"
                                        "字母表 %4 个符号；状态集合合计 %5 个原状态。")
                             .arg(dfa.stateCount)
                             .arg(acceptCount)
                             .arg(edgeCount)
                             .arg(dfa.alphabet.size())
                             .arg(mergedCount));
}

/** 功能：设置说明区文本。输入：text —— 文本。输出：无。 */
void DfaPanel::setDetailText(const QString &text)
{
    m_detailView->setPlainText(text);
    if (!text.isEmpty())
        setDetailVisible(true);
}

/** 功能：设置说明区标题。输入：title —— 标题。输出：无。 */
void DfaPanel::setDetailTitle(const QString &title)
{
    if (auto *label = m_detailContainer->findChild<QLabel *>(QStringLiteral("detailTitle")))
        label->setText(title);
}

/** 功能：是否显示说明区。输入：visible —— 是否显示。输出：无。 */
void DfaPanel::setDetailVisible(bool visible)
{
    m_detailContainer->setVisible(visible);
    if (!visible)
        m_splitter->setStretchFactor(0, 1);
}

/** 功能：清空全部显示内容。输入：无。输出：无。 */
void DfaPanel::clearAll()
{
    m_stateTable->clearAll();
    m_detailView->clear();
    m_hintLabel->clear();
}

/** 功能：【自测用】取 DOT 文本。输入：无。输出：DOT 文本（无数据时为空串）。 */
QString DfaPanel::dotText() const
{
    return m_stateTable == nullptr ? QString() : m_stateTable->dotText();
}

/** 功能：【自测用】取状态转换表列数。输入：无。输出：列数（无数据时为 0）。 */
int DfaPanel::columnCount() const
{
    return m_stateTable == nullptr ? 0 : m_stateTable->columnCount();
}

/** 功能：【自测用】取字符类对照图例。输入：无。输出：图例文本（无类表时为空串）。 */
QString DfaPanel::charClassLegend() const
{
    return m_stateTable == nullptr ? QString() : m_stateTable->charClassLegend();
}

}  // namespace ccd
