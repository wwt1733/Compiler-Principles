/******************************************************************************
 * (1) 版权信息 : 编译原理课程设计 - 华南师范大学计算机学院
 * (2) 文件名称 : GrammarEditorPanel.cpp
 *     标识符   : GrammarEditorPanel
 *     摘要     : 文法编辑器面板的实现。提供 BNF 文法文本编辑框，支持打开/保存/
 *                另存为、载入随附语言文法、以及在 mini-C 原文文法与修正版文法
 *                之间切换。
 * (3) 版本号   : 1.0.0       作者/修改者 : <学号 姓名>   完成日期 : 2026-XX-XX
 * (4) 版本历史 : 1.0.0  2026-XX-XX  首次实现
 *****************************************************************************/

#include "gui/panels/GrammarEditorPanel.h"

#include <QCheckBox>
#include <QFileDialog>
#include <QFileInfo>
#include <QFont>
#include <QFontDatabase>
#include <QHBoxLayout>
#include <QLabel>
#include <QMessageBox>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QVBoxLayout>

#include "common/FileUtil.h"
#include "common/LanguageConfig.h"
#include "common/Logger.h"

namespace ccd {

/**
 * 功能：构造文法编辑器面板。
 * 输入：parent —— 父控件。
 * 输出：无。
 * 注意：构造时不载入文件，由 MainWindow 在语言切换时调用 loadLanguageGrammar()。
 */
GrammarEditorPanel::GrammarEditorPanel(QWidget *parent) : QWidget(parent)
{
    buildUi();
}

/**
 * 功能：构建界面控件与布局。
 * 输入：无。
 * 输出：无。
 * 注意：文法文本较长，因此编辑框占满整个面板，不设结果表（化简结果在⑧页签显示）。
 */
void GrammarEditorPanel::buildUi()
{
    auto *rootLayout = new QVBoxLayout(this);
    rootLayout->setContentsMargins(8, 8, 8, 8);
    rootLayout->setSpacing(6);

    auto *titleLabel = new QLabel(
        QStringLiteral("⑦ 文法编辑器（BNF）　（任务书要求 2.1）"), this);
    QFont titleFont = titleLabel->font();
    titleFont.setBold(true);
    titleFont.setPointSize(titleFont.pointSize() + 2);
    titleLabel->setFont(titleFont);
    rootLayout->addWidget(titleLabel);

    auto *buttonBar = new QHBoxLayout();
    buttonBar->setSpacing(6);

    m_openButton = new QPushButton(QStringLiteral("打开文法文件…"), this);
    m_saveButton = new QPushButton(QStringLiteral("保存"), this);
    m_saveAsButton = new QPushButton(QStringLiteral("另存为…"), this);
    m_simplifyButton = new QPushButton(QStringLiteral("载入并化简文法"), this);
    m_pipelineButton = new QPushButton(QStringLiteral("执行完整语法流水线"), this);
    m_fixedCheckBox = new QCheckBox(QStringLiteral("使用修正版文法（mini-C）"), this);
    m_fixedCheckBox->setToolTip(QStringLiteral(
        "任务书原文的 mini-C 文法中，产生式 13 引用了未定义的 dowhile-stmt，而 while-stmt\n"
        "定义后无人引用，会导致 while 语句无法分析。勾选后改用 minic_grammar_fixed.txt。"));
    m_simplifyButton->setToolTip(
        QStringLiteral("载入当前文法并删除不可达、无法推导出句子的规则（要求 2.2）"));
    m_pipelineButton->setToolTip(QStringLiteral(
        "依次执行：文法化简→First/Follow→LR(0)DFA→LALR(1)分析表→语法分析过程→语法树"));

    buttonBar->addWidget(m_openButton);
    buttonBar->addWidget(m_saveButton);
    buttonBar->addWidget(m_saveAsButton);
    buttonBar->addSpacing(12);
    buttonBar->addWidget(m_simplifyButton);
    buttonBar->addWidget(m_pipelineButton);
    buttonBar->addSpacing(12);
    buttonBar->addWidget(m_fixedCheckBox);
    buttonBar->addStretch(1);
    rootLayout->addLayout(buttonBar);

    m_fileLabel = new QLabel(QStringLiteral("当前文件：（未保存）"), this);
    m_fileLabel->setStyleSheet(QStringLiteral("color:#555555;"));
    rootLayout->addWidget(m_fileLabel);

    m_editor = new QPlainTextEdit(this);
    m_editor->setFont(QFontDatabase::systemFont(QFontDatabase::FixedFont));
    m_editor->setTabStopDistance(4 * m_editor->fontMetrics().horizontalAdvance(QLatin1Char(' ')));
    m_editor->setPlaceholderText(QStringLiteral(
        "在此输入 BNF 文法，一行一条产生式，用 | 分隔多个候选式，用 # 或 @ 表示空串，\n"
        "第一个非终结符即开始符号。例如：\n"
        "program -> stmt-sequence\n"
        "stmt-sequence -> stmt-sequence ; statement | statement\n"
        "statement -> if-stmt | repeat-stmt | assign-stmt | read-stmt | write-stmt\n"
        "assign-stmt -> identifier := exp"));
    rootLayout->addWidget(m_editor, 1);

    m_hintLabel = new QLabel(this);
    m_hintLabel->setWordWrap(true);
    setHint(QStringLiteral("提示：空串请写 #（任务书 p.3）或 @（mini-C 文法，任务书 p.8），两者等效。"));
    rootLayout->addWidget(m_hintLabel);

    connect(m_openButton, &QPushButton::clicked, this, &GrammarEditorPanel::openFile);
    connect(m_saveButton, &QPushButton::clicked, this, [this] { saveFile(); });
    connect(m_saveAsButton, &QPushButton::clicked, this, [this] { saveFileAs(); });
    connect(m_simplifyButton, &QPushButton::clicked, this,
            &GrammarEditorPanel::loadAndSimplifyRequested);
    connect(m_pipelineButton, &QPushButton::clicked, this,
            &GrammarEditorPanel::fullPipelineRequested);
    connect(m_fixedCheckBox, &QCheckBox::toggled, this, [this](bool) {
        loadLanguageGrammar(m_language, useFixedGrammar());
    });
}

// ---------------------------------------------------------------------------
// 文件操作
// ---------------------------------------------------------------------------

/**
 * 功能：载入随程序分发的指定语言 BNF 文法文件。
 * 输入：id —— 语言标识；useFixedVersion —— 是否使用修正版文法（mini-C）。
 * 输出：无。
 * 注意：
 *  1. 修正版文法文件不存在时自动回退到原文文法，并在提示区说明；
 *  2. 会同步更新 m_language 并发出 languageChanged 信号。
 */
void GrammarEditorPanel::loadLanguageGrammar(LanguageId id, bool useFixedVersion)
{
    m_language = id;
    const LanguageConfig &cfg = LanguageConfigRegistry::get(id);

    QString fileName = cfg.grammarFile;
    bool usedFixed = false;
    if (useFixedVersion && !cfg.grammarFixedFile.isEmpty()) {
        const QString fixedPath = FileUtil::locateDataFile(cfg.grammarFixedFile);
        if (QFileInfo::exists(fixedPath)) {
            fileName = cfg.grammarFixedFile;
            usedFixed = true;
        } else {
            setHint(QStringLiteral("未找到修正版文法文件 %1，已回退到原文文法。")
                        .arg(cfg.grammarFixedFile),
                    true);
        }
    }

    const QString path = FileUtil::locateDataFile(fileName);
    bool ok = false;
    QString encoding;
    const QString content = FileUtil::readText(path, &ok, &encoding);
    if (!ok) {
        setHint(QStringLiteral("无法读取文法文件：%1").arg(FileUtil::toNativePath(path)), true);
        LOG_ERROR(QStringLiteral("读取文法文件失败：%1").arg(path));
        return;
    }

    m_currentFile = path;
    m_editor->setPlainText(content);
    updateFileLabel();
    setHint(QStringLiteral("已载入 %1 的 BNF 文法%2（编码 %3，%4 行）。")
                .arg(cfg.displayName,
                     usedFixed ? QStringLiteral("（修正版）") : QString(),
                     encoding)
                .arg(content.count(QLatin1Char('\n')) + 1));
    LOG_INFO(QStringLiteral("载入文法文件：%1").arg(path));
    emit languageChanged(m_language);
}

/** 功能：取编辑框文本。输入：无。输出：全文。 */
QString GrammarEditorPanel::text() const
{
    return m_editor->toPlainText();
}

/** 功能：设置编辑框文本。输入：text —— 新内容。输出：无。 */
void GrammarEditorPanel::setText(const QString &text)
{
    m_editor->setPlainText(text);
}

/** 功能：是否勾选"使用修正版文法"。输入：无。输出：勾选状态。 */
bool GrammarEditorPanel::useFixedGrammar() const
{
    return m_fixedCheckBox->isChecked();
}

/**
 * 功能：打开文法文件。
 * 输入：无（弹出文件对话框）。
 * 输出：无。
 * 注意：读出后不改变 languageId，仅更新文本与文件标签。
 */
void GrammarEditorPanel::openFile()
{
    const QString startDir = FileUtil::ensureDir(FileUtil::dataDir()) ? FileUtil::dataDir()
                                                                     : FileUtil::appDir();
    const QString path = QFileDialog::getOpenFileName(
        this, QStringLiteral("打开 BNF 文法文件"), startDir,
        QStringLiteral("文本文件 (*.txt);;所有文件 (*.*)"));
    if (path.isEmpty())
        return;

    bool ok = false;
    QString encoding;
    const QString content = FileUtil::readText(path, &ok, &encoding);
    if (!ok) {
        QMessageBox::warning(this, QStringLiteral("打开失败"),
                             QStringLiteral("无法读取文件：\n%1").arg(FileUtil::toNativePath(path)));
        return;
    }
    m_currentFile = path;
    m_editor->setPlainText(content);
    updateFileLabel();
    setHint(QStringLiteral("已打开：%1（编码 %2）").arg(FileUtil::toNativePath(path), encoding));
    LOG_INFO(QStringLiteral("打开文法文件：%1").arg(path));
}

/**
 * 功能：保存到当前文件；无当前文件时转为另存为。
 * 输入：无。
 * 输出：保存成功为 true。
 * 注意：写出编码固定为 UTF-8（不带 BOM）。
 */
bool GrammarEditorPanel::saveFile()
{
    if (m_currentFile.isEmpty())
        return saveFileAs();

    if (!FileUtil::writeText(m_currentFile, m_editor->toPlainText(), false)) {
        QMessageBox::warning(this, QStringLiteral("保存失败"),
                             QStringLiteral("无法写入文件：\n%1")
                                 .arg(FileUtil::toNativePath(m_currentFile)));
        return false;
    }
    setHint(QStringLiteral("已保存：%1").arg(FileUtil::toNativePath(m_currentFile)));
    LOG_INFO(QStringLiteral("保存文法文件：%1").arg(m_currentFile));
    return true;
}

/**
 * 功能：另存为新的文法文件。
 * 输入：无（弹出文件对话框）。
 * 输出：保存成功为 true。
 * 注意：默认文件名沿用当前文件名。
 */
bool GrammarEditorPanel::saveFileAs()
{
    const QString startDir = FileUtil::ensureDir(FileUtil::dataDir()) ? FileUtil::dataDir()
                                                                     : FileUtil::appDir();
    const QString startName = m_currentFile.isEmpty() ? QStringLiteral("grammar.txt")
                                                     : QFileInfo(m_currentFile).fileName();

    const QString path = QFileDialog::getSaveFileName(
        this, QStringLiteral("保存 BNF 文法文件"), startDir + QLatin1Char('/') + startName,
        QStringLiteral("文本文件 (*.txt);;所有文件 (*.*)"));
    if (path.isEmpty())
        return false;

    if (!FileUtil::writeText(path, m_editor->toPlainText(), false)) {
        QMessageBox::warning(this, QStringLiteral("保存失败"),
                             QStringLiteral("无法写入文件：\n%1").arg(FileUtil::toNativePath(path)));
        return false;
    }
    m_currentFile = path;
    updateFileLabel();
    setHint(QStringLiteral("已保存：%1").arg(FileUtil::toNativePath(path)));
    LOG_INFO(QStringLiteral("另存文法文件：%1").arg(path));
    return true;
}

// ---------------------------------------------------------------------------
// 内部工具
// ---------------------------------------------------------------------------

/** 功能：刷新当前文件提示标签。输入：无。输出：无。 */
void GrammarEditorPanel::updateFileLabel()
{
    if (m_currentFile.isEmpty())
        m_fileLabel->setText(QStringLiteral("当前文件：（未保存）"));
    else
        m_fileLabel->setText(
            QStringLiteral("当前文件：%1").arg(FileUtil::toNativePath(m_currentFile)));
}

/**
 * 功能：设置底部提示文本。
 * 输入：text —— 提示内容；isError —— 是否为错误提示。
 * 输出：无。
 * 注意：不使用模态对话框，保证程序化批量操作时不阻塞界面。
 */
void GrammarEditorPanel::setHint(const QString &text, bool isError)
{
    m_hintLabel->setText(text);
    m_hintLabel->setStyleSheet(isError ? QStringLiteral("color:#c01c28;")
                                       : QStringLiteral("color:#1a5fb4;"));
}

}  // namespace ccd
