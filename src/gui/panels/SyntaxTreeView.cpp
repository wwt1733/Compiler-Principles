/******************************************************************************
 * (1) 版权信息 : 编译原理课程设计 - 华南师范大学计算机学院
 * (2) 文件名称 : SyntaxTreeView.cpp
 *     标识符   : SyntaxTreeView
 *     摘要     : 语法树图形化控件的实现（第二项任务 2.8）。
 *                用 QGraphicsView + QGraphicsScene 绘制：矩形结点 + 居中标签
 *                + 父子连线；适应窗口、滚轮缩放、拖动平移全部用 Qt 内置能力。
 * (3) 版本号   : 2.0.0       作者/修改者 : <学号 姓名>   完成日期 : 2026-XX-XX
 * (4) 版本历史 : 1.0.0  2026-XX-XX  首次实现（手绘 QWidget + 滚动条）
 *                2.0.0  2026-XX-XX  改为 QGraphicsView（见头文件的版本说明）
 *****************************************************************************/

#include "gui/panels/SyntaxTreeView.h"

#include <QFontMetricsF>
#include <QGraphicsPathItem>
#include <QGraphicsSimpleTextItem>
#include <QPainterPath>
#include <QPen>
#include <QScrollBar>
#include <QWheelEvent>

#include <cmath>

namespace ccd {

namespace {

/** 内容四周留白（像素）：让结点框不贴着视口边缘 */
const double kSceneMargin = 8.0;

/** 缩放范围：2% ~ 800%。下限要能装下"整棵大树适应窗口"，上限要能看清单个标签 */
const double kMinZoom = 0.02;
const double kMaxZoom = 8.0;

/** @brief 结点标签的显示文本：终结符带单词原文时写成 `ID: xXx` */
QString nodeDisplayText(const SyntaxNode &n)
{
    if (n.isTerminal && !n.value.isEmpty())
        return QStringLiteral("%1: %2").arg(n.label, n.value);
    return n.label.isEmpty() ? QStringLiteral("?") : n.label;
}

}  // namespace

/** 功能：构造控件并配置浏览方式。输入：parent。输出：无。 */
SyntaxTreeView::SyntaxTreeView(QWidget *parent) : QGraphicsView(parent)
{
    m_scene = new QGraphicsScene(this);
    setScene(m_scene);

    setRenderHint(QPainter::Antialiasing, true);
    setRenderHint(QPainter::TextAntialiasing, true);
    // 按住左键拖动即可平移（手形光标）；用户不必去够滚动条
    setDragMode(QGraphicsView::ScrollHandDrag);
    // 缩放时"光标下的那个点"保持不动 —— 放大某处细节时不会跑偏
    setTransformationAnchor(QGraphicsView::AnchorUnderMouse);
    setResizeAnchor(QGraphicsView::AnchorViewCenter);
    // 内容比视口小时自动居中（否则会贴在左上角）
    setAlignment(Qt::AlignCenter);
    setBackgroundBrush(Qt::white);
    setFrameShape(QFrame::NoFrame);
    setMinimumHeight(240);
    setHorizontalScrollBarPolicy(Qt::ScrollBarAsNeeded);
    setVerticalScrollBarPolicy(Qt::ScrollBarAsNeeded);

    clearTree();
}

/**
 * 功能：设置要绘制的树。
 * 输入：tree。输出：无。
 * 注意：重建场景后**立即适应窗口** —— 用户一打开页签⑭ 看到的就是整棵树，
 *       而不是"缩在左上角的一团"。之后可滚轮放大、拖动平移。
 */
void SyntaxTreeView::setTree(const SyntaxTreeData &tree)
{
    m_tree = tree;
    rebuildScene(tree);
    fitToWindow();
}

/** 功能：清空。输入：无。输出：无。 */
void SyntaxTreeView::clearTree()
{
    m_tree = SyntaxTreeData();
    m_layout = TreeLayout();
    if (m_scene != nullptr)
        m_scene->clear();
    setSceneRect(QRectF(0.0, 0.0, 1.0, 1.0));

    if (m_scene != nullptr) {
        auto *hint = m_scene->addSimpleText(
            QStringLiteral("（还没有语法树：先在页签⑬完成一次成功的语法分析）"));
        QFont f = font();
        f.setPointSizeF(10.0);
        hint->setFont(f);
        hint->setBrush(QColor(0x88, 0x88, 0x88));
        // 场景矩形贴合提示文字，靠 setAlignment(AlignCenter) 居中显示
        setSceneRect(hint->boundingRect());
    }
    m_userZoomed = false;
    resetTransform();
}

/**
 * 功能：依据当前树重建场景（结点框、标签、父子连线、场景矩形）。
 * 输入：tree。输出：无。
 * 注意：布局参数（字符宽、框高、层距）必须按**真实字体**算出来再传给
 *       `SyntaxTree::layout()`。历史上这里把布局的"逻辑单位"当像素用，
 *       82 个结点被画进约 40×20 像素的角落（AGENT.md §13.35）。
 */
void SyntaxTreeView::rebuildScene(const SyntaxTreeData &tree)
{
    if (m_scene == nullptr)
        return;
    m_scene->clear();

    if (tree.root < 0 || tree.root >= tree.nodes.size()) {
        m_layout = TreeLayout();
        setSceneRect(QRectF(0.0, 0.0, 1.0, 1.0));
        return;
    }

    // ---- 1. 按字体度量决定布局参数 ----
    QFont textFont = font();
    textFont.setPointSizeF(10.0);
    const QFontMetricsF fm(textFont);

    TreeLayoutOptions options;
    // 用 'M' 的宽度作"每字符宽"：比平均值更保守，长标签不会被截断
    options.charWidth = qMax(6.0, fm.horizontalAdvance(QLatin1Char('M')));
    options.boxHeight = qMax(22.0, fm.height() + 12.0);
    options.levelHeight = options.boxHeight + 38.0;   // 层间留出连线空间
    options.minBoxWidth = options.charWidth * 3.0;
    options.siblingGap = 18.0;
    options.padding = 14.0;
    m_layout = SyntaxTree::layout(tree, options);

    // ---- 2. 连线：父框底中点 → 子框顶中点 ----
    // 用 cosmetic pen：缩放时线宽保持 1px，不会放大成一团粗线。
    QPen edgePen(QColor(0x94, 0xA3, 0xB8));
    edgePen.setWidthF(1.0);
    edgePen.setCosmetic(true);

    for (const TreeNodeLayout &lay : m_layout.nodes) {
        if (lay.nodeIndex < 0 || lay.nodeIndex >= tree.nodes.size())
            continue;
        const SyntaxNode &n = tree.nodes.at(lay.nodeIndex);
        for (int c : n.children) {
            if (c < 0 || c >= m_layout.nodes.size())
                continue;
            const TreeNodeLayout &cl = m_layout.nodes.at(c);
            const QPointF from(lay.box.center().x(), lay.box.bottom());
            const QPointF to(cl.box.center().x(), cl.box.top());
            m_scene->addLine(QLineF(from, to), edgePen);
        }
    }

    // ---- 3. 结点框与标签 ----
    for (const TreeNodeLayout &lay : m_layout.nodes) {
        if (lay.nodeIndex < 0 || lay.nodeIndex >= tree.nodes.size())
            continue;
        const SyntaxNode &n = tree.nodes.at(lay.nodeIndex);
        const QString text = nodeDisplayText(n);

        const QColor border = n.isTerminal ? QColor(0x16, 0xA3, 0x4A) : QColor(0x25, 0x63, 0xEB);
        const QColor fill = n.isTerminal ? QColor(0xDC, 0xFC, 0xE7) : QColor(0xDB, 0xEA, 0xFE);

        QPen boxPen(border);
        boxPen.setWidthF(1.4);
        QPainterPath path;
        path.addRoundedRect(lay.box, 5.0, 5.0);
        auto *boxItem = m_scene->addPath(path, boxPen, QBrush(fill));
        // 悬停显示完整标签（长标签会被截断成 "…"）
        boxItem->setToolTip(QStringLiteral("状态%1：%2").arg(lay.nodeIndex).arg(text));

        // 标签：框内居中；放不下时用省略号截断（完整文本见悬停提示）
        const double available = lay.box.width() - 8.0;
        const QString shown =
            (fm.horizontalAdvance(text) <= available)
                ? text
                : fm.elidedText(text, Qt::ElideRight, static_cast<int>(available));
        auto *textItem = m_scene->addSimpleText(shown, textFont);
        textItem->setBrush(QColor(0x11, 0x18, 0x27));
        const QRectF tb = textItem->boundingRect();
        textItem->setPos(lay.box.center().x() - tb.width() / 2.0,
                         lay.box.center().y() - tb.height() / 2.0);
    }

    // ---- 4. 场景矩形 = 内容包围盒 + 留白 ----
    QRectF bounds = m_scene->itemsBoundingRect();
    if (bounds.isEmpty())
        bounds = QRectF(0.0, 0.0, 1.0, 1.0);
    bounds.adjust(-kSceneMargin, -kSceneMargin, kSceneMargin, kSceneMargin);
    setSceneRect(bounds);
}

/** 功能：设置缩放。输入：zoom。输出：无。 */
void SyntaxTreeView::setZoom(double zoom)
{
    const double z = qBound(kMinZoom, zoom, kMaxZoom);
    resetTransform();
    scale(z, z);
    if (!m_applyingFit)
        m_userZoomed = true;   // 手动缩放后，改变窗口大小不再强行适应
    emit zoomChanged(this->zoom());
}

/** 功能：当前缩放比例（取变换矩阵的水平缩放分量）。输入：无。输出：比例。 */
double SyntaxTreeView::zoom() const
{
    return transform().m11();
}

/** 功能：内容尺寸（场景矩形，像素）。输入：无。输出：尺寸。 */
QSize SyntaxTreeView::layoutSize() const
{
    const QRectF r = sceneRect();
    return QSize(qRound(r.width()), qRound(r.height()));
}

/**
 * 功能：计算适应窗口的缩放比例。
 * 输入：无。输出：比例，限制在 [0.02, 1.0]。
 * 注意：上限取 1.0，即"适应窗口只缩小不放大" —— 小树按 100% 居中显示，
 *       不会被放大到夸张。历史上这里用的是控件自身尺寸（那尺寸本身是按
 *       "逻辑单位"算出来的），因此永远算得 ≈0.95，"适应窗口"形同虚设。
 */
double SyntaxTreeView::fitZoom() const
{
    const QRectF content = sceneRect();
    if (content.width() <= 0.0 || content.height() <= 0.0)
        return 1.0;
    const QSize vp = viewport()->size();
    if (vp.width() < 10 || vp.height() < 10)
        return 1.0;   // 还没完成布局，先不折腾

    const double sx = (vp.width() - 4.0) / content.width();
    const double sy = (vp.height() - 4.0) / content.height();
    return qBound(kMinZoom, qMin(sx, sy), 1.0);
}

/** 功能：应用适应窗口比例并居中。输入：无。输出：无。 */
void SyntaxTreeView::fitToWindow()
{
    m_applyingFit = true;
    setZoom(fitZoom());
    m_applyingFit = false;
    m_userZoomed = false;
    centerOn(sceneRect().center());
    // 内容比视口小时 centerOn 不产生滚动，此时靠 setAlignment(AlignCenter) 居中
    emit zoomChanged(zoom());
}

/**
 * 功能：滚轮缩放。
 * 输入：event。
 * 输出：无。
 * 注意：`AnchorUnderMouse` 让光标下的点保持不动 —— 想放大某一处细节时，
 *       把鼠标放在那里滚一下即可，不会"一放大就不知道跑到哪去了"。
 *       这里**不**把事件传给父控件：本控件自己滚动/平移，不需要外层再滚。
 */
void SyntaxTreeView::wheelEvent(QWheelEvent *event)
{
    const int delta = event->angleDelta().y();
    if (delta == 0) {
        QGraphicsView::wheelEvent(event);
        return;
    }
    const double factor = std::pow(1.2, delta / 120.0);
    setZoom(zoom() * factor);
    event->accept();
}

/**
 * 功能：尺寸变化时保持"适应窗口"。
 * 输入：event。输出：无。
 * 注意：只有用户**还没手动缩放过**才重新适应 —— 否则"我刚放大看清一块，
 *       窗口一动就被拉回全貌"会非常恼人。
 */
void SyntaxTreeView::resizeEvent(QResizeEvent *event)
{
    QGraphicsView::resizeEvent(event);
    if (!m_userZoomed)
        fitToWindow();
}

/** 功能：【自测】当前比例是否正好等于适应窗口比例。输入：无。输出：布尔。 */
bool SyntaxTreeView::atFitZoom() const
{
    return qAbs(zoom() - fitZoom()) < 1e-6;
}

/** 功能：【自测】内容 × 比例是否落在视口内。输入：无。输出：布尔。 */
bool SyntaxTreeView::fitsViewport() const
{
    const QSizeF content = sceneRect().size() * zoom();
    const QSize vp = viewport()->size();
    return content.width() <= vp.width() + 2.0 && content.height() <= vp.height() + 2.0;
}

/** 功能：【自测】内容中心是否与视口中心重合。输入：无。输出：布尔。 */
bool SyntaxTreeView::centered() const
{
    if (sceneRect().width() <= 0.0)
        return false;
    const QPoint sceneCenter = mapFromScene(sceneRect().center());
    const QPoint vpCenter = viewport()->rect().center();
    return (sceneCenter - vpCenter).manhattanLength() <= 20;
}

}  // namespace ccd
