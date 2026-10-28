/******************************************************************************
 * (1) 版权信息 : 编译原理课程设计 - 华南师范大学计算机学院
 * (2) 文件名称 : WordsEditorPanel.cpp
 *     标识符   : WordsEditorPanel
 *     摘要     : 单词规格编辑器面板的实现。左侧为规格文本编辑框，右侧为规格
 *                解析结果表；支持打开/保存/另存为以及载入随附语言数据。
 * (3) 版本号   : 1.0.0       作者/修改者 : <学号 姓名>   完成日期 : 2026-XX-XX
 * (4) 版本历史 : 1.0.0  2026-XX-XX  首次实现
 *****************************************************************************/

#include "gui/panels/WordsEditorPanel.h"

#include <QFileDialog>
#include <QFileInfo>
#include <QFont>
#include <QFontDatabase>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QMessageBox>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QSplitter>
#include <QTableWidget>
#include <QVBoxLayout>

#include "common/FileUtil.h"
#include "common/Logger.h"

namespace ccd {

/**
 * 功能：构造单词规格编辑器面板。
 * 输入：parent —— 父控件。
 * 输出：无。
 * 注意：构造时不载入任何文件，由 MainWindow 决定调用 loadLanguageWords()。
 */
WordsEditorPanel::WordsEditorPanel(QWidget *parent) : QWidget(parent)
{
    buildUi();
}

/**
 * 功能：构建界面控件与布局。
 * 输入：无。
 * 输出：无。
 * 注意：采用 QSplitter 左右布局：左=编辑框（占 3/5），右=解析结果表（占 2/5）。
 */
void WordsEditorPanel::buildUi()
{
    auto *rootLayout = new QVBoxLayout(this);
    rootLayout->setContentsMargins(8, 8, 8, 8);
    rootLayout->setSpacing(6);

    // ---- 顶部：说明 + 按钮条 ----
    auto *titleLabel = new QLabel(
        QStringLiteral("① 单词规格编辑器　（任务书要求 1.1 / 1.3）"), this);
    QFont titleFont = titleLabel->font();
    titleFont.setBold(true);
    titleFont.setPointSize(titleFont.pointSize() + 2);
    titleLabel->setFont(titleFont);
    rootLayout->addWidget(titleLabel);

    auto *buttonBar = new QHBoxLayout();
    buttonBar->setSpacing(6);

    m_openButton = new QPushButton(QStringLiteral("打开规格文件…"), this);
    m_saveButton = new QPushButton(QStringLiteral("保存"), this);
    m_saveAsButton = new QPushButton(QStringLiteral("另存为…"), this);
    m_parseButton = new QPushButton(QStringLiteral("解析规格"), this);
    m_pipelineButton = new QPushButton(QStringLiteral("执行完整词法流水线"), this);
    m_parseButton->setToolTip(
        QStringLiteral("解析编辑框中的正则表达式定义，生成单词规格表（名字/编码/类别）"));
    m_pipelineButton->setToolTip(QStringLiteral(
        "依次执行：正则→NFA→DFA→最小化DFA→生成词法分析源程序→编译→词法测试"));

    buttonBar->addWidget(m_openButton);
    buttonBar->addWidget(m_saveButton);
    buttonBar->addWidget(m_saveAsButton);
    buttonBar->addSpacing(12);
    buttonBar->addWidget(m_parseButton);
    buttonBar->addWidget(m_pipelineButton);
    buttonBar->addStretch(1);
    rootLayout->addLayout(buttonBar);

    m_fileLabel = new QLabel(QStringLiteral("当前文件：（未保存）"), this);
    m_fileLabel->setStyleSheet(QStringLiteral("color:#555555;"));
    rootLayout->addWidget(m_fileLabel);

    // ---- 中部：编辑框 + 结果表 ----
    auto *splitter = new QSplitter(Qt::Horizontal, this);

    m_editor = new QPlainTextEdit(splitter);
    m_editor->setFont(QFontDatabase::systemFont(QFontDatabase::FixedFont));
    m_editor->setTabStopDistance(4 * m_editor->fontMetrics().horizontalAdvance(QLatin1Char(' ')));
    m_editor->setPlaceholderText(QStringLiteral(
        "在此输入单词的正则表达式定义，例如：\n"
        "digit=[0-9]\n"
        "letter=[A-Za-z]\n"
        "Keyword103L=INT | DOUBLE | BOOL\n"
        "Comment_102=//(~eoln)*\n"
        "Num_100=(\\+|-)?digit+\n"
        "ID_101=letter(letter|digit)*\n"
        "Symbol_200L= \\+ | - | \\* | / | = | < | <= | <<"));
    splitter->addWidget(m_editor);

    m_resultTable = new QTableWidget(splitter);
    m_resultTable->setColumnCount(5);
    m_resultTable->setHorizontalHeaderLabels(QStringList{
        QStringLiteral("序号"), QStringLiteral("名称"), QStringLiteral("编码"),
        QStringLiteral("类别"), QStringLiteral("正则式")});
    m_resultTable->horizontalHeader()->setStretchLastSection(true);
    m_resultTable->verticalHeader()->setVisible(false);
    m_resultTable->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_resultTable->setSelectionBehavior(QAbstractItemView::SelectRows);
    splitter->addWidget(m_resultTable);

    splitter->setStretchFactor(0, 3);
    splitter->setStretchFactor(1, 2);
    rootLayout->addWidget(splitter, 1);

    // ---- 底部：状态提示 ----
    m_hintLabel = new QLabel(this);
    m_hintLabel->setWordWrap(true);
    setHint(QStringLiteral("提示：可直接编辑，或点击“打开规格文件…”载入已有的正则表达式文件。"));
    rootLayout->addWidget(m_hintLabel);

    // ---- 信号连接 ----
    connect(m_openButton, &QPushButton::clicked, this, &WordsEditorPanel::openFile);
    connect(m_saveButton, &QPushButton::clicked, this, [this] { saveFile(); });
    connect(m_saveAsButton, &QPushButton::clicked, this, [this] { saveFileAs(); });
    connect(m_parseButton, &QPushButton::clicked, this,
            &WordsEditorPanel::parseRequested);
    connect(m_pipelineButton, &QPushButton::clicked, this,
            &WordsEditorPanel::fullPipelineRequested);
}

// ---------------------------------------------------------------------------
// 文件操作
// ---------------------------------------------------------------------------

/**
 * 功能：载入随程序分发的指定语言单词规格文件。
 * 输入：id —— 语言标识（TINY / mini-C）。
 * 输出：无（更新编辑框与当前文件名）。
 * 注意：文件不存在时保留原内容并在提示区报错，不弹模态对话框，避免启动时打扰。
 */
void WordsEditorPanel::loadLanguageWords(LanguageId id)
{
    const LanguageConfig &cfg = LanguageConfigRegistry::get(id);
    const QString path = FileUtil::locateDataFile(cfg.wordsFile);

    bool ok = false;
    QString encoding;
    const QString content = FileUtil::readText(path, &ok, &encoding);
    if (!ok) {
        setHint(QStringLiteral("无法读取单词规格文件：%1").arg(FileUtil::toNativePath(path)),
                true);
        LOG_ERROR(QStringLiteral("读取单词规格文件失败：%1").arg(path));
        return;
    }

    m_currentFile = path;
    m_editor->setPlainText(content);
    updateFileLabel();
    setHint(QStringLiteral("已载入 %1 的单词规格（编码 %2，%3 行）。")
                .arg(cfg.displayName, encoding)
                .arg(content.count(QLatin1Char('\n')) + 1));
    LOG_INFO(QStringLiteral("载入单词规格文件：%1").arg(path));
}

/** 功能：取编辑框文本。输入：无。输出：全文（LF 换行）。 */
QString WordsEditorPanel::text() const
{
    return m_editor->toPlainText();
}

/** 功能：设置编辑框文本。输入：text —— 新内容。输出：无。 */
void WordsEditorPanel::setText(const QString &text)
{
    m_editor->setPlainText(text);
}

/**
 * 功能：打开规格文件。
 * 输入：无（弹出文件对话框）。
 * 输出：无。
 * 注意：默认目录为随附数据目录；读取编码由 FileUtil 自动判定。
 */
void WordsEditorPanel::openFile()
{
    const QString startDir = FileUtil::ensureDir(FileUtil::dataDir()) ? FileUtil::dataDir()
                                                                     : FileUtil::appDir();
    const QString path = QFileDialog::getOpenFileName(
        this, QStringLiteral("打开单词规格文件"), startDir,
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
    LOG_INFO(QStringLiteral("打开单词规格文件：%1").arg(path));
}

/**
 * 功能：保存到当前文件；无当前文件时转为另存为。
 * 输入：无。
 * 输出：保存成功为 true。
 * 注意：写出编码固定为 UTF-8（不带 BOM），便于后续解析。
 */
bool WordsEditorPanel::saveFile()
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
    LOG_INFO(QStringLiteral("保存单词规格文件：%1").arg(m_currentFile));
    return true;
}

/**
 * 功能：另存为新的规格文件。
 * 输入：无（弹出文件对话框）。
 * 输出：保存成功为 true。
 * 注意：默认文件名沿用当前文件名，默认目录为数据目录。
 */
bool WordsEditorPanel::saveFileAs()
{
    const QString startDir = FileUtil::ensureDir(FileUtil::dataDir()) ? FileUtil::dataDir()
                                                                     : FileUtil::appDir();
    const QString startName =
        m_currentFile.isEmpty() ? QStringLiteral("words.txt") : QFileInfo(m_currentFile).fileName();

    const QString path = QFileDialog::getSaveFileName(
        this, QStringLiteral("保存单词规格文件"), startDir + QLatin1Char('/') + startName,
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
    LOG_INFO(QStringLiteral("另存单词规格文件：%1").arg(path));
    return true;
}

// ---------------------------------------------------------------------------
// 结果展示
// ---------------------------------------------------------------------------

/**
 * 功能：显示规格解析结果表。
 * 输入：headers —— 列标题；rows —— 每行文本；summary —— 底部汇总提示。
 * 输出：无。
 * 注意：由后续阶段的规格解析器调用；列数不匹配时按 headers 的列数补齐空串。
 */
void WordsEditorPanel::showParseResult(const QStringList &headers,
                                       const QList<QStringList> &rows,
                                       const QString &summary)
{
    m_resultTable->clear();
    m_resultTable->setColumnCount(headers.size());
    m_resultTable->setHorizontalHeaderLabels(headers);
    m_resultTable->setRowCount(rows.size());

    for (int r = 0; r < rows.size(); ++r) {
        const QStringList &row = rows.at(r);
        for (int c = 0; c < headers.size(); ++c) {
            const QString cell = (c < row.size()) ? row.at(c) : QString();
            m_resultTable->setItem(r, c, new QTableWidgetItem(cell));
        }
    }
    m_resultTable->resizeColumnsToContents();
    setHint(summary);
}

// ---------------------------------------------------------------------------
// 内部工具
// ---------------------------------------------------------------------------

/** 功能：刷新当前文件提示标签。输入：无。输出：无。 */
void WordsEditorPanel::updateFileLabel()
{
    if (m_currentFile.isEmpty())
        m_fileLabel->setText(QStringLiteral("当前文件：（未保存）"));
    else
        m_fileLabel->setText(
            QStringLiteral("当前文件：%1").arg(FileUtil::toNativePath(m_currentFile)));
}

/**
 * 功能：设置底部提示文本。
 * 输入：text —— 提示内容；isError —— 是否为错误提示（红色显示）。
 * 输出：无。
 * 注意：不使用模态对话框，保证批量操作时不阻塞界面。
 */
void WordsEditorPanel::setHint(const QString &text, bool isError)
{
    m_hintLabel->setText(text);
    m_hintLabel->setStyleSheet(isError ? QStringLiteral("color:#c01c28;")
                                       : QStringLiteral("color:#1a5fb4;"));
}

}  // namespace ccd
