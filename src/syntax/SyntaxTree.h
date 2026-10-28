/******************************************************************************
 * (1) 版权信息 : 编译原理课程设计 - 华南师范大学计算机学院
 * (2) 文件名称 : SyntaxTree.h
 *     标识符   : SyntaxTree
 *     摘要     : 语法树的文本化、括号表示、DOT 导出与绘制（第二项任务 2.6）。
 *                语法树的**构造**在 Parser 里随规约完成（见 syntax/Parser），
 *                本模块只负责把构造好的树"呈现出来"：
 *                  · 缩进文本（与 `reference/08_tiny_tree.out` 同构，便于对照）；
 *                  · 括号表示（一行式，便于写进实验报告）；
 *                  · DOT 源文件（可用 Graphviz 出图，任务书 2.4 的"图形"要求）；
 *                  · 分层布局坐标（供界面自绘，避免额外依赖 Graphviz）。
 *                本文件只依赖 QtCore（绘制部分不依赖 QtGui）。
 * (3) 版本号   : 1.0.0       作者/修改者 : <学号 姓名>   完成日期 : 2026-XX-XX
 * (4) 版本历史 : 1.0.0  2026-XX-XX  首次实现（阶段 6）
 *****************************************************************************/

#pragma once

#include <QPointF>
#include <QRectF>
#include <QString>
#include <QStringList>
#include <QVector>

#include "common/Types.h"

namespace ccd {

/**
 * @brief 布局后的一个结点（供界面绘制）。
 *
 * 坐标单位是**像素/点**，由调用者通过 `TreeLayoutOptions` 按字体度量给出
 * （本类只依赖 QtCore，不能自己量字体）。整个布局会被整体平移到从 (0,0) 起，
 * 因此调用者无需再猜原点。
 */
struct TreeNodeLayout {
    int nodeIndex = -1;   ///< 对应的 SyntaxNode 下标
    QPointF center;       ///< 结点中心坐标（像素）
    QRectF box;           ///< 结点外框（像素）
    int depth = 0;        ///< 层次（根为 0）
};

/**
 * @brief 一棵树的完整布局结果。
 */
struct TreeLayout {
    QVector<TreeNodeLayout> nodes;  ///< 各结点布局（顺序与 SyntaxNode 一致）
    double width = 0.0;             ///< 内容总宽（像素）
    double height = 0.0;            ///< 内容总高（像素）

    /** @brief 该树是否为空 */
    bool isEmpty() const { return nodes.isEmpty(); }
};

/**
 * @brief 语法树布局参数（单位：像素/点）。
 *
 * 为什么由调用者给：结点框要按**真实文字宽度**定尺寸，而 `syntax/` 只依赖
 * QtCore、拿不到 QFontMetrics。界面层量好字体后把几个数字传进来即可，
 * 核心算法仍然与 GUI 无关、可被无界面自测直接调用。
 *
 * 历史教训（AGENT.md §13.35）：早期版本把这些值写死成"逻辑单位"（1.0），
 * 界面却把它们当像素用 —— 82 个结点被画进约 40×20 像素的角落，
 * 整棵树"缩在左上角成一团"。
 */
struct TreeLayoutOptions {
    double charWidth = 8.0;    ///< 每个字符的横向宽度估计（像素）
    double minBoxWidth = 26.0; ///< 结点框最小宽度
    double boxHeight = 26.0;   ///< 结点框高度
    double levelHeight = 62.0; ///< 相邻两层的中心距
    double siblingGap = 18.0;  ///< 相邻兄弟结点框之间的最小间隙
    double padding = 12.0;     ///< 文字两侧再留的余量
};

/**
 * @brief 语法树的呈现工具集合。
 *
 * 调用注意：
 *  1. 本类全部为静态方法，不持有状态 —— 树本身由 `SyntaxTreeData` 承载；
 *  2. 文本格式刻意与老师给的 `tree.out` 保持同构（`BTreeNode { kind: ... value: ... }`
 *     的外层结构换成更易读的形式），这样两边的树一比对就能发现语义动作是否
 *     理解正确 —— 这是阶段 6 最重要的自检手段；
 *  3. `layout()` 采用经典的"后序遍历定叶序、父结点居中"算法：
 *     先给每个叶子分配递增的 x 序号，再让父结点的 x 取子结点 x 的中值。
 *     该算法不会让兄弟子树互相重叠，且层次由深度直接决定，简单且稳定。
 */
class SyntaxTree
{
public:
    /**
     * @brief 缩进文本形式。
     * @param tree  树。
     * @param showValue 是否在叶结点后附上单词原文（如 `ID: xXx`）。
     * @return 多行文本；空树返回说明性文字。
     */
    static QString toText(const SyntaxTreeData &tree, bool showValue = true);

    /**
     * @brief 括号表示（`(program (stmt-sequence ...))` 形式）。
     * @param tree 树。
     * @param showValue 是否附上叶结点原文。
     * @return 单行文本（过长，不适合界面直接显示，但适合写进报告）。
     */
    static QString toBracket(const SyntaxTreeData &tree, bool showValue = true);

    /**
     * @brief 生成 DOT 源文件内容（可用 Graphviz 渲染成图片）。
     * @param tree 树。
     * @param title 图标题（写入 label，便于区分不同文法的树）。
     * @return DOT 文本。
     * @note 结点形状区分叶与非叶：叶用方框（box），非叶用椭圆（ellipse）。
     */
    static QString toDot(const SyntaxTreeData &tree, const QString &title = QString());

    /**
     * @brief 计算分层布局。
     * @param tree 树。
     * @param options 布局参数（字符宽、框高、层距、兄弟间隙等，单位像素）。
     * @return 布局结果（像素坐标，已整体平移到从 (0,0) 起）。
     * @note 算法：后序遍历算子树宽度 → 先序按区间切分定横向中心。
     *       叶子占"自己的框宽 + 兄弟间隙"的横向槽，因此标签长短不一时
     *       同一层的结点框也**不会互相重叠**（这是"能看清"的前提）。
     */
    static TreeLayout layout(const SyntaxTreeData &tree,
                             const TreeLayoutOptions &options = TreeLayoutOptions());

    /**
     * @brief 树的括号化"结构签名"，用于**比对两棵树是否同构**。
     *
     * 只含标签与括号，不含单词原文与行号。同构比对是自测里最有价值的断言：
     * 老师给的参考树与本项目生成的树只要签名一致，就说明语义动作的判读正确。
     * @param tree 树。
     * @return 签名文本。
     */
    static QString signature(const SyntaxTreeData &tree);

    /**
     * @brief 统计各标签出现次数（用于自测断言与报告数据）。
     * @param tree 树。
     * @return 标签 → 次数。
     */
    static QMap<QString, int> labelHistogram(const SyntaxTreeData &tree);

    /**
     * @brief 读取"老师给出的参考语法树文件"（`reference/08_tiny_tree.out` 那种缩进文本）。
     *
     * 该格式为嵌套的 `BTreeNode { kind: ? value: ? nodeList: ... }`，每个结点用
     * 若干行描述，层次由行首空格数体现。读进来之后可以：
     *   · 用 `signature()` 与本项目自建的树做**同构比对**，
     *     验证语义动作文件的判读是否与老师的实现一致；
     *   · 作为教学演示，让使用者对照两棵树的形状差异。
     *
     * @param text 文件全文。
     * @param error 输出：失败原因。
     * @return 解析出的树；失败时为空树并把原因写入 error。
     * @note 本函数取 `kind` 作为结点标签，并把参考文件里的 `kind: -1`
     *       **归一化**为本项目的非终结符占位标签 `kReferenceNonterminal`
     *       （非叶结点在参考文件里 kind 恒为 `-1`，成分名写在 value 里）。
     *       归一化后即可与 `signature()` 的输出直接比较：两边都以
     *       "终结符名 / 统一的非终结符占位符"参与签名，比对的正是树的形状。
     */
    static SyntaxTreeData fromReferenceText(const QString &text, QString &error);

    /** @brief 参考文件里非叶结点 kind 的归一化占位标签（值为 `-1` 字符串） */
    static QString referenceNonterminalLabel();
};

}  // namespace ccd
