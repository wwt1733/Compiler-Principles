/******************************************************************************
 * (1) 版权信息 : 编译原理课程设计 - 华南师范大学计算机学院
 * (2) 文件名称 : GrammarSimplifyPanel.cpp
 *     标识符   : GrammarSimplifyPanel
 *     摘要     : "⑧ 文法化简结果"功能面板的实现（任务书要求 2.2）。
 * (3) 版本号   : 1.0.0       作者/修改者 : <学号 姓名>   完成日期 : 2026-XX-XX
 * (4) 版本历史 : 1.0.0  2026-XX-XX  首次实现（阶段 4 界面）
 *****************************************************************************/

#include "gui/panels/GrammarSimplifyPanel.h"

#include <QHeaderView>
#include <QLabel>
#include <QPlainTextEdit>
#include <QTabWidget>
#include <QTableWidget>
#include <QTableWidgetItem>
#include <QVBoxLayout>

namespace ccd {

/** 功能：构造面板。输入：parent。输出：无。 */
GrammarSimplifyPanel::GrammarSimplifyPanel(QWidget *parent)
    : QWidget(parent)
{
    buildUi();
}

/**
 * 功能：构建界面控件与布局。
 * 输入：无。输出：无。
 * 注意：把"化简后文法""不可推导""不可达""过程"分成四个页，
 *       而不是挤在一个文本里 —— 报告里要分节引用这几类信息，
 *       分开后每页都能整段复制。
 */
void GrammarSimplifyPanel::buildUi()
{
    auto *root = new QVBoxLayout(this);

    m_summaryLabel = new QLabel(this);
    m_summaryLabel->setWordWrap(true);
    root->addWidget(m_summaryLabel);

    m_tabs = new QTabWidget(this);

    m_keptView = new QPlainTextEdit(m_tabs);
    m_keptView->setReadOnly(true);
    m_keptView->setLineWrapMode(QPlainTextEdit::NoWrap);
    m_tabs->addTab(m_keptView, QStringLiteral("化简后的文法"));

    m_nonProductiveView = new QPlainTextEdit(m_tabs);
    m_nonProductiveView->setReadOnly(true);
    m_tabs->addTab(m_nonProductiveView, QStringLiteral("删除的不可推导规则"));

    m_unreachableView = new QPlainTextEdit(m_tabs);
    m_unreachableView->setReadOnly(true);
    m_tabs->addTab(m_unreachableView, QStringLiteral("删除的不可达规则"));

    m_stepsView = new QPlainTextEdit(m_tabs);
    m_stepsView->setReadOnly(true);
    m_tabs->addTab(m_stepsView, QStringLiteral("化简过程"));

    m_symbolTable = new QTableWidget(m_tabs);
    m_symbolTable->setColumnCount(3);
    m_symbolTable->setHorizontalHeaderLabels(
        {QStringLiteral("编码"), QStringLiteral("文法中的终结符"), QStringLiteral("类型")});
    m_symbolTable->horizontalHeader()->setStretchLastSection(true);
    m_symbolTable->verticalHeader()->setVisible(false);
    m_symbolTable->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_symbolTable->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_tabs->addTab(m_symbolTable, QStringLiteral("符号对照表"));

    root->addWidget(m_tabs, 1);
}

/**
 * 功能：把符号列表格式化为多行文本。
 * 输入：symbols —— 符号列表；emptyHint —— 列表为空时显示的提示。
 * 输出：文本。
 * 注意：列表为空时**必须**给出明确的提示文字。若直接返回空串，
 *       使用者会看到一个空白页，无法判断"没有删除任何规则"与"功能没执行"
 *       这两种截然不同的情况。
 */
QString GrammarSimplifyPanel::formatSymbolList(const QStringList &symbols, const QString &emptyHint)
{
    if (symbols.isEmpty())
        return emptyHint;
    return symbols.join(QLatin1Char('\n'));
}

/**
 * 功能：注入原始文法与化简结果并刷新显示。
 * 输入：original、result。
 * 输出：无。
 * 注意：化简结果的 `kept` 是**未增广**的文法（化简必须在增广之前做，
 *       否则增广产生的 S' 因从开始符号不可达会被误删）。本面板显示的正是
 *       这份未增广的化简结果 —— 增广是后续 LR 构造的内部步骤，不属于"化简结果"。
 */
void GrammarSimplifyPanel::showSimplifyResult(const Grammar &original, const SimplifyResult &result)
{
    m_summaryLabel->setText(
        QStringLiteral("原始文法：%1 条产生式（%2 个非终结符、%3 个终结符）　→　"
                       "化简后：%4 条产生式（%5 个非终结符、%6 个终结符）　｜　%7")
            .arg(original.productionCount())
            .arg(original.nonterminals().size())
            .arg(original.terminals().size())
            .arg(result.kept.productionCount())
            .arg(result.kept.nonterminals().size())
            .arg(result.kept.terminals().size())
            .arg(result.changed() ? QStringLiteral("文法已被化简")
                                  : QStringLiteral("文法无需化简（无不可达、无不可推导规则）")));

    m_keptView->setPlainText(result.kept.toText());

    m_nonProductiveView->setPlainText(formatSymbolList(
        result.nonProductiveRemoved,
        QStringLiteral("没有不可推导的非终结符。\n\n"
                       "说明：不可推导（non-productive）指该非终结符无论怎样推导，"
                       "都无法推出任何**全由终结符组成的句子**。\n"
                       "例如 `A -> A B` 且 B 也推不出终结符串时，A 就是不可推导的。")));

    m_unreachableView->setPlainText(formatSymbolList(
        result.unreachableRemoved,
        QStringLiteral("没有不可达的非终结符。\n\n"
                       "说明：不可达（unreachable）指从开始符号出发，"
                       "无论怎样推导都无法到达该非终结符。\n"
                       "常见的不可达来源是文法里写错名字的规则，"
                       "或改写后遗留的旧规则。")));

    m_stepsView->setPlainText(result.steps.join(QLatin1Char('\n')));

    refreshSymbolTable(original);
}

/**
 * 功能：刷新符号对照表。
 * 输入：grammar —— 用哪份文法取终结符与编码表。
 * 输出：无。
 * 注意：这张表是本项目的一个关键接合点 —— 文法里写的是**符号名**
 *       （如 `LTEQ`、`ASSIGN`），而词法分析程序输出的是**数字编码**
 *       （如 `207`、`215`）。分析器必须靠 `terminalNames()` 把两者对上，
 *       界面上把这张对照表露出来，既便于核对，也是报告里说明设计的一手材料。
 */
void GrammarSimplifyPanel::refreshSymbolTable(const Grammar &grammar)
{
    const QStringList terminals = grammar.terminals();
    const QMap<int, QString> names = grammar.terminalNames();

    // 建立 name -> code 的反查表
    QMap<QString, int> codeOfName;
    for (auto it = names.constBegin(); it != names.constEnd(); ++it)
        codeOfName.insert(it.value(), it.key());

    m_symbolTable->setRowCount(terminals.size());

    for (int i = 0; i < terminals.size(); ++i) {
        const QString &name = terminals.at(i);
        const bool hasCode = codeOfName.contains(name);
        const QString codeText = hasCode ? QString::number(codeOfName.value(name))
                                         : QStringLiteral("（无）");
        const QString typeText = hasCode ? QStringLiteral("终结符")
                                         : QStringLiteral("未登记（请检查编码表）");

        const QStringList cells = {codeText, name, typeText};
        for (int c = 0; c < cells.size(); ++c)
            m_symbolTable->setItem(i, c, new QTableWidgetItem(cells.at(c)));
    }

    m_symbolTable->resizeColumnsToContents();
}

/** 功能：清空全部显示。输入：无。输出：无。 */
void GrammarSimplifyPanel::clearAll()
{
    m_summaryLabel->clear();
    m_keptView->clear();
    m_nonProductiveView->clear();
    m_unreachableView->clear();
    m_stepsView->clear();
    m_symbolTable->setRowCount(0);
}

/** 功能：摘要标签文本（自测用）。输入：无。输出：文本。 */
QString GrammarSimplifyPanel::summaryText() const
{
    return m_summaryLabel->text();
}

/** 功能：化简后文法页文本（自测用）。输入：无。输出：文本。 */
QString GrammarSimplifyPanel::keptGrammarText() const
{
    return m_keptView->toPlainText();
}

/** 功能：化简过程页文本（自测用）。输入：无。输出：文本。 */
QString GrammarSimplifyPanel::stepsText() const
{
    return m_stepsView->toPlainText();
}

/** 功能：符号对照表行数（自测用）。输入：无。输出：行数。 */
int GrammarSimplifyPanel::symbolRowCount() const
{
    return m_symbolTable->rowCount();
}

}  // namespace ccd
