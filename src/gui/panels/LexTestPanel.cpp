/******************************************************************************
 * (1) 版权信息 : 编译原理课程设计 - 华南师范大学计算机学院
 * (2) 文件名称 : LexTestPanel.cpp
 *     标识符   : LexTestPanel
 *     摘要     : 页签⑥（编译与词法测试）的实现。
 * (3) 版本号   : 1.0.0       作者/修改者 : <学号 姓名>   完成日期 : 2026-XX-XX
 * (4) 版本历史 : 1.0.0  2026-XX-XX  首次实现
 *****************************************************************************/

#include "gui/panels/LexTestPanel.h"

#include <QFileDialog>
#include <QFontDatabase>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QTabWidget>
#include <QVBoxLayout>

#include "common/FileUtil.h"
#include "common/Logger.h"

namespace ccd {

namespace {

/**
 * 功能：创建只读、等宽的文本显示控件。
 * 输入：parent —— 父控件。
 * 输出：配置好的 QPlainTextEdit。
 * 注意：不自动换行（.lex 一行可能较长，换行会破坏"一行一个单词"的可读性）。
 */
QPlainTextEdit *makeReadOnlyView(QWidget *parent)
{
    auto *view = new QPlainTextEdit(parent);
    view->setReadOnly(true);
    view->setLineWrapMode(QPlainTextEdit::NoWrap);
    view->setFont(QFontDatabase::systemFont(QFontDatabase::FixedFont));
    return view;
}

}  // namespace

/**
 * 功能：构造编译与词法测试面板。
 * 输入：parent —— 父控件。
 * 输出：无。
 */
LexTestPanel::LexTestPanel(QWidget *parent) : QWidget(parent)
{
    buildUi();
}

/**
 * 功能：构建界面控件与布局。
 * 输入：无。
 * 输出：无。
 * 注意：编译器路径默认留空，由流水线的 setCompiler("g++") 从 PATH 查找；
 *       界面上给出提示，便于换机时改成本机 g++ 的绝对路径。
 */
void LexTestPanel::buildUi()
{
    auto *rootLayout = new QVBoxLayout(this);
    rootLayout->setContentsMargins(8, 8, 8, 8);
    rootLayout->setSpacing(6);

    auto *titleLabel = new QLabel(QStringLiteral("⑥ 编译与词法测试"), this);
    QFont titleFont = titleLabel->font();
    titleFont.setBold(true);
    titleFont.setPointSize(titleFont.pointSize() + 2);
    titleLabel->setFont(titleFont);
    rootLayout->addWidget(titleLabel);

    auto *requirementLabel = new QLabel(
        QStringLiteral("要求 1.8 / 1.9：对生成的源程序调用 C/C++ 编译器生成可执行程序，"
                       "再以本语言的源程序测试，输出并显示单词编码文件 sample.lex。"),
        this);
    requirementLabel->setWordWrap(true);
    requirementLabel->setStyleSheet(QStringLiteral("color:#1a5fb4;"));
    rootLayout->addWidget(requirementLabel);

    auto *form = new QFormLayout();
    form->setSpacing(6);

    auto *sourceRow = new QHBoxLayout();
    m_sourceEdit = new QLineEdit(this);
    m_sourceEdit->setPlaceholderText(QStringLiteral("待词法分析的源程序，如 data/sample.tny"));
    m_browseButton = new QPushButton(QStringLiteral("浏览..."), this);
    sourceRow->addWidget(m_sourceEdit, 1);
    sourceRow->addWidget(m_browseButton);
    form->addRow(QStringLiteral("源程序："), sourceRow);

    m_compilerEdit = new QLineEdit(this);
    m_compilerEdit->setPlaceholderText(
        QStringLiteral("留空表示从 PATH 查找 g++；也可填绝对路径，"
                       "如 D:\\Qt6.10\\Tools\\mingw1310_64\\bin\\g++.exe"));
    form->addRow(QStringLiteral("编译器："), m_compilerEdit);

    rootLayout->addLayout(form);

    auto *actionRow = new QHBoxLayout();
    m_runButton = new QPushButton(QStringLiteral("编译并测试"), this);
    actionRow->addWidget(m_runButton);
    actionRow->addStretch(1);
    rootLayout->addLayout(actionRow);

    m_statusLabel = new QLabel(QStringLiteral("尚未执行。"), this);
    m_statusLabel->setWordWrap(true);
    m_statusLabel->setStyleSheet(QStringLiteral("color:#555555;"));
    rootLayout->addWidget(m_statusLabel);

    m_outputTabs = new QTabWidget(this);
    m_outputTabs->setDocumentMode(true);

    m_compilerView = makeReadOnlyView(m_outputTabs);
    m_runView = makeReadOnlyView(m_outputTabs);

    auto *lexContainer = new QWidget(m_outputTabs);
    auto *lexLayout = new QVBoxLayout(lexContainer);
    lexLayout->setContentsMargins(0, 0, 0, 0);
    lexLayout->setSpacing(4);
    auto *lexBar = new QHBoxLayout();
    m_saveLexButton = new QPushButton(QStringLiteral("另存单词编码文件..."), lexContainer);
    lexBar->addWidget(m_saveLexButton);
    lexBar->addStretch(1);
    lexLayout->addLayout(lexBar);
    m_lexView = makeReadOnlyView(lexContainer);
    lexLayout->addWidget(m_lexView, 1);

    m_outputTabs->addTab(m_compilerView, QStringLiteral("编译器输出"));
    m_outputTabs->addTab(m_runView, QStringLiteral("运行输出"));
    m_outputTabs->addTab(lexContainer, QStringLiteral("单词编码 (.lex)"));
    rootLayout->addWidget(m_outputTabs, 1);

    connect(m_runButton, &QPushButton::clicked, this, &LexTestPanel::runRequested);
    connect(m_browseButton, &QPushButton::clicked, this, &LexTestPanel::browseRequested);
    connect(m_saveLexButton, &QPushButton::clicked, this, &LexTestPanel::saveLexToFile);
}

/**
 * 功能：注入一次"编译 + 运行"的完整结果。
 * 输入：result —— 流水线结果；summary —— 一句话摘要。
 * 输出：无。
 * 注意：即便编译失败也要把编译器输出显示出来——这是任务书要求"必须提供编译
 *       结果"的取证材料，不能只在成功时才显示。
 */
void LexTestPanel::setResult(const LexBuild::Result &result, const QString &summary)
{
    m_lexPath = result.lexPath;
    m_compilerView->setPlainText(result.compilerOutput.isEmpty()
                                     ? QStringLiteral("（编译器无输出）")
                                     : result.compilerOutput);
    m_runView->setPlainText(result.runOutput.isEmpty()
                                ? QStringLiteral("（运行无标准错误输出）")
                                : result.runOutput);
    m_lexView->setPlainText(result.lexContent.isEmpty()
                                ? QStringLiteral("（未产出单词编码文件）")
                                : result.lexContent);

    QString status = summary;
    if (!result.exePath.isEmpty())
        status += QStringLiteral("\n可执行文件：%1").arg(result.exePath);
    if (!result.lexPath.isEmpty())
        status += QStringLiteral("\n单词编码文件：%1").arg(result.lexPath);
    m_statusLabel->setText(status);
    m_statusLabel->setStyleSheet(result.ran ? QStringLiteral("color:#1a7f37;")
                                            : QStringLiteral("color:#b3261e;"));
    // 成功时把焦点停在 .lex 页，失败时停在编译器输出页
    m_outputTabs->setCurrentIndex(result.ran ? 2 : 0);
}

/** 功能：只设置提示文本。输入：text —— 提示。输出：无。 */
void LexTestPanel::setHint(const QString &text)
{
    m_statusLabel->setText(text);
    m_statusLabel->setStyleSheet(QStringLiteral("color:#b3261e;"));
}

/** 功能：取待测源程序路径。输入：无。输出：路径文本。 */
QString LexTestPanel::sourceFile() const
{
    return m_sourceEdit->text().trimmed();
}

/** 功能：设置待测源程序路径。输入：path —— 路径。输出：无。 */
void LexTestPanel::setSourceFile(const QString &path)
{
    m_sourceEdit->setText(path);
}

/** 功能：取编译器路径。输入：无。输出：路径文本（可能为空）。 */
QString LexTestPanel::compilerPath() const
{
    return m_compilerEdit->text().trimmed();
}

/** 功能：清空全部输出。输入：无。输出：无。 */
void LexTestPanel::clearAll()
{
    m_compilerView->clear();
    m_runView->clear();
    m_lexView->clear();
    m_lexPath.clear();
    m_statusLabel->setText(QStringLiteral("尚未执行。"));
    m_statusLabel->setStyleSheet(QStringLiteral("color:#555555;"));
}

/**
 * 功能：把当前 .lex 内容另存为用户指定的文件。
 * 输入：无。
 * 输出：无（结果通过日志与提示框反馈）。
 * 注意：交付要求的 sample.lex 通常由这里导出，因此默认文件名就叫 sample.lex。
 */
void LexTestPanel::saveLexToFile()
{
    const QString content = m_lexView->toPlainText();
    if (content.isEmpty()) {
        QMessageBox::information(this, QStringLiteral("另存单词编码文件"),
                                 QStringLiteral("当前没有可保存的内容，请先编译并测试。"));
        return;
    }
    const QString path = QFileDialog::getSaveFileName(
        this, QStringLiteral("保存单词编码文件"),
        m_lexPath.isEmpty() ? QStringLiteral("sample.lex") : m_lexPath,
        QStringLiteral("单词编码文件 (*.lex);;所有文件 (*)"));
    if (path.isEmpty())
        return;
    if (FileUtil::writeText(path, content)) {
        LOG_INFO(QStringLiteral("单词编码文件已保存：%1").arg(path));
        QMessageBox::information(this, QStringLiteral("另存单词编码文件"),
                                 QStringLiteral("已保存到：\n%1").arg(path));
    } else {
        LOG_ERROR(QStringLiteral("单词编码文件保存失败：%1").arg(path));
        QMessageBox::warning(this, QStringLiteral("另存单词编码文件"),
                             QStringLiteral("保存失败：\n%1").arg(path));
    }
}

}  // namespace ccd
