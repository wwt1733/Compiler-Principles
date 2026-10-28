/******************************************************************************
 * (1) 版权信息 : 编译原理课程设计 - 华南师范大学计算机学院
 * (2) 文件名称 : LexCodePanel.cpp
 *     标识符   : LexCodePanel
 *     摘要     : 页签⑤（词法分析源程序）的实现。
 * (3) 版本号   : 1.0.0       作者/修改者 : <学号 姓名>   完成日期 : 2026-XX-XX
 * (4) 版本历史 : 1.0.0  2026-XX-XX  首次实现
 *****************************************************************************/

#include "gui/panels/LexCodePanel.h"

#include <QComboBox>
#include <QFileDialog>
#include <QFontDatabase>
#include <QHBoxLayout>
#include <QLabel>
#include <QMessageBox>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QSignalBlocker>
#include <QVBoxLayout>

#include "common/FileUtil.h"
#include "common/Logger.h"

namespace ccd {

/**
 * 功能：构造词法分析源程序面板。
 * 输入：parent —— 父控件。
 * 输出：无。
 * 注意：构造函数里同步下拉框会触发 currentIndexChanged，因此必须先连信号
 *       再把索引落到 0，或者用 blockSignals 兜住（这里用后者，简单可靠）。
 */
LexCodePanel::LexCodePanel(QWidget *parent) : QWidget(parent)
{
    buildUi();
}

/**
 * 功能：构建界面控件与布局。
 * 输入：无。
 * 输出：无。
 * 注意：源码区只读，但允许选中复制；使用等宽字体便于对照讲稿阅读。
 */
void LexCodePanel::buildUi()
{
    auto *rootLayout = new QVBoxLayout(this);
    rootLayout->setContentsMargins(8, 8, 8, 8);
    rootLayout->setSpacing(6);

    auto *titleLabel = new QLabel(QStringLiteral("⑤ 词法分析源程序"), this);
    QFont titleFont = titleLabel->font();
    titleFont.setBold(true);
    titleFont.setPointSize(titleFont.pointSize() + 2);
    titleLabel->setFont(titleFont);
    rootLayout->addWidget(titleLabel);

    auto *requirementLabel = new QLabel(
        QStringLiteral("要求 1.7：由最小化 DFA 生成 C/C++ 词法分析源程序。"
                       "方法一按字符判断（JudgeChar/GetNext/GetToken）；"
                       "方法二用 switch(state) 直接编码状态转移。"),
        this);
    requirementLabel->setWordWrap(true);
    requirementLabel->setStyleSheet(QStringLiteral("color:#1a5fb4;"));
    rootLayout->addWidget(requirementLabel);

    auto *buttonBar = new QHBoxLayout();
    buttonBar->setSpacing(6);

    buttonBar->addWidget(new QLabel(QStringLiteral("转换方法："), this));
    m_methodCombo = new QComboBox(this);
    m_methodCombo->addItem(QStringLiteral("方法一：按字符判断（judge-char）"),
                           static_cast<int>(LexCodeGen::Method::JudgeChar));
    m_methodCombo->addItem(QStringLiteral("方法二：switch(state)"),
                           static_cast<int>(LexCodeGen::Method::SwitchState));
    buttonBar->addWidget(m_methodCombo);

    buttonBar->addSpacing(12);
    buttonBar->addWidget(new QLabel(QStringLiteral(".lex 排版："), this));
    m_layoutCombo = new QComboBox(this);
    m_layoutCombo->addItem(QStringLiteral("每行一个单词编码"),
                           static_cast<int>(LexCodeGen::Layout::OnePerLine));
    m_layoutCombo->addItem(QStringLiteral("任务书示例（拼成一行）"),
                           static_cast<int>(LexCodeGen::Layout::TaskbookSample));
    buttonBar->addWidget(m_layoutCombo);

    buttonBar->addSpacing(12);
    m_regenerateButton = new QPushButton(QStringLiteral("重新生成"), this);
    m_saveButton = new QPushButton(QStringLiteral("另存源码..."), this);
    buttonBar->addWidget(m_regenerateButton);
    buttonBar->addWidget(m_saveButton);
    buttonBar->addStretch(1);
    rootLayout->addLayout(buttonBar);

    m_codeView = new QPlainTextEdit(this);
    m_codeView->setReadOnly(true);
    m_codeView->setLineWrapMode(QPlainTextEdit::NoWrap);
    m_codeView->setFont(QFontDatabase::systemFont(QFontDatabase::FixedFont));
    rootLayout->addWidget(m_codeView, 1);

    m_hintLabel = new QLabel(QStringLiteral("尚未生成源程序。"), this);
    m_hintLabel->setWordWrap(true);
    m_hintLabel->setStyleSheet(QStringLiteral("color:#555555;"));
    rootLayout->addWidget(m_hintLabel);

    // 下拉框变化即请求重新生成；两个动作按钮各司其职
    connect(m_methodCombo, &QComboBox::currentIndexChanged, this, [this](int) {
        emit regenerateRequested();
    });
    connect(m_layoutCombo, &QComboBox::currentIndexChanged, this, [this](int) {
        emit regenerateRequested();
    });
    connect(m_regenerateButton, &QPushButton::clicked, this, &LexCodePanel::regenerateRequested);
    connect(m_saveButton, &QPushButton::clicked, this, &LexCodePanel::saveToFile);
}

/**
 * 功能：注入生成的源程序文本与统计摘要。
 * 输入：code —— 源码文本；summary —— 统计摘要（可为空）。
 * 输出：无。
 * 注意：源码行数按 '\n' 统计，用于向用户交代生成规模。
 */
void LexCodePanel::setSource(const QString &code, const QString &summary)
{
    m_codeView->setPlainText(code);
    if (summary.isEmpty()) {
        m_hintLabel->setText(QStringLiteral("共 %1 行。")
                                 .arg(code.isEmpty() ? 0 : code.count(QLatin1Char('\n'))));
    } else {
        m_hintLabel->setText(summary);
    }
}

/** 功能：取当前显示的源程序文本。输入：无。输出：源码文本。 */
QString LexCodePanel::source() const
{
    return m_codeView->toPlainText();
}

/** 功能：取统计摘要 / 失败原因文本。输入：无。输出：摘要文本。 */
QString LexCodePanel::summaryText() const
{
    return m_hintLabel == nullptr ? QString() : m_hintLabel->text();
}

/** 功能：设置当前转换方法（同步下拉框，不触发重新生成）。输入：method。输出：无。 */
void LexCodePanel::setMethod(LexCodeGen::Method method)
{
    const QSignalBlocker blocker(m_methodCombo);
    m_methodCombo->setCurrentIndex(method == LexCodeGen::Method::SwitchState ? 1 : 0);
}

/** 功能：取当前选中的转换方法。输入：无。输出：方法枚举。 */
LexCodeGen::Method LexCodePanel::method() const
{
    return static_cast<LexCodeGen::Method>(m_methodCombo->currentData().toInt());
}

/** 功能：设置当前 .lex 排版（同步下拉框，不触发重新生成）。输入：layout。输出：无。 */
void LexCodePanel::setLayout(LexCodeGen::Layout layout)
{
    const QSignalBlocker blocker(m_layoutCombo);
    m_layoutCombo->setCurrentIndex(layout == LexCodeGen::Layout::TaskbookSample ? 1 : 0);
}

/** 功能：取当前选中的 .lex 排版。输入：无。输出：排版枚举。 */
LexCodeGen::Layout LexCodePanel::layout() const
{
    return static_cast<LexCodeGen::Layout>(m_layoutCombo->currentData().toInt());
}

/** 功能：清空显示。输入：无。输出：无。 */
void LexCodePanel::clearAll()
{
    m_codeView->clear();
    m_hintLabel->setText(QStringLiteral("尚未生成源程序。"));
}

/**
 * 功能：把当前显示的源程序另存为用户指定的文件。
 * 输入：无。
 * 输出：无（结果通过日志与提示框反馈）。
 * 注意：写文件统一走 FileUtil::writeText，保证换行与编码规范一致。
 */
void LexCodePanel::saveToFile()
{
    const QString code = source();
    if (code.isEmpty()) {
        QMessageBox::information(this, QStringLiteral("另存源码"),
                                 QStringLiteral("当前没有可保存的源程序，请先生成。"));
        return;
    }
    const QString path = QFileDialog::getSaveFileName(
        this, QStringLiteral("保存词法分析源程序"), QStringLiteral("lexer.cpp"),
        QStringLiteral("C++ 源程序 (*.cpp);;所有文件 (*)"));
    if (path.isEmpty())
        return;
    if (FileUtil::writeText(path, code)) {
        LOG_INFO(QStringLiteral("词法分析源程序已保存：%1").arg(path));
        QMessageBox::information(this, QStringLiteral("另存源码"),
                                 QStringLiteral("已保存到：\n%1").arg(path));
    } else {
        LOG_ERROR(QStringLiteral("词法分析源程序保存失败：%1").arg(path));
        QMessageBox::warning(this, QStringLiteral("另存源码"),
                             QStringLiteral("保存失败：\n%1").arg(path));
    }
}

}  // namespace ccd
