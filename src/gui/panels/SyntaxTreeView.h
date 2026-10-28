/******************************************************************************
 * (1) 版权信息 : 编译原理课程设计 - 华南师范大学计算机学院
 * (2) 文件名称 : SyntaxTreeView.h
 *     标识符   : SyntaxTreeView
 *     摘要     : 语法树的图形化绘制控件（第二项任务 2.8 的"以树状形式呈现"）。
 *                按 `SyntaxTree::layout()` 算出的**像素**坐标建立图元，
 *                提供"适应窗口 / 滚轮缩放 / 按住拖动平移"三种浏览方式。
 *                只依赖 QtWidgets，不含任何算法逻辑。
 * (3) 版本号   : 2.0.0       作者/修改者 : <学号 姓名>   完成日期 : 2026-XX-XX
 * (4) 版本历史 : 1.0.0  2026-XX-XX  首次实现（手绘 QWidget + 滚动条）
 *                2.0.0  2026-XX-XX  改为 QGraphicsView：
 *                   ① 布局坐标本来是"逻辑单位"，手绘版却当像素用，82 个结点
 *                      被画进约 40×20 像素的角落（"缩在左上角成一团"）；
 *                   ② 内置能力替代手写滚动条数学：ScrollHandDrag 拖动平移、
 *                      AnchorUnderMouse 锚点缩放、AlignCenter 自动居中、
 *                      fitInView 式适应窗口。
 *****************************************************************************/

#pragma once

#include <QGraphicsScene>
#include <QGraphicsView>
#include <QSize>

#include "syntax/SyntaxTree.h"

namespace ccd {

/**
 * @brief 语法树绘制控件（QGraphicsView）。
 *
 * 调用注意：
 *  1. `setTree()` 会重建场景并**自动适应窗口**，因此推入树之后整棵树立刻可见；
 *  2. 坐标由 `SyntaxTree::layout()` 统一给出（像素坐标，父结点 x 落在子结点区间内），
 *     本控件不自己算布局 —— 否则"文本形式"和"图形形式"会出现两套层次；
 *  3. 交互约定：**滚轮 = 以光标为锚点缩放**、**按住左键拖动 = 平移**、
 *     "适应窗口"按钮 = 一键看全貌；用户手动缩放后，改变窗口大小不会再强行拉回。
 */
class SyntaxTreeView : public QGraphicsView
{
    Q_OBJECT

public:
    /** @brief 构造控件。输入：parent。输出：无。 */
    explicit SyntaxTreeView(QWidget *parent = nullptr);

    /** @brief 设置要绘制的树（重建场景 → 自动适应窗口 → 居中） */
    void setTree(const SyntaxTreeData &tree);

    /** @brief 清空（场景里只留一条灰色提示） */
    void clearTree();

    /** @brief 设置缩放比例（1.0 为 100%） */
    void setZoom(double zoom);

    /** @brief 当前缩放比例 */
    double zoom() const;

    /**
     * @brief 适合窗口的缩放比例。
     * @return `min(视口宽/内容宽, 视口高/内容高)` 留边距，限制在 [0.02, 1.0]。
     * @note 上限取 1.0 —— **适应窗口只缩小不放大**，小树按 100% 居中显示即可，
     *       否则一棵三五个结点的树会被放大到夸张。
     */
    double fitZoom() const;

    /** @brief 应用 fitZoom() 并把内容居中 */
    void fitToWindow();

    /** @brief 【自测用】内容尺寸（像素，100% 时的场景矩形，含边距） */
    QSize layoutSize() const;

    /** @brief 【自测用】当前比例是否正好等于 fitZoom()（用于断言"自动适应窗口生效"） */
    bool atFitZoom() const;

    /** @brief 【自测用】内容 × 当前比例是否落在视口内（2 像素容差） */
    bool fitsViewport() const;

    /** @brief 【自测用】内容中心是否与视口中心重合（偏差 ≤ 20 像素） */
    bool centered() const;

signals:
    /** 缩放比例变化（滚轮 / 滑块 / 适应窗口都会发），供面板回填滑块 */
    void zoomChanged(double zoom);

protected:
    /** @brief 滚轮：以光标为锚点缩放（不滚动） */
    void wheelEvent(QWheelEvent *event) override;
    /** @brief 尺寸变化：用户没手动缩放过时重新适应窗口 */
    void resizeEvent(QResizeEvent *event) override;

private:
    /**
     * @brief 依据当前树重建场景（图元 + 场景矩形）。
     * @param tree 树。
     * @note 布局参数在这里按**字体度量**算出来传给 `SyntaxTree::layout()` ——
     *       `syntax/` 只依赖 QtCore 量不了字体，所以由界面层提供这几个数字。
     */
    void rebuildScene(const SyntaxTreeData &tree);

    QGraphicsScene *m_scene = nullptr;  ///< 场景（父子关系托管给本控件）
    TreeLayout m_layout;                ///< 最近一次布局结果
    SyntaxTreeData m_tree;              ///< 最近一次注入的树
    bool m_userZoomed = false;          ///< 用户是否手动缩放过（决定 resize 是否重新适应）
    bool m_applyingFit = false;         ///< 正在执行"适应窗口"（避免它把 m_userZoomed 置脏）
};

}  // namespace ccd
