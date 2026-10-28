/******************************************************************************
 * (1) 版权信息 : 编译原理课程设计 - 华南师范大学计算机学院
 * (2) 文件名称 : SyntaxTreePanel.h
 *     标识符   : SyntaxTreePanel
 *     摘要     : "⑭ 语法树"功能面板（任务书要求 2.7 / 2.8）。
 *                三块内容：
 *                  · 语法树结构定义（语义动作）表格 —— 每行一条产生式，
 *                    显示原始写法与"建几个结点、谁是结果"的中文解释，
 *                    让使用者能确认自己写的定义确实被正确判读；
 *                  · 图形化语法树 —— 按层次自动布局后绘制，支持缩放与拖动；
 *                  · 文本形式导出 —— 缩进文本、括号表示、DOT（可用 Graphviz 转图）。
 * (3) 版本号   : 1.0.0       作者/修改者 : <学号 姓名>   完成日期 : 2026-XX-XX
 * (4) 版本历史 : 1.0.0  2026-XX-XX  首次实现（阶段 6 界面）
 *****************************************************************************/

#pragma once

#include <QSize>
#include <QString>
#include <QWidget>

#include "gui/panels/SyntaxTreeView.h"
#include "syntax/Grammar.h"
#include "syntax/SemanticsAction.h"
#include "syntax/SyntaxTree.h"

class QLabel;
class QPlainTextEdit;
class QSlider;
class QTabWidget;
class QTableWidget;

namespace ccd {

/**
 * @brief 语法树面板。
 *
 * 调用注意：
 *  1. 本面板**只做显示**：树的建立发生在 `syntax/Parser` 规约时（见 AGENT.md §4.2），
 *     这里只接收结果；
 *  2. `showSemantics()` 与 `showTree()` 是两条独立的入口 —— 前者在"文法前端"
 *     跑完就能填（不需要分析源程序），后者要等一次成功的语法分析；
 *  3. 图形绘制委托给 `SyntaxTreeView`（独立的 `QWidget` 子类），
 *     布局由 `SyntaxTree::layout()` 统一计算，保证"文本形式"与"图形形式"
 *     是同一种层次结构，不会出现两套显示对不上的情况。
 */
class SyntaxTreePanel : public QWidget
{
    Q_OBJECT

public:
    /** @brief 构造面板（只建立控件，不载入数据） */
    explicit SyntaxTreePanel(QWidget *parent = nullptr);

    /** @brief 注入文法与语义动作，刷新"语法树结构定义"表格 */
    void showSemantics(const Grammar &grammar, const SemanticsActionSet &semantics);

    /**
     * @brief 推入一棵语法树并刷新全部显示。
     * @param tree  语法树。
     * @param title 标题（通常是源程序或 `.lex` 文件名）。
     */
    void showTree(const SyntaxTreeData &tree, const QString &title);

    /** @brief 清空全部显示 */
    void clearAll();

    /** @brief 【自测用】语义动作表格的行数 */
    int semanticsRowCount() const;

    /** @brief 【自测用】语义动作表格某单元格文本 */
    QString semanticsCellText(int row, int column) const;

    /** @brief 【自测用】语法树结点数（-1 表示还没有树） */
    int treeNodeCount() const;

    /** @brief 【自测用】摘要标签文本 */
    QString summaryText() const;

    /** @brief 【自测用】缩进文本形式的内容 */
    QString treeTextContent() const;

    /** @brief 【自测用】括号表示的内容 */
    QString bracketTextContent() const;

    /** @brief 【自测用】图形视图当前缩放比例 */
    double graphZoom() const;
    /** @brief 【自测用】图形视图当前比例是否正好是"适应窗口"比例 */
    bool graphAtFitZoom() const;
    /** @brief 【自测用】整棵树是否落在视口内 */
    bool graphFitsViewport() const;
    /** @brief 【自测用】整棵树是否在视口内居中 */
    bool graphCentered() const;
    /** @brief 【自测用】内容尺寸（像素，100% 时） */
    QSize graphSceneSize() const;
    /** @brief 【自测用】是否可拖动浏览（滚动了） */
    bool graphCanPan() const;
    /** @brief 把图形视图缩放到指定比例（等价于用户拖滑块） */
    void setGraphZoom(double zoom);

signals:
    /** 用户请求导出语法树（由主窗口弹出文件对话框） */
    void exportRequested();

private slots:
    /** @brief 缩放滑块变化（用户拖滑块） */
    void onZoomChanged(int value);
    /** @brief 视图自身缩放变化（滚轮 / 适应窗口），回填滑块 */
    void onViewZoomChanged(double zoom);
    /** @brief 视图形式切换（缩进 / 括号 / DOT） */
    void onFormChanged(int index);

private:
    /** @brief 构建界面控件与布局 */
    void buildUi();

    /** @brief 把视图的当前比例回填到滑块（不触发反向设置） */
    void syncSliderToView();

    /** @brief 按当前视图形式刷新文本区 */
    void refreshTextViews();

    QTabWidget *m_formTabs = nullptr;      ///< 视图形式：图形 / 缩进文本 / 括号 / DOT
    SyntaxTreeView *m_graphView = nullptr; ///< 图形化语法树（QGraphicsView）
    QSlider *m_zoomSlider = nullptr;       ///< 缩放
    QLabel *m_summaryLabel = nullptr;      ///< 摘要
    QTableWidget *m_semanticsTable = nullptr; ///< 语法树结构定义表
    QPlainTextEdit *m_textView = nullptr;  ///< 当前文本形式的内容

    SyntaxTreeData m_tree;    ///< 最近一次注入的树
    QString m_title;          ///< 最近一次的标题
    bool m_hasTree = false;   ///< 是否已有树
    bool m_syncingZoom = false; ///< 正在滑块 ↔ 视图之间同步（防互相触发）
    int m_form = 0;           ///< 当前文本形式（0 缩进 / 1 括号 / 2 DOT）
};

}  // namespace ccd
