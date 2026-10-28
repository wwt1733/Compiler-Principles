/******************************************************************************
 * (1) 版权信息 : 编译原理课程设计 - 华南师范大学计算机学院
 * (2) 文件名称 : NfaPanel.cpp
 *     标识符   : NfaPanel
 *     摘要     : NFA 展示面板的实现。上半部分为 NFA 状态转换表（复用
 *                StateTableView 控件），下半部分为各定义的正则式与逆波兰式
 *                对照文本，便于实验报告中展示中间过程。
 * (3) 版本号   : 1.0.0       作者/修改者 : <学号 姓名>   完成日期 : 2026-XX-XX
 * (4) 版本历史 : 1.0.0  2026-XX-XX  首次实现
 *****************************************************************************/

#include "gui/panels/NfaPanel.h"

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
 * 功能：构造 NFA 面板。
 * 输入：parent —— 父控件。
 * 输出：无。
 * 注意：构造后表格为空，需调用 setNfa() 注入数据。
 */
NfaPanel::NfaPanel(QWidget *parent) : QWidget(parent)
{
    buildUi();
}

/**
 * 功能：构建界面控件与布局。
 * 输入：无。
 * 输出：无。
 * 注意：使用垂直 QSplitter 让用户可调整"状态转换表"与"逆波兰式对照"的比例，
 *       默认表格占 3/4 高度。
 */
void NfaPanel::buildUi()
{
    auto *rootLayout = new QVBoxLayout(this);
    rootLayout->setContentsMargins(8, 8, 8, 8);
    rootLayout->setSpacing(6);

    auto *titleLabel = new QLabel(
        QStringLiteral("② NFA 状态转换表　（任务书要求 1.4）"), this);
    QFont titleFont = titleLabel->font();
    titleFont.setBold(true);
    titleFont.setPointSize(titleFont.pointSize() + 2);
    titleLabel->setFont(titleFont);
    rootLayout->addWidget(titleLabel);

    auto *buttonBar = new QHBoxLayout();
    buttonBar->setSpacing(6);
    m_buildButton = new QPushButton(QStringLiteral("由当前规格构造 NFA"), this);
    m_buildButton->setToolTip(QStringLiteral(
        "读取“① 单词规格编辑器”中的文本，完成：正则式 → 逆波兰式 → NFA（Thompson 构造）"));
    buttonBar->addWidget(m_buildButton);
    buttonBar->addStretch(1);
    rootLayout->addLayout(buttonBar);

    m_splitter = new QSplitter(Qt::Vertical, this);

    // 上半：状态转换表
    m_stateTable = new StateTableView(m_splitter);
    m_splitter->addWidget(m_stateTable);

    // 下半：逆波兰式对照
    auto *postfixContainer = new QWidget(m_splitter);
    auto *postfixLayout = new QVBoxLayout(postfixContainer);
    postfixLayout->setContentsMargins(0, 0, 0, 0);
    postfixLayout->setSpacing(2);

    auto *postfixTitle = new QLabel(
        QStringLiteral("中间过程对照：定义名 → 正则式（中缀） → 逆波兰式"), postfixContainer);
    postfixTitle->setStyleSheet(QStringLiteral("color:#1a5fb4; font-weight:bold;"));
    postfixLayout->addWidget(postfixTitle);

    m_postfixView = new QPlainTextEdit(postfixContainer);
    m_postfixView->setReadOnly(true);
    m_postfixView->setFont(QFontDatabase::systemFont(QFontDatabase::FixedFont));
    m_postfixView->setPlaceholderText(
        QStringLiteral("点击“由当前规格构造 NFA”后，此处显示每个定义的逆波兰式。"));
    postfixLayout->addWidget(m_postfixView, 1);

    m_splitter->addWidget(postfixContainer);
    m_splitter->setStretchFactor(0, 3);
    m_splitter->setStretchFactor(1, 1);
    rootLayout->addWidget(m_splitter, 1);

    m_hintLabel = new QLabel(
        QStringLiteral("提示：NFA 中每个接受状态携带其对应的单词编码；"
                       "注释类规则参与自动机但不输出编码。"), this);
    m_hintLabel->setWordWrap(true);
    m_hintLabel->setStyleSheet(QStringLiteral("color:#555555;"));
    rootLayout->addWidget(m_hintLabel);

    connect(m_buildButton, &QPushButton::clicked, this, &NfaPanel::buildRequested);
}

/**
 * 功能：显示 NFA 状态转换表。
 * 输入：nfa —— NFA 数据；caption —— 表格上方说明文字。
 * 输出：无。
 * 注意：表格的 ε 列由 StateTableView 自动生成，无需在此处理。
 */
void NfaPanel::setNfa(const Nfa &nfa, const QString &caption)
{
    m_stateTable->setNfa(nfa, caption);
    m_hintLabel->setText(
        QStringLiteral("NFA 共 %1 个状态、%2 条边、%3 个接受状态；字母表 %4 个符号。"
                       "（ε 列表示空串转移，同一格多个目的状态以逗号分隔）")
            .arg(nfa.stateCount)
            .arg(nfa.edges.size())
            .arg(nfa.accepts.size())
            .arg(nfa.alphabet.size()));
}

/** 功能：设置逆波兰式对照文本。输入：text —— 文本。输出：无。 */
void NfaPanel::setPostfixText(const QString &text)
{
    m_postfixView->setPlainText(text);
}

/** 功能：清空全部显示内容。输入：无。输出：无。 */
void NfaPanel::clearAll()
{
    m_stateTable->clearAll();
    m_postfixView->clear();
    m_hintLabel->clear();
}

/**
 * 功能：【自测用】取 NFA 图的 DOT 文本。
 * 输入：无。
 * 输出：DOT 文本（无数据时为空串）。
 * 注意：NFA 图是唯一会画出 ε 边的图 —— 这条边不消耗输入字符，
 *       过去从状态转换表里凑 DOT 时被整批丢掉了（见 StateTableView 的说明）。
 */
QString NfaPanel::dotText() const
{
    return m_stateTable == nullptr ? QString() : m_stateTable->dotText();
}

}  // namespace ccd
