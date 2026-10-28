/******************************************************************************
 * (1) 版权信息 : 编译原理课程设计 - 华南师范大学计算机学院
 * (2) 文件名称 : SyntaxTree.cpp
 *     标识符   : SyntaxTree
 *     摘要     : 语法树文本化、DOT 导出与布局算法的实现（第二项任务 2.6）。
 * (3) 版本号   : 1.0.0       作者/修改者 : <学号 姓名>   完成日期 : 2026-XX-XX
 * (4) 版本历史 : 1.0.0  2026-XX-XX  首次实现（阶段 6）
 *****************************************************************************/

#include "syntax/SyntaxTree.h"

#include <QMap>
#include <QStringList>

#include "common/FileUtil.h"

namespace ccd {

namespace {

/** @brief 结点显示名：叶结点带上单词原文 */
QString displayName(const SyntaxNode &n, bool showValue)
{
    if (showValue && n.isTerminal && !n.value.isEmpty())
        return QStringLiteral("%1: %2").arg(n.label, n.value);
    return n.label;
}

}  // namespace

/**
 * 功能：缩进文本形式。
 * 输入：tree、showValue。输出：文本。
 * 注意：空树与"根下标越界"都要给出明确文字而不是空串 —— 界面拿到空串会
 *       显示成一片空白，使用者无法区分"没建树"与"建树失败"。
 */
QString SyntaxTree::toText(const SyntaxTreeData &tree, bool showValue)
{
    if (tree.root < 0 || tree.root >= tree.nodes.size()) {
        if (tree.nodes.isEmpty())
            return QStringLiteral("（语法树为空）");
        return QStringLiteral("（语法树根下标 %1 无效，结点数 %2）")
            .arg(tree.root)
            .arg(tree.nodes.size());
    }

    QStringList out;
    // 用显式栈做先序遍历，避免深树递归过深
    struct Frame {
        int node;
        int depth;
    };
    QVector<Frame> stack;
    stack.append({tree.root, 0});

    while (!stack.isEmpty()) {
        const Frame f = stack.takeLast();
        const SyntaxNode &n = tree.nodes.at(f.node);
        out << QString(f.depth * 4, QLatin1Char(' ')) + displayName(n, showValue);

        // 逆序压栈，保证弹出顺序仍是"从左到右"
        for (int i = n.children.size() - 1; i >= 0; --i) {
            const int c = n.children.at(i);
            if (c >= 0 && c < tree.nodes.size())
                stack.append({c, f.depth + 1});
        }
    }
    return out.join(QLatin1Char('\n'));
}

/**
 * 功能：括号表示。
 * 输入：tree、showValue。输出：单行文本。
 * 注意：用显式栈而非递归（深树不必担心爆栈）。每个元素是一个待处理项：
 *       `node >= 0` 表示"输出该结点"，`node == -1` 表示"补一个右括号"，
 *       `node == -2` 表示"补一个空格"。空格用独立的栈项插入，
 *       而不是靠"判断是不是最后一个子结点"来拼字符串 —— 后者在
 *       子结点下标非法（被跳过）时容易多加或漏加空格。
 */
QString SyntaxTree::toBracket(const SyntaxTreeData &tree, bool showValue)
{
    if (tree.root < 0 || tree.root >= tree.nodes.size())
        return QStringLiteral("()");

    QString out;
    QVector<int> stack;
    stack.append(tree.root);

    while (!stack.isEmpty()) {
        const int idx = stack.takeLast();
        if (idx == -1) {
            out += QLatin1Char(')');
            continue;
        }
        if (idx == -2) {
            out += QLatin1Char(' ');
            continue;
        }

        const SyntaxNode &n = tree.nodes.at(idx);
        out += QLatin1Char('(') + displayName(n, showValue);

        if (n.children.isEmpty()) {
            out += QLatin1Char(')');
            continue;
        }

        // 先压右括号（最后输出），再逆序压子结点与分隔空格
        stack.append(-1);
        for (int i = n.children.size() - 1; i >= 0; --i) {
            const int c = n.children.at(i);
            if (c < 0 || c >= tree.nodes.size())
                continue;
            stack.append(c);
            if (i > 0)
                stack.append(-2);
        }
        // 末尾可能因为子结点非法而留下一个多余空格，这里去掉
    }

    out.replace(QStringLiteral(" )"), QStringLiteral(")"));
    return out;
}

/**
 * 功能：生成 DOT 源文件内容。
 * 输入：tree、title。输出：DOT 文本。
 * 注意：
 *  1. 结点标识用 `n<下标>`，避免标签里的字符（如 `<`、`"`）破坏 DOT 语法；
 *  2. 标签文本一律走 `FileUtil::escapeForDot()`，它保证输出**纯可打印 ASCII**。
 *     这是必需的 —— 标签里会出现单词原文，而本项目按字节处理（自动机字母表
 *     覆盖全部 256 个字节值），原文里可能含控制字节与高位字节；
 *     原样写进 .dot 会让 Graphviz 报 "syntax error ... scanning a quoted string"
 *     而**整张图画不出来**（不是字体问题，是根本无法解析）；
 *  3. 叶用方框、非叶用椭圆，一眼能看出哪些是终结符；
 *  4. 输出带 `rankdir=TB`，与界面绘制方向一致。
 */
QString SyntaxTree::toDot(const SyntaxTreeData &tree, const QString &title)
{
    QStringList lines;
    lines << QStringLiteral("digraph SyntaxTree {");
    lines << QStringLiteral("    rankdir=TB;");
    lines << QStringLiteral("    node [fontname=\"Helvetica\"];");
    if (!title.isEmpty())
        lines << QStringLiteral("    label=\"%1\"; labelloc=t; fontsize=14;")
                     .arg(FileUtil::escapeForDot(title));

    for (int i = 0; i < tree.nodes.size(); ++i) {
        const SyntaxNode &n = tree.nodes.at(i);
        // 标签里的英文/数字/运算符保持原样，控制字节与中文会变成 <NN> / <U+XXXX>
        const QString text = FileUtil::escapeForDot(displayName(n, true));
        const QString shape = n.isTerminal ? QStringLiteral("box") : QStringLiteral("ellipse");
        const QString fill = n.isTerminal ? QStringLiteral("#DCFCE7") : QStringLiteral("#DBEAFE");
        lines << QStringLiteral("    n%1 [label=\"%2\", shape=%3, style=filled, fillcolor=\"%4\"];")
                     .arg(i)
                     .arg(text, shape, fill);
    }

    for (int i = 0; i < tree.nodes.size(); ++i) {
        for (int c : tree.nodes.at(i).children) {
            if (c >= 0 && c < tree.nodes.size())
                lines << QStringLiteral("    n%1 -> n%2;").arg(i).arg(c);
        }
    }

    lines << QStringLiteral("}");
    return lines.join(QLatin1Char('\n'));
}

/**
 * 功能：计算分层布局（像素坐标）。
 * 输入：tree —— 语法树；options —— 布局参数（字符宽、框高、层距、兄弟间隙）。
 * 输出：布局结果；坐标已整体平移到从 (0,0) 起。
 *
 * 注意 —— 算法（后序定叶序、父取子中值）：
 *  1. 第一遍用显式栈做后序遍历，给每个结点算出 depth 与"子树横向槽宽"；
 *  2. 第二遍自顶向下分配 x：叶占据自己区间的中点，
 *     父结点的 x = 首子结点 x 与末子结点 x 的中值；
 *  3. y = depth × levelHeight，纵轴向下为正，与屏幕坐标一致；
 *  4. 之所以自己算而不依赖 Graphviz：交付要求"零外部依赖可运行"。
 *
 * 两个**关键约定**（早期踩过，见 AGENT.md §13.35）：
 *  · 坐标单位是**像素**，由调用者按字体度量给出（`syntax/` 只依赖 QtCore，
 *    量不了字体）。早期把这些值写死成"逻辑单位"、界面却当像素用，
 *    82 个结点被画进约 40×20 像素的角落 —— "缩在左上角成一团"；
 *  · 叶子占"**自己的框宽 + 兄弟间隙**"的横向槽，不再固定为 1.0。
 *    否则标签长（如 `variable-definition`）的结点框会横向互相压住，
 *    即使给了正确的像素比例也看不清。
 */
TreeLayout SyntaxTree::layout(const SyntaxTreeData &tree, const TreeLayoutOptions &options)
{
    TreeLayout result;
    if (tree.root < 0 || tree.root >= tree.nodes.size())
        return result;

    const int total = tree.nodes.size();

    /** 结点显示文本的长度（与界面口径一致：终结符带原文时写成 `标签: 原文`） */
    const auto displayLength = [&tree](int index) {
        const SyntaxNode &n = tree.nodes.at(index);
        if (n.isTerminal && !n.value.isEmpty())
            return qMax(1, static_cast<int>(n.label.size()) + 2 + static_cast<int>(n.value.size()));
        return qMax(1, static_cast<int>(n.label.size()));
    };
    /** 结点框宽度：按字符数估宽，并保证不小于最小值 */
    const auto boxWidthOf = [&options, &displayLength](int index) {
        return qMax(options.minBoxWidth,
                    displayLength(index) * options.charWidth + options.padding);
    };

    QVector<int> depth(total, 0);
    QVector<double> slot(total, 1.0);   ///< 该结点子树占用的横向槽宽（像素）
    QVector<double> x(total, 0.0);

    // ---- 第一遍（后序）：算子树宽度，并给子结点定深度 ----
    // 显式栈做后序遍历。父结点下标不一定大于子结点（规约时先建父、后挂子），
    // 所以不能按下标顺序遍历。
    struct Frame {
        int node;
        bool childrenPushed;
    };
    QVector<Frame> stack;
    stack.append({tree.root, false});

    while (!stack.isEmpty()) {
        Frame f = stack.last();
        const SyntaxNode &n = tree.nodes.at(f.node);

        if (!f.childrenPushed) {
            stack.last().childrenPushed = true;
            for (int i = n.children.size() - 1; i >= 0; --i) {
                const int c = n.children.at(i);
                if (c < 0 || c >= total)
                    continue;
                depth[c] = depth.at(f.node) + 1;
                stack.append({c, false});
            }
            continue;
        }

        // 子结点已全部处理完，累加子树占用的横向槽宽。
        // 关键：叶子不再固定占 1.0，而是"自己的框宽 + 兄弟间隙" —— 这样
        // 标签长短不一时，同一层的结点框也不会互相重叠。
        const double ownSlot = boxWidthOf(f.node) + options.siblingGap;
        double w = 0.0;
        for (int c : n.children) {
            if (c >= 0 && c < total)
                w += slot.at(c);
        }
        slot[f.node] = qMax(w, ownSlot);
        stack.removeLast();
    }

    // ---- 第二遍（先序）：按区间切分定 x ----
    // 叶结点占据自己区间的中点；父结点的两个子区间中点的平均**就是**父的 x，
    // 这个闭式表达与子结点最终算出的 x 必然一致，不需要依赖遍历顺序。
    struct AssignFrame {
        int node;
        double left;   ///< 分配给该子树的横向区间左端
    };
    QVector<AssignFrame> aStack;
    aStack.append({tree.root, 0.0});

    while (!aStack.isEmpty()) {
        const AssignFrame af = aStack.takeLast();
        const SyntaxNode &n = tree.nodes.at(af.node);

        QVector<int> kids;
        for (int c : n.children) {
            if (c >= 0 && c < total)
                kids.append(c);
        }

        if (kids.isEmpty()) {
            x[af.node] = af.left + slot.at(af.node) / 2.0;
            continue;
        }

        // 依次把区间切给子结点，并算出首/末子区间中点
        double cursor = af.left;
        double firstMid = af.left;
        double lastMid = af.left;
        for (int k = 0; k < kids.size(); ++k) {
            const int c = kids.at(k);
            aStack.append({c, cursor});
            const double mid = cursor + slot.at(c) / 2.0;
            if (k == 0)
                firstMid = mid;
            lastMid = mid;
            cursor += slot.at(c);
        }
        x[af.node] = (firstMid + lastMid) / 2.0;
    }

    // ---- 汇总：整理成像素矩形 ----
    result.nodes.reserve(total);

    double minLeft = 1e18;
    double minTop = 1e18;
    double maxRight = -1e18;
    double maxBottom = -1e18;

    for (int i = 0; i < total; ++i) {
        TreeNodeLayout l;
        l.nodeIndex = i;
        l.depth = depth.at(i);
        const double centerY = l.depth * options.levelHeight;
        l.center = QPointF(x.at(i), centerY);
        const double boxW = boxWidthOf(i);
        l.box = QRectF(l.center.x() - boxW / 2.0, centerY - options.boxHeight / 2.0,
                       boxW, options.boxHeight);
        minLeft = qMin(minLeft, l.box.left());
        minTop = qMin(minTop, l.box.top());
        maxRight = qMax(maxRight, l.box.right());
        maxBottom = qMax(maxBottom, l.box.bottom());
        result.nodes.append(l);
    }

    // ---- 归一化：整体平移到从 (0,0) 起，视图层不必猜原点 ----
    if (minLeft != 0.0 || minTop != 0.0) {
        const QPointF shift(-minLeft, -minTop);
        for (TreeNodeLayout &l : result.nodes) {
            l.center += shift;
            l.box.translate(shift);
        }
    }
    result.width = maxRight - minLeft;
    result.height = maxBottom - minTop;

    return result;
}

/**
 * 功能：结构签名（只含标签与括号）。
 * 输入：tree。输出：签名文本。
 *
 * 注意 —— 归一化规则：
 *  1. **终结符结点**用其标签（如 `ID`、`PLUS`）参与签名；
 *  2. **非终结符结点**统一用占位符 `-1`。理由：老师给出的参考树
 *     （`reference/08_tiny_tree.out`）里非叶结点的 kind 恒为 `-1`，
 *     成分名写在 value 里；统一归一化后两侧签名可以直接字符串比较，
 *     比对的正是**树的形状 + 终结符出现情况** —— 这才是要验证的东西
 *     （语义动作文件判读得对不对），而不是"成分名放在了哪个字段"；
 *  3. 签名不含单词原文与行号：标识符具体叫什么不影响同构判断。
 */
QString SyntaxTree::signature(const SyntaxTreeData &tree)
{
    if (tree.root < 0 || tree.root >= tree.nodes.size())
        return QStringLiteral("()");

    const QString nt = referenceNonterminalLabel();
    QString out;
    struct Item {
        int node;
        bool closing;
    };
    QVector<Item> stack;
    stack.append({tree.root, false});

    while (!stack.isEmpty()) {
        const Item it = stack.takeLast();
        if (it.closing) {
            out += QLatin1Char(')');
            continue;
        }
        const SyntaxNode &n = tree.nodes.at(it.node);
        out += QLatin1Char('(') + (n.isTerminal ? n.label : nt);

        if (!n.children.isEmpty()) {
            stack.append({it.node, true});
            for (int i = n.children.size() - 1; i >= 0; --i) {
                const int c = n.children.at(i);
                if (c >= 0 && c < tree.nodes.size())
                    stack.append({c, false});
            }
        } else {
            out += QLatin1Char(')');
        }
    }
    return out;
}

/** 功能：标签直方图。输入：tree。输出：标签→次数。 */
QMap<QString, int> SyntaxTree::labelHistogram(const SyntaxTreeData &tree)
{
    QMap<QString, int> hist;
    for (const SyntaxNode &n : tree.nodes)
        hist[n.label] += 1;
    return hist;
}

/** 功能：参考树非终结符的归一化占位标签。输入：无。输出："-1"。 */
QString SyntaxTree::referenceNonterminalLabel()
{
    return QStringLiteral("-1");
}

/**
 * 功能：读取老师给出的参考语法树文件。
 * 输入：text、error。输出：树。
 *
 * 注意 —— 解析策略：
 *  1. 用显式栈记录"正在收集子结点的结点 + 它的缩进列号"，不递归；
 *  2. 遇到 `BTreeNode` 就新建结点。它的父结点是**最近的、缩进比它小且
 *     尚未闭合**的那个结点；若没有这样的结点，它就是根（只取第一个）；
 *  3. 遇到 `}` 就闭合所有"缩进不小于本行"的开放结点；
 *  4. 只取 kind 与 value 两个字段，`nodeList:` 及花括号行忽略 ——
 *     父子关系由缩进与 `}` 决定，不需要解析 nodeList 这一行本身。
 */
SyntaxTreeData SyntaxTree::fromReferenceText(const QString &text, QString &error)
{
    SyntaxTreeData tree;
    error.clear();

    const QStringList lines = text.split(QLatin1Char('\n'));
    if (lines.isEmpty()) {
        error = QStringLiteral("参考语法树文件为空。");
        return tree;
    }

    struct OpenNode {
        int index;    ///< 结点下标
        int indent;   ///< 该结点 `BTreeNode` 行的缩进列数
    };
    QVector<OpenNode> open;

    int pendingIndex = -1;   ///< 刚建立、等待读取 kind/value 的结点

    for (const QString &rawLine : lines) {
        // 计算缩进（行首空格数）
        int indent = 0;
        while (indent < rawLine.size() && rawLine.at(indent) == QLatin1Char(' '))
            ++indent;

        const QString line = rawLine.trimmed();
        if (line.isEmpty())
            continue;

        if (line == QStringLiteral("BTreeNode")) {
            int parent = -1;
            for (int i = open.size() - 1; i >= 0; --i) {
                if (open.at(i).indent < indent) {
                    parent = open.at(i).index;
                    break;
                }
            }

            const int idx = tree.addNode(QString());
            if (parent >= 0)
                tree.nodes[parent].children.append(idx);
            else if (tree.root < 0)
                tree.root = idx;

            open.append({idx, indent});
            pendingIndex = idx;
            continue;
        }

        if (line == QStringLiteral("{") || line == QStringLiteral("nodeList:"))
            continue;

        if (line == QStringLiteral("}")) {
            while (!open.isEmpty() && open.last().indent >= indent)
                open.removeLast();
            continue;
        }

        if (line.startsWith(QStringLiteral("kind:"))) {
            const QString v = line.mid(5).trimmed();
            if (pendingIndex >= 0 && pendingIndex < tree.nodes.size()) {
                tree.nodes[pendingIndex].label = v;
                // 参考文件里非叶结点的 kind 恒为 -1；据此判定是否终止符，
                // 使 isTerminal 与本项目自建树的口径一致
                tree.nodes[pendingIndex].isTerminal =
                    (v != referenceNonterminalLabel());
            }
            continue;
        }

        if (line.startsWith(QStringLiteral("value:"))) {
            const QString v = line.mid(6).trimmed();
            if (pendingIndex >= 0 && pendingIndex < tree.nodes.size()) {
                SyntaxNode &n = tree.nodes[pendingIndex];
                // 参考文件里两类结点的写法并不一样：
                //   非叶结点： kind 恒为 `-1`，真正的**语法成分名**写在 value 里
                //             （`kind: -1 / value: stmt-sequence`）
                //   叶结点  ： kind 是单词类别，value 是单词原文
                //             （`kind: ID / value: xXx`）
                // 所以非叶结点要把 value 收作**标签**、并把 value 清空，
                // 否则所有非叶结点的标签都会退化成 `-1`，
                // 直方图与签名里就完全看不出语法成分名了（实际踩过这个坑）。
                if (!n.isTerminal) {
                    n.label = v;
                    n.value.clear();
                } else {
                    n.value = v;
                }
            }
            continue;
        }
    }

    if (tree.root < 0 || tree.nodes.isEmpty()) {
        error = QStringLiteral("未能从参考语法树文件中解析出任何结点。");
        return tree;
    }
    return tree;
}

}  // namespace ccd
