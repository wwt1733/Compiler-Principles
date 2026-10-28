/******************************************************************************
 * (1) 版权信息 : 编译原理课程设计 - 华南师范大学计算机学院
 * (2) 文件名称 : SyntaxProcessPanel.cpp
 *     标识符   : SyntaxProcessPanel
 *     摘要     : "⑬ 语法分析过程"功能面板的实现（任务书要求 2.6）。
 * (3) 版本号   : 1.0.0       作者/修改者 : <学号 姓名>   完成日期 : 2026-XX-XX
 * (4) 版本历史 : 1.0.0  2026-XX-XX  首次实现（阶段 6 界面）
 *****************************************************************************/

#include "gui/panels/SyntaxProcessPanel.h"

#include <QFileDialog>
#include <QFileInfo>
#include <QFontDatabase>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QLineEdit>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QSpinBox>
#include <QSplitter>
#include <QTableWidget>
#include <QVBoxLayout>

#include "common/FileUtil.h"

namespace ccd {

/**
 * 功能：构造面板。
 * 输入：parent —— 父控件。
 * 输出：无。
 * 注意：构造函数只建立控件与信号连接，不读取任何数据文件 —— 数据路径由
 *       主窗口注入（`.lex` 的默认位置取决于词法阶段刚才把文件写到了哪里）。
 */
SyntaxProcessPanel::SyntaxProcessPanel(QWidget *parent)
    : QWidget(parent)
{
    buildUi();
}

/**
 * 功能：构建界面控件与布局。
 * 输入：无。输出：无。
 * 注意：过程表用 `QTableWidget`，因为要按"语句号变化"给分隔行上色；
 *       过程记录可能上千行，因此表格不做自动换行、固定行高，滚动更快。
 */
void SyntaxProcessPanel::buildUi()
{
    auto *root = new QVBoxLayout(this);

    // ---- 顶部：文件与操作 ----
    auto *top = new QHBoxLayout;
    top->addWidget(new QLabel(QStringLiteral("单词编码文件："), this));
    m_pathEdit = new QLineEdit(this);
    m_pathEdit->setPlaceholderText(
        QStringLiteral("任务一生成的 .lex 文件（默认 output/lexbuild/tiny.lex）"));
    top->addWidget(m_pathEdit, 1);

    m_browseButton = new QPushButton(QStringLiteral("浏览…"), this);
    m_analyzeButton = new QPushButton(QStringLiteral("开始语法分析"), this);
    m_exportButton = new QPushButton(QStringLiteral("导出过程"), this);
    top->addWidget(m_browseButton);
    top->addWidget(m_analyzeButton);
    top->addWidget(m_exportButton);
    root->addLayout(top);

    // ---- 第二行：显示条数与摘要 ----
    auto *second = new QHBoxLayout;
    second->addWidget(new QLabel(QStringLiteral("显示步数上限："), this));
    m_limitSpin = new QSpinBox(this);
    m_limitSpin->setRange(0, 100000);
    m_limitSpin->setValue(300);
    m_limitSpin->setSpecialValueText(QStringLiteral("全部"));
    m_limitSpin->setToolTip(QStringLiteral("0 表示全部显示；TINY 样例约 300 步"));
    second->addWidget(m_limitSpin);
    m_summaryLabel = new QLabel(QStringLiteral("（尚未分析）"), this);
    m_summaryLabel->setWordWrap(true);
    second->addWidget(m_summaryLabel, 1);
    root->addLayout(second);

    // ---- 主体：过程表 / 语句片段 / 错误明细 ----
    m_stepTable = new QTableWidget(this);
    m_stepTable->setColumnCount(5);
    m_stepTable->setHorizontalHeaderLabels(
        {QStringLiteral("步"), QStringLiteral("状态栈"),
         QStringLiteral("符号栈"), QStringLiteral("剩余输入"),
         QStringLiteral("动作")});
    m_stepTable->horizontalHeader()->setStretchLastSection(true);
    m_stepTable->verticalHeader()->setVisible(false);
    m_stepTable->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_stepTable->setWordWrap(false);
    m_stepTable->setFont(QFontDatabase::systemFont(QFontDatabase::FixedFont));

    m_statementView = new QPlainTextEdit(this);
    m_statementView->setReadOnly(true);
    m_statementView->setFont(QFontDatabase::systemFont(QFontDatabase::FixedFont));
    m_statementView->setPlaceholderText(
        QStringLiteral("按顶层语句切出的输入片段会显示在这里。"));

    m_errorView = new QPlainTextEdit(this);
    m_errorView->setReadOnly(true);
    m_errorView->setFont(QFontDatabase::systemFont(QFontDatabase::FixedFont));
    m_errorView->setPlaceholderText(
        QStringLiteral("若语法分析失败，错误明细（行号 / 出错符号 / 期望符号集合）"
                       "会显示在这里。"));

    auto *lower = new QSplitter(Qt::Horizontal, this);
    auto *stmtBox = new QGroupBox(QStringLiteral("按语句分段"), this);
    auto *stmtLayout = new QVBoxLayout(stmtBox);
    stmtLayout->addWidget(m_statementView);
    auto *errBox = new QGroupBox(QStringLiteral("错误明细"), this);
    auto *errLayout = new QVBoxLayout(errBox);
    errLayout->addWidget(m_errorView);
    lower->addWidget(stmtBox);
    lower->addWidget(errBox);

    auto *splitter = new QSplitter(Qt::Vertical, this);
    auto *stepBox = new QGroupBox(QStringLiteral("分析过程"), this);
    auto *stepLayout = new QVBoxLayout(stepBox);
    stepLayout->addWidget(m_stepTable);
    splitter->addWidget(stepBox);
    splitter->addWidget(lower);
    splitter->setStretchFactor(0, 3);
    splitter->setStretchFactor(1, 1);
    root->addWidget(splitter, 1);

    connect(m_browseButton, &QPushButton::clicked, this,
            &SyntaxProcessPanel::onBrowseRequested);
    connect(m_analyzeButton, &QPushButton::clicked, this,
            &SyntaxProcessPanel::onAnalyzeRequested);
    connect(m_exportButton, &QPushButton::clicked, this,
            &SyntaxProcessPanel::onExportRequested);
    connect(m_limitSpin, QOverload<int>::of(&QSpinBox::valueChanged), this,
            &SyntaxProcessPanel::onLimitChanged);
}

/** 功能：注入分析表。输入：table。输出：无。 */
void SyntaxProcessPanel::setTable(const ParseTable &table)
{
    m_table = table;
}

/** 功能：注入语义动作。输入：semantics。输出：无。 */
void SyntaxProcessPanel::setSemantics(const SemanticsActionSet &semantics)
{
    m_sem = semantics;
}

/**
 * 功能：回填 .lex 文件路径。
 * 输入：path。输出：无。
 * 注意：仅在输入框为空或被主窗口自动定位时调用；用户手动改过的内容不覆盖 ——
 *       否则每次自动分析都会把用户填的路径冲掉，体验很差。
 */
void SyntaxProcessPanel::setLexPath(const QString &path)
{
    if (m_pathEdit->text().trimmed().isEmpty() || m_pathEdit->property("autoFilled").toBool()) {
        m_pathEdit->setText(path);
        m_pathEdit->setProperty("autoFilled", true);
    }
}

/**
 * 功能：清空全部显示。
 * 输入：无。输出：无。
 * 注意：只清"显示"，不清已注入的分析表与语义动作 —— 切换语言后主窗口会重新注入。
 */
void SyntaxProcessPanel::clearAll()
{
    m_result = ParseResult();
    m_lexPath.clear();
    m_hasResult = false;
    m_stepTable->setRowCount(0);
    m_statementView->clear();
    m_errorView->clear();
    m_summaryLabel->setText(QStringLiteral("（尚未分析）"));
}

/**
 * 功能：推入一次分析结果并刷新显示。
 * 输入：result —— 分析结果；lexPath —— 本次分析的 .lex 路径。
 * 输出：无。
 * 注意：过程表按当前"显示步数上限"截断，但**摘要与错误明细始终是全量** ——
 *       否则把上限调小就看不到后面的错误，容易误判成"分析通过"。
 */
void SyntaxProcessPanel::showParseResult(const ParseResult &result, const QString &lexPath)
{
    m_result = result;
    m_lexPath = lexPath;
    m_hasResult = true;

    m_summaryLabel->setText(QStringLiteral("文件：%1　|　%2")
                                .arg(lexPath.isEmpty() ? QStringLiteral("(未指定)") : lexPath,
                                     result.summary()));
    refreshSteps();

    // 按语句分段的输入片段
    if (result.statements.isEmpty()) {
        m_statementView->setPlainText(
            QStringLiteral("（未识别出顶层语句；可能是输入为空或语法错误过早终止）"));
    } else {
        QStringList out;
        for (int i = 0; i < result.statements.size(); ++i) {
            out << QStringLiteral("第 %1 条语句：%2")
                       .arg(i + 1)
                       .arg(result.statements.at(i).isEmpty()
                                ? QStringLiteral("(空)")
                                : result.statements.at(i));
        }
        m_statementView->setPlainText(out.join(QLatin1Char('\n')));
    }

    m_errorView->setPlainText(result.errorsText());
}

/**
 * 功能：按 m_result 重画过程表格。
 * 输入：无。输出：无。
 * 注意：语句号变化时插入一条**跨列的分隔行**并给它上底色，这样"每条语句
 *       各自的步骤区间"在长表里也能一眼找到；分隔行本身不属于任何一步，
 *       因此不写入 `result.steps`，只影响显示。
 */
void SyntaxProcessPanel::refreshSteps()
{
    m_stepTable->setRowCount(0);
    if (!m_hasResult)
        return;

    const int limit = m_limitSpin->value();
    const int total = m_result.steps.size();
    const int show = (limit <= 0) ? total : qMin(limit, total);

    // 先算需要多少行（含分隔行）
    struct Row {
        bool separator = false;
        int index = -1;
        int statement = 0;
    };
    QVector<Row> rows;
    int lastStatement = -1;
    for (int i = 0; i < show; ++i) {
        const ParseStep &s = m_result.steps.at(i);
        if (s.statement != lastStatement) {
            lastStatement = s.statement;
            Row r;
            r.separator = true;
            r.statement = s.statement;
            rows.append(r);
        }
        Row r;
        r.index = i;
        rows.append(r);
    }

    m_stepTable->setRowCount(rows.size());
    for (int row = 0; row < rows.size(); ++row) {
        const Row &r = rows.at(row);
        if (r.separator) {
            auto *item = new QTableWidgetItem(
                QStringLiteral("──────── 第 %1 条语句 ────────").arg(r.statement));
            item->setBackground(QColor(0xE0, 0xE7, 0xFF));
            QFont f = item->font();
            f.setBold(true);
            item->setFont(f);
            m_stepTable->setItem(row, 0, item);
            for (int c = 1; c < m_stepTable->columnCount(); ++c)
                m_stepTable->setItem(row, c, new QTableWidgetItem(QString()));
            continue;
        }

        const ParseStep &s = m_result.steps.at(r.index);
        const QStringList cells{
            QString::number(s.index),
            s.stateStack.isEmpty() ? QString()
                                   : QStringLiteral("[%1]")
                                         .arg([&] {
                                             QStringList t;
                                             for (int v : s.stateStack)
                                                 t << QString::number(v);
                                             return t.join(QLatin1Char(' '));
                                         }()),
            s.symbolStack.join(QLatin1Char(' ')),
            // 剩余输入：从当前向前看符号起，最多列 10 个，多了用 … 收尾
            [&] {
                QStringList rest;
                for (int i = s.inputPos; i < m_result.inputSymbols.size(); ++i) {
                    if (rest.size() >= 10) {
                        rest << QStringLiteral("…");
                        break;
                    }
                    rest << m_result.inputSymbols.at(i);
                }
                return rest.join(QLatin1Char(' '));
            }(),
            s.action};
        for (int c = 0; c < cells.size() && c < m_stepTable->columnCount(); ++c) {
            auto *item = new QTableWidgetItem(cells.at(c));
            if (s.isError) {
                item->setBackground(QColor(0xFE, 0xE2, 0xE2));
                item->setForeground(QColor(0xB9, 0x1C, 0x1C));
            }
            m_stepTable->setItem(row, c, item);
        }
    }

    m_stepTable->resizeColumnsToContents();
    if (m_stepTable->columnCount() > 0)
        m_stepTable->horizontalHeader()->setStretchLastSection(true);
}

/** 功能：选择 .lex 文件。输入：无。输出：无。 */
void SyntaxProcessPanel::onBrowseRequested()
{
    const QString start = m_pathEdit->text().isEmpty()
                              ? FileUtil::outputDir()
                              : QFileInfo(m_pathEdit->text()).absolutePath();
    const QString path = QFileDialog::getOpenFileName(
        this, QStringLiteral("选择单词编码文件"), start,
        QStringLiteral("单词编码文件 (*.lex);;所有文件 (*)"));
    if (!path.isEmpty())
        m_pathEdit->setText(path);
}

/**
 * 功能：请求主窗口执行一次语法分析。
 * 输入：无。输出：无。
 * 注意：本面板不自己读文件、不自己分析 —— 它只把路径发出去，由主窗口统一
 *       编排"读 .lex → 分析 → 建树 → 分发到⑬⑭"，保证两个页签看到的是同一次
 *       分析的结果（各自算一遍会出现"过程显示通过、树却是旧的"这种不一致）。
 */
void SyntaxProcessPanel::onAnalyzeRequested()
{
    emit analyzeRequested(m_pathEdit->text().trimmed());
}

/** 功能：导出分析过程。输入：无。输出：无。 */
void SyntaxProcessPanel::onExportRequested()
{
    if (!m_hasResult) {
        m_summaryLabel->setText(QStringLiteral("（还没有分析结果可导出）"));
        return;
    }

    const QString path = QFileDialog::getSaveFileName(
        this, QStringLiteral("导出语法分析过程"), FileUtil::outputDir() + QStringLiteral("/syntax_trace.txt"),
        QStringLiteral("文本文件 (*.txt)"));
    if (path.isEmpty())
        return;

    QString text;
    text += QStringLiteral("语法分析过程\n");
    text += QStringLiteral("文件：%1\n").arg(m_lexPath);
    text += m_result.summary() + QLatin1Char('\n');
    text += QStringLiteral("────────────────────────────────\n");
    text += m_result.stepsText(0);
    text += QStringLiteral("\n────────────────────────────────\n");
    text += m_result.errorsText();

    QString error;
    if (FileUtil::writeText(path, text, false))
        m_summaryLabel->setText(QStringLiteral("已导出：%1").arg(path));
    else
        m_summaryLabel->setText(QStringLiteral("导出失败：%1").arg(path));
}

/** 功能：显示条数变化。输入：value。输出：无。 */
void SyntaxProcessPanel::onLimitChanged(int value)
{
    Q_UNUSED(value);
    refreshSteps();
}

/** 功能：过程表格行数。输入：无。输出：行数。 */
int SyntaxProcessPanel::stepRowCount() const
{
    return m_stepTable->rowCount();
}

/** 功能：某单元格文本。输入：row、column。输出：文本。 */
QString SyntaxProcessPanel::stepCellText(int row, int column) const
{
    const QTableWidgetItem *item = m_stepTable->item(row, column);
    return item == nullptr ? QString() : item->text();
}

/** 功能：摘要标签文本。输入：无。输出：文本。 */
QString SyntaxProcessPanel::summaryText() const
{
    return m_summaryLabel->text();
}

/** 功能：错误明细文本。输入：无。输出：文本。 */
QString SyntaxProcessPanel::errorsText() const
{
    return m_errorView->toPlainText();
}

}  // namespace ccd
