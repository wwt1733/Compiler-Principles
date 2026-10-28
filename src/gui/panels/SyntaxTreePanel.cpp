/******************************************************************************
 * (1) 版权信息 : 编译原理课程设计 - 华南师范大学计算机学院
 * (2) 文件名称 : SyntaxTreePanel.cpp
 *     标识符   : SyntaxTreePanel
 *     摘要     : "⑭ 语法树"功能面板的实现（任务书要求 2.7 / 2.8）。
 * (3) 版本号   : 1.0.0       作者/修改者 : <学号 姓名>   完成日期 : 2026-XX-XX
 * (4) 版本历史 : 1.0.0  2026-XX-XX  首次实现（阶段 6 界面）
 *****************************************************************************/

#include "gui/panels/SyntaxTreePanel.h"

#include <QFontDatabase>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QScrollBar>
#include <QSlider>
#include <QSplitter>
#include <QTabWidget>
#include <QTableWidget>
#include <QVBoxLayout>

namespace ccd {

/**
 * 功能：构造面板。
 * 输入：parent —— 父控件。
 * 输出：无。
 * 注意：构造函数只建立控件，不载入任何数据 —— 文法与语义动作由
 *       `showSemantics()` 注入，树由 `showTree()` 注入。
 */
SyntaxTreePanel::SyntaxTreePanel(QWidget *parent)
    : QWidget(parent)
{
    buildUi();
}

/**
 * 功能：构建界面控件与布局。
 * 输入：无。输出：无。
 * 注意：上半部分是"语法树结构定义"表（能看清用户写的定义被如何判读），
 *       下半部分是三种视图形式（图形 / 缩进文本 / 括号表示 / DOT）。
 *       图形视图放在 QScrollArea 里，配合缩放滑块即可浏览大树。
 */
void SyntaxTreePanel::buildUi()
{
    auto *root = new QVBoxLayout(this);

    auto *top = new QHBoxLayout;
    top->addWidget(new QLabel(QStringLiteral("缩放："), this));
    m_zoomSlider = new QSlider(Qt::Horizontal, this);
    // 范围必须能装下"适应窗口"所需的比例：一棵 80 多个结点的树要缩到 10% 上下
    // 才看得全，所以下限取 2%；上限 800% 用于放大看单个标签。
    m_zoomSlider->setRange(2, 800);   // 2% ~ 800%
    m_zoomSlider->setValue(100);
    m_zoomSlider->setPageStep(10);
    m_zoomSlider->setFixedWidth(200);
    top->addWidget(m_zoomSlider);

    auto *fitButton = new QPushButton(QStringLiteral("适应窗口"), this);
    auto *resetButton = new QPushButton(QStringLiteral("100%"), this);
    auto *exportButton = new QPushButton(QStringLiteral("导出语法树…"), this);
    top->addWidget(fitButton);
    top->addWidget(resetButton);
    top->addWidget(exportButton);
    top->addStretch(1);
    m_summaryLabel = new QLabel(QStringLiteral("（还没有语法树）"), this);
    m_summaryLabel->setWordWrap(true);
    top->addWidget(m_summaryLabel, 2);
    root->addLayout(top);

    // 操作提示：滚轮缩放 / 拖动平移
    auto *hintLabel = new QLabel(
        QStringLiteral("浏览方式：滚轮 = 以光标为中心缩放　·　按住左键拖动 = 平移　·　"
                       "「适应窗口」= 一键看全貌（默认已自动适应）"),
        this);
    hintLabel->setStyleSheet(QStringLiteral("color:#555555;"));
    hintLabel->setWordWrap(true);
    root->addWidget(hintLabel);

    // ---- 视图形式 ----
    m_formTabs = new QTabWidget(this);

    // 图形视图本身就是 QGraphicsView（自带滚动与拖动平移），
    // 不再需要外层 QScrollArea —— 早期套了 QScrollArea(widgetResizable=true)，
    // 视图尺寸被逻辑单位算小了，整棵树就挤在左上角。
    m_graphView = new SyntaxTreeView(this);
    m_formTabs->addTab(m_graphView, QStringLiteral("图形"));

    m_textView = new QPlainTextEdit(this);
    m_textView->setReadOnly(true);
    m_textView->setFont(QFontDatabase::systemFont(QFontDatabase::FixedFont));
    m_formTabs->addTab(m_textView, QStringLiteral("缩进文本 / 括号 / DOT"));

    // ---- 语法树结构定义表 ----
    auto *semBox = new QGroupBox(QStringLiteral("语法树结构定义（语义动作）"), this);
    auto *semLayout = new QVBoxLayout(semBox);
    m_semanticsTable = new QTableWidget(this);
    m_semanticsTable->setColumnCount(4);
    m_semanticsTable->setHorizontalHeaderLabels(
        {QStringLiteral("产生式号"), QStringLiteral("产生式"),
         QStringLiteral("原始动作写法"), QStringLiteral("判读结果")});
    m_semanticsTable->horizontalHeader()->setStretchLastSection(true);
    m_semanticsTable->verticalHeader()->setVisible(false);
    m_semanticsTable->setEditTriggers(QAbstractItemView::NoEditTriggers);
    semLayout->addWidget(m_semanticsTable);

    auto *splitter = new QSplitter(Qt::Vertical, this);
    splitter->addWidget(m_formTabs);
    splitter->addWidget(semBox);
    splitter->setStretchFactor(0, 3);
    splitter->setStretchFactor(1, 2);
    root->addWidget(splitter, 1);

    connect(m_zoomSlider, &QSlider::valueChanged, this, &SyntaxTreePanel::onZoomChanged);
    connect(m_formTabs, &QTabWidget::currentChanged, this, &SyntaxTreePanel::onFormChanged);
    // 视图自己（滚轮/适应窗口）改了比例时回填滑块，两边永远一致
    connect(m_graphView, &SyntaxTreeView::zoomChanged, this, &SyntaxTreePanel::onViewZoomChanged);
    connect(fitButton, &QPushButton::clicked, this, [this] {
        m_graphView->fitToWindow();
        syncSliderToView();
    });
    connect(resetButton, &QPushButton::clicked, this, [this] {
        m_graphView->setZoom(1.0);
        syncSliderToView();
    });
    connect(exportButton, &QPushButton::clicked, this, &SyntaxTreePanel::exportRequested);
}

/**
 * 功能：注入文法与语义动作，刷新结构定义表。
 * 输入：grammar、semantics。输出：无。
 * 注意：这张表是需求 2.7「每条文法规则的语法树结构由用户定义」的可视化解释 ——
 *       左边的"原始写法"逐字回读用户写的内容，右边的"判读结果"用中文说明
 *       "建几个结点、标签取谁、谁是最终结果"。两者并排放在一起，
 *       用户一旦写错（例如把单链写成 `L`），能立刻从表里看出来。
 */
void SyntaxTreePanel::showSemantics(const Grammar &grammar, const SemanticsActionSet &semantics)
{
    QStringList headers;
    QList<QStringList> rows;
    semantics.buildSummaryTable(grammar, headers, rows);

    m_semanticsTable->setColumnCount(headers.size());
    m_semanticsTable->setHorizontalHeaderLabels(headers);
    m_semanticsTable->setRowCount(rows.size());
    for (int r = 0; r < rows.size(); ++r) {
        const QStringList &row = rows.at(r);
        for (int c = 0; c < row.size() && c < m_semanticsTable->columnCount(); ++c)
            m_semanticsTable->setItem(r, c, new QTableWidgetItem(row.at(c)));
    }
    m_semanticsTable->resizeColumnsToContents();
    m_semanticsTable->horizontalHeader()->setStretchLastSection(true);
}

/** 功能：清空全部显示。输入：无。输出：无。 */
void SyntaxTreePanel::clearAll()
{
    m_tree = SyntaxTreeData();
    m_title.clear();
    m_hasTree = false;
    m_graphView->clearTree();
    m_textView->clear();
    m_semanticsTable->setRowCount(0);
    m_summaryLabel->setText(QStringLiteral("（还没有语法树）"));
}

/**
 * 功能：推入一棵语法树并刷新显示。
 * 输入：tree、title。输出：无。
 * 注意：同时把树的规模写进摘要 —— "结点数 / 叶结点数 / 从根可达数"三个数字
 *       放在一起，一旦语义动作写错导致树断开（可达数 < 结点数），
 *       使用者不用打开图形视图就能发现。
 */
void SyntaxTreePanel::showTree(const SyntaxTreeData &tree, const QString &title)
{
    m_tree = tree;
    m_title = title;
    m_hasTree = tree.root >= 0;

    // setTree() 内部会重建场景并**自动适应窗口**，因此不需要（也不应该）
    // 再把缩放强制设成滑块的值 —— 早期那样做会把整棵树拉到 100% 并裁在左上角。
    m_graphView->setTree(tree);
    syncSliderToView();
    refreshTextViews();

    if (!m_hasTree) {
        m_summaryLabel->setText(QStringLiteral("（语法树为空：%1）").arg(title));
        return;
    }

    const int reachable = tree.reachableCount();
    const bool connected = (reachable == tree.size());
    m_summaryLabel->setText(QStringLiteral("%1　|　结点 %2 个（叶 %3 个）　|　从根可达 %4 个%5")
                                .arg(title)
                                .arg(tree.size())
                                .arg(tree.leafCount())
                                .arg(reachable)
                                .arg(connected ? QString()
                                               : QStringLiteral("　⚠ 有结点未被挂到树上")));
}

/**
 * 功能：按当前视图形式刷新文本区。
 * 输入：无。输出：无。
 * 注意：三种文本形式拼在同一个文本框里并用分隔标题隔开，
 *       方便一次性复制粘贴进实验报告；当前选中的形式排在最前面。
 */
void SyntaxTreePanel::refreshTextViews()
{
    if (!m_hasTree) {
        m_textView->setPlainText(
            QStringLiteral("（还没有语法树：先在页签⑬完成一次成功的语法分析）"));
        return;
    }

    const QString indent = SyntaxTree::toText(m_tree);
    const QString bracket = SyntaxTree::toBracket(m_tree);
    const QString dot = SyntaxTree::toDot(m_tree, m_title.isEmpty()
                                                       ? QStringLiteral("语法树")
                                                       : m_title);

    QStringList parts;
    const int order[3] = {0, 1, 2};
    // 把当前选中的形式排最前
    QVector<int> seq;
    seq << qBound(0, m_form, 2);
    for (int i : order) {
        if (i != m_form)
            seq << i;
    }

    for (int which : seq) {
        if (which == 0) {
            parts << QStringLiteral("════════ 缩进文本形式 ════════");
            parts << indent;
        } else if (which == 1) {
            parts << QStringLiteral("════════ 括号表示形式 ════════");
            parts << bracket;
        } else {
            parts << QStringLiteral("════════ DOT 图形描述（可用 Graphviz 转成图片）════════");
            parts << dot;
        }
        parts << QString();
    }
    m_textView->setPlainText(parts.join(QLatin1Char('\n')));
}

/** 功能：缩放滑块变化（用户拖动滑块）。输入：value（百分比）。输出：无。 */
void SyntaxTreePanel::onZoomChanged(int value)
{
    if (m_syncingZoom)
        return;   // 正在把视图的比例回填到滑块，不要再反向设置视图
    m_graphView->setZoom(value / 100.0);
}

/**
 * 功能：视图自身缩放变化（滚轮 / 适应窗口 / 100% 按钮）→ 回填滑块。
 * 输入：zoom —— 新比例。
 * 输出：无。
 * 注意：必须屏蔽信号回填，否则"滑块改视图 → 视图发信号 → 再改滑块"会打转。
 */
void SyntaxTreePanel::onViewZoomChanged(double zoom)
{
    Q_UNUSED(zoom);
    syncSliderToView();
}

/**
 * 功能：把视图的当前比例同步到滑块。
 * 输入：无。输出：无。
 * 注意：滑块是整数百分比，且范围 2%~800%；视图实际比例可能超出该范围
 *       （例如极小的树自动适应后仍是 100%，或 800% 之后又被滚轮放大），
 *       这里做钳制，避免滑块把视图的比例"吸"回去。
 */
void SyntaxTreePanel::syncSliderToView()
{
    if (m_zoomSlider == nullptr || m_graphView == nullptr)
        return;
    const int percent = qBound(m_zoomSlider->minimum(), qRound(m_graphView->zoom() * 100.0),
                               m_zoomSlider->maximum());
    if (percent == m_zoomSlider->value())
        return;
    m_syncingZoom = true;
    m_zoomSlider->setValue(percent);
    m_syncingZoom = false;
}

/** 功能：视图形式切换。输入：index。输出：无。 */
void SyntaxTreePanel::onFormChanged(int index)
{
    m_form = index;
    refreshTextViews();
}

/** 功能：语义动作表行数。输入：无。输出：行数。 */
int SyntaxTreePanel::semanticsRowCount() const
{
    return m_semanticsTable->rowCount();
}

/** 功能：语义动作表某单元格文本。输入：row、column。输出：文本。 */
QString SyntaxTreePanel::semanticsCellText(int row, int column) const
{
    const QTableWidgetItem *item = m_semanticsTable->item(row, column);
    return item == nullptr ? QString() : item->text();
}

/** 功能：语法树结点数。输入：无。输出：结点数（-1 表示还没有树）。 */
int SyntaxTreePanel::treeNodeCount() const
{
    return m_hasTree ? m_tree.size() : -1;
}

/** 功能：摘要标签文本。输入：无。输出：文本。 */
QString SyntaxTreePanel::summaryText() const
{
    return m_summaryLabel->text();
}

/** 功能：缩进文本形式。输入：无。输出：文本。 */
QString SyntaxTreePanel::treeTextContent() const
{
    return m_hasTree ? SyntaxTree::toText(m_tree) : QString();
}

/** 功能：括号表示形式。输入：无。输出：文本。 */
QString SyntaxTreePanel::bracketTextContent() const
{
    return m_hasTree ? SyntaxTree::toBracket(m_tree) : QString();
}

// ---------------------------------------------------------------------------
// 【自测用】图形视图的状态查询
// ---------------------------------------------------------------------------

/** 功能：【自测】图形视图当前缩放比例。输入：无。输出：比例（1.0 = 100%）。 */
double SyntaxTreePanel::graphZoom() const
{
    return m_graphView == nullptr ? 1.0 : m_graphView->zoom();
}

/** 功能：【自测】当前比例是否正好是"适应窗口"比例。输入：无。输出：布尔。 */
bool SyntaxTreePanel::graphAtFitZoom() const
{
    return m_graphView != nullptr && m_graphView->atFitZoom();
}

/** 功能：【自测】整棵树是否落在视口内。输入：无。输出：布尔。 */
bool SyntaxTreePanel::graphFitsViewport() const
{
    return m_graphView != nullptr && m_graphView->fitsViewport();
}

/** 功能：【自测】整棵树是否在视口内居中。输入：无。输出：布尔。 */
bool SyntaxTreePanel::graphCentered() const
{
    return m_graphView != nullptr && m_graphView->centered();
}

/** 功能：【自测】内容尺寸（100% 时的像素大小）。输入：无。输出：尺寸。 */
QSize SyntaxTreePanel::graphSceneSize() const
{
    return m_graphView == nullptr ? QSize() : m_graphView->layoutSize();
}

/** 功能：【自测】当前是否可以拖动浏览（内容比视口大，出现了滚动条）。输入：无。输出：布尔。 */
bool SyntaxTreePanel::graphCanPan() const
{
    if (m_graphView == nullptr)
        return false;
    return m_graphView->horizontalScrollBar()->maximum() > 0
           || m_graphView->verticalScrollBar()->maximum() > 0;
}

/** 功能：把图形视图缩放到指定比例。输入：zoom。输出：无。 */
void SyntaxTreePanel::setGraphZoom(double zoom)
{
    if (m_graphView == nullptr)
        return;
    m_graphView->setZoom(zoom);
    syncSliderToView();
}

}  // namespace ccd
