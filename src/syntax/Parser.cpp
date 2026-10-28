/******************************************************************************
 * (1) 版权信息 : 编译原理课程设计 - 华南师范大学计算机学院
 * (2) 文件名称 : Parser.cpp
 *     标识符   : Parser
 *     摘要     : 表驱动 LALR(1) 语法分析器的实现（第二项任务 2.6）。
 * (3) 版本号   : 1.0.0       作者/修改者 : <学号 姓名>   完成日期 : 2026-XX-XX
 * (4) 版本历史 : 1.0.0  2026-XX-XX  首次实现（阶段 6）
 *****************************************************************************/

#include "syntax/Parser.h"

#include <QMap>
#include <QSet>

#include <algorithm>

namespace ccd {

namespace {

/** @brief 把符号栈渲染成短文本（过长时省略中间，便于界面一行显示） */
QString renderStack(const QStringList &stack, int limit = 24)
{
    if (stack.size() <= limit)
        return stack.join(QLatin1Char(' '));
    QStringList head = stack.mid(0, limit / 2);
    QStringList tail = stack.mid(stack.size() - limit / 2);
    return head.join(QLatin1Char(' ')) + QStringLiteral(" ... ") + tail.join(QLatin1Char(' '));
}

/** @brief 动作类型的中文名 */
QString actionName(ActionType t)
{
    switch (t) {
    case ActionType::Shift:
        return QStringLiteral("移进");
    case ActionType::Reduce:
        return QStringLiteral("规约");
    case ActionType::Accept:
        return QStringLiteral("接受");
    case ActionType::Error:
    default:
        return QStringLiteral("出错");
    }
}

/**
 * @brief 把一次规约渲染成过程文本，例如 `规约 12：exp -> exp PLUS term`。
 * @param prodId 产生式号。
 * @param prod   产生式。
 * @return 文本。
 */
QString reduceText(int prodId, const Production &prod)
{
    return QStringLiteral("%1 %2：%3")
        .arg(actionName(ActionType::Reduce))
        .arg(prodId)
        .arg(prod.toString());
}

}  // namespace

/** 功能：单行文本。输入：无。输出：文本。 */
QString ParseStep::toText() const
{
    return QStringLiteral("%1 | 状态[%2] | 符号[%3] | 输入 %4 | 第%5行 | %6")
        .arg(index, 5)
        .arg(stateStack.isEmpty() ? QString() : QString::number(stateStack.last()))
        .arg(renderStack(symbolStack))
        .arg(lookahead)
        .arg(line)
        .arg(action);
}

/** 功能：单行文本。输入：无。输出：文本。 */
QString SyntaxError::toText() const
{
    QString exp;
    if (!expected.isEmpty())
        exp = QStringLiteral("；应为以下之一：%1").arg(expected.join(QStringLiteral(" / ")));
    return QStringLiteral("第 %1 行：%2%3").arg(line).arg(message, exp);
}

/** 功能：过程文本。输入：maxSteps。输出：文本。 */
QString ParseResult::stepsText(int maxSteps) const
{
    QStringList out;
    const int n = (maxSteps > 0) ? qMin(maxSteps, steps.size()) : steps.size();

    int lastStatement = 0;
    for (int i = 0; i < n; ++i) {
        const ParseStep &s = steps.at(i);
        // 按语句分段：语句号变化时插入一条分隔行，便于"每条语句"独立查看
        if (s.statement != lastStatement) {
            lastStatement = s.statement;
            out << QStringLiteral("──────── 第 %1 条语句 ────────").arg(s.statement);
        }
        out << s.toText();
    }

    if (n < steps.size())
        out << QStringLiteral("……（共 %1 步，此处只显示前 %2 步）").arg(steps.size()).arg(n);
    if (out.isEmpty())
        out << QStringLiteral("（没有分析过程记录）");
    return out.join(QLatin1Char('\n'));
}

/** 功能：错误明细文本。输入：无。输出：文本。 */
QString ParseResult::errorsText() const
{
    if (errors.isEmpty())
        return QStringLiteral("语法分析通过，未发现错误。");
    QStringList out;
    for (const SyntaxError &e : errors)
        out << e.toText();
    return out.join(QLatin1Char('\n'));
}

/** 功能：一句话摘要。输入：无。输出：文本。 */
QString ParseResult::summary() const
{
    return QStringLiteral("语法分析：%1；%2 步；语法树 %3 个结点（叶 %4 个）；%5")
        .arg(success ? QStringLiteral("成功") : QStringLiteral("失败"))
        .arg(steps.size())
        .arg(tree.size())
        .arg(tree.leafCount())
        .arg(errors.isEmpty() ? QStringLiteral("无语法错误")
                              : QStringLiteral("%1 处语法错误").arg(errors.size()));
}

/** 功能：设置分析表。输入：table。输出：无。 */
void Parser::setTable(const ParseTable &table)
{
    m_table = table;
    m_hasTable = true;
    m_error.clear();
}

/** 功能：设置语义动作。输入：semantics。输出：无。 */
void Parser::setSemantics(const SemanticsActionSet &semantics)
{
    m_semantics = semantics;
}

/** 功能：设置语句边界符号。输入：symbol。输出：无。 */
void Parser::setStatementBoundary(const QString &symbol)
{
    m_statementBoundary = symbol;
}

/**
 * 功能：从某状态列出"期望符号集"。
 * 输入：state、limit。输出：符号列表。
 * 注意：这里**不区分移进与规约** —— 只要能推进，对使用者而言都是"这里可以
 *       出现的东西"。列规约动作对应的符号时要去掉 `$`（`$` 是结束标记，
 *       把它列进"应为"里会让报错信息难以理解），但若状态**只**能靠 `$`
 *       推进（例如输入已结束而缺少收尾符号），则保留 `$` 以免给出空集。
 */
QStringList Parser::expectedAt(int state, int limit) const
{
    if (state < 0 || state >= m_table.action.size())
        return {};

    QStringList symbols;
    for (auto it = m_table.action.at(state).constBegin();
         it != m_table.action.at(state).constEnd(); ++it) {
        if (it.value().type == ActionType::Error)
            continue;
        symbols << it.key();
    }
    symbols.sort();

    QStringList named;
    for (const QString &s : symbols) {
        if (s != QStringLiteral("$"))
            named << s;
    }
    if (named.isEmpty())
        named = symbols;

    if (limit > 0 && named.size() > limit) {
        const int extra = named.size() - limit;
        named = named.mid(0, limit);
        named << QStringLiteral("…（另 %1 个）").arg(extra);
    }
    return named;
}

/**
 * 功能：分析表所用文法相对"用户原文法"的产生式编号偏移量。
 * 输入：无。输出：偏移量。
 *
 * 注意：本项目 `Grammar::augment()` 把增广产生式 `S' -> S` **追加在末尾**，
 *       且"前 N 条 = 用户写的产生式"这一对应关系始终成立（正因如此，
 *       `GrammarSimplify` 也要求先化简再增广）。所以用户产生式 i 在增广后
 *       仍然是 id = i，偏移量为 0。
 *
 *       保留这个函数是为了把规则集中在一处、并让自测可以断言它：
 *       若将来有人把增广产生式改插到下标 0，只要改这里的返回值即可。
 *       判定方式是"下标 0 的左部是不是增广开始符号"。
 */
int Parser::semanticsOffset() const
{
    if (!m_hasTable)
        return 0;

    const Grammar &g = m_table.grammar;
    if (!g.augmented() || g.productionCount() <= 0)
        return 0;

    return (g.production(0).left == g.augmentedStart()) ? 1 : 0;
}

/**
 * 功能：执行语义动作，构造本次规约的结点。
 * 输入：prod、rhsNodes、tree。输出：结果树根下标。
 *
 * 注意 —— 建树规则（本项目的语义动作契约，详见 AGENT.md §10）：
 *  1. `rhsNodes[i]` 是右部第 i 项对应的树根：可能是一个**终结符叶结点**
 *     （移进时创建），也可能是一个**非终结符结点**（先前规约建立），
 *     还可能是 `-1`（该位置没有树根）；
 *  2. 一行的每个组按顺序处理，**最后一个组**的结果就是本次规约的返回值；
 *  3. 组的三种写法：
 *       `L c…`    —— **新建**结点，标签 = 产生式左部符号名；
 *       `R n c…`  —— **复用**右部第 n 项已有的那个结点作为本组的结点
 *                    （该位置没有结点时才新建，标签取右部第 n 个符号名）；
 *       `-1 n`    —— 不建结点，直接把右部第 n 项的树根传上去。
 *     夹在组名后面的数字 c 是"要挂成子结点的右部项下标"；
 *  4. "复用"是表达"操作符结点带两侧操作数"的关键：
 *     `exp -> simple-exp comparison-op simple-exp` 写成 `R1 0 2`，
 *     意为"取 comparison-op 那个结点，把左右两个 simple-exp 挂成它的子结点"，
 *     于是 `<` 结点直接代表整个 exp —— 与老师参考树同形。
 *     若写成"新建"，比较运算符的结点就会另起一个，真正的 `<` 反而成了孤岛；
 *  5. 挂子结点是通过回填**子结点的 parent** 完成的（不是往父结点的
 *     children 里追加）。原因是规约自底向上，建父结点时子结点已存在、
 *     但父结点要到最后才确定；统一记 parent 后由
 *     `SyntaxTreeData::rebuildChildren()` 一次算出 children，
 *     这样"子结点被重新认父"也能正确处理（上面第 4 条正需要这个能力）；
 *  6. `rhsNodes` 里的 `-1` 以及"自己挂自己"都会被跳过，避免悬挂引用与自环。
 */
int Parser::applySemantics(const Production &prod, const QVector<int> &rhsNodes,
                          SyntaxTreeData &tree) const
{
    // 增广产生式 `S' -> S` 不对应语义动作文件里的任何一行（文件按用户原文法编号）。
    // 它的作用只是给整棵树一个唯一的根，因此在这里直接"包一层"：
    // 新建一个以增广开始符号为标签的结点，把 S 的树根挂成它的子结点。
    if (prod.left == m_table.grammar.augmentedStart()) {
        const int node = tree.addNode(prod.left, false, 0);
        if (!rhsNodes.isEmpty() && rhsNodes.first() >= 0)
            tree.nodes[rhsNodes.first()].parent = node;
        return node;
    }

    const QVector<SemanticsGroup> groups =
        m_semantics.actionAt(prod.id - semanticsOffset());
    if (groups.isEmpty())
        return -1;

    // 结果树根：默认取右部第 0 项的树根；建了结点时会被最后一个组覆盖。
    // "默认取第 0 项"必须保留 —— 整行写成 `-1 n` 时不建结点，
    // 若这里返回 -1，上层就再也接不到这棵子树，树会从中间断开。
    int resultNode = rhsNodes.isEmpty() ? -1 : rhsNodes.first();

    for (const SemanticsGroup &g : groups) {
        // ---- `-1 n`：不建结点，传递右部第 n 项的树根 ----
        if (g.kind == SemanticsGroup::PassThrough) {
            const int idx = g.symbolIndex;
            resultNode = (idx >= 0 && idx < rhsNodes.size()) ? rhsNodes.at(idx) : -1;
            continue;
        }

        // ---- 确定本组的结点：能复用就复用，否则新建 ----
        int node = -1;
        if (g.kind == SemanticsGroup::BuildSymbol && g.symbolIndex >= 0
            && g.symbolIndex < rhsNodes.size() && rhsNodes.at(g.symbolIndex) >= 0) {
            node = rhsNodes.at(g.symbolIndex);   // 复用右部第 n 项已有的结点
        } else {
            QString label;
            bool isTerminalNode = false;
            if (g.kind == SemanticsGroup::BuildLhs) {
                label = prod.left;   // 左部一定是非终结符
            } else if (g.symbolIndex >= 0 && g.symbolIndex < prod.right.size()) {
                label = prod.right.at(g.symbolIndex);
                // 取右部符号为结点时，若该符号是终结符，这个结点就是终结符结点。
                // 这个标志决定树文本的写法与界面着色，必须如实设置。
                isTerminalNode = m_table.grammar.isTerminal(label);
            } else {
                label = prod.left;   // 下标越界时退回左部名，保证标签不为空
            }
            node = tree.addNode(label, isTerminalNode, 0);
        }

        // ---- 把右部第 c 项的树根挂成本组结点的子结点 ----
        for (int c : g.children) {
            if (c < 0 || c >= rhsNodes.size())
                continue;
            const int child = rhsNodes.at(c);
            if (child < 0 || child == node)
                continue;
            if (child < tree.nodes.size())
                tree.nodes[child].parent = node;
        }

        resultNode = node;
    }

    return resultNode;
}

/**
 * 功能：执行分析。
 * 输入：symbols、lines、values、maxSteps。输出：结果。
 *
 * 注意 —— 算法与记录方式：
 *  1. 经典 LR 算法：状态栈初值为 `[0]`，符号栈初值为空；
 *     移进时把符号与原子的树根一起压栈，规约时弹出 n 项、执行语义动作、
 *     再按 GOTO 压入新状态与结果树根；
 *  2. 每一步都记录"**执行前**"的两个栈与当前向前看符号 —— 这样整张过程表
 *     读起来就是"看到什么、栈里是什么、于是做什么"，与实际手算过程一致；
 *  3. 出错恢复：先弹栈直到当前状态对该符号有动作，再丢弃输入符号直到
 *     能推进，从而一次报出多处错误。这是**刻意**的设计：只报第一个错误
 *     对调试帮助有限，而实验报告要求展示错误处理能力。
 */
ParseResult Parser::parse(const QStringList &symbols, const QVector<int> &lines,
                          const QStringList &values, int maxSteps) const
{
    ParseResult result;

    if (!m_hasTable) {
        SyntaxError e;
        e.message = QStringLiteral("尚未设置分析表，无法分析。");
        result.errors.append(e);
        return result;
    }
    if (symbols.isEmpty()) {
        SyntaxError e;
        e.message = QStringLiteral("输入符号序列为空。");
        result.errors.append(e);
        return result;
    }

    const int n = symbols.size();
    result.inputSymbols = symbols;   ///< 供界面显示"剩余输入"

    // 状态栈 / 符号栈 / 树根栈
    QVector<int> stateStack;
    QStringList symbolStack;
    QVector<int> nodeStack;
    stateStack.append(0);

    // 当前向前看符号位置
    int pos = 0;
    int statement = 1;
    int stepIndex = 0;

    // 收集顶层语句文本片段（用于界面"按语句查看"）
    QStringList currentStatement;

    // 防御：步数上限。正常情况下 LR 分析步数约为 O(输入长度)，
    // 但若分析表本身有缺陷（如某状态对某符号错误地填了规约而 GOTO 为空），
    // 可能陷入"规约-移进"死循环。设一个与输入规模挂钩的上限避免界面卡死。
    const int stepLimit = qMax(10000, n * 200);
    int guard = 0;

    auto recordStep = [&](const QString &action, int line, bool isError) {
        if (maxSteps > 0 && result.steps.size() >= maxSteps)
            return;
        ParseStep s;
        s.index = stepIndex;
        s.stateStack = stateStack;
        s.symbolStack = symbolStack;
        s.inputPos = pos;
        s.lookahead = (pos < n) ? symbols.at(pos) : QStringLiteral("<输入结束>");
        s.action = action;
        s.line = line;
        s.statement = statement;
        s.isError = isError;
        result.steps.append(s);
    };

    auto lineAt = [&](int p) -> int {
        if (p >= 0 && p < lines.size())
            return lines.at(p);
        return (lines.isEmpty() ? 0 : lines.last());
    };
    auto valueAt = [&](int p) -> QString {
        if (p >= 0 && p < values.size())
            return values.at(p);
        return QString();
    };

    while (true) {
        if (++guard > stepLimit) {
            SyntaxError e;
            e.line = lineAt(pos);
            e.inputPos = pos;
            e.message = QStringLiteral("分析步数超过上限（%1 步），已中止。"
                                       "请检查分析表是否存在异常的规约循环。")
                            .arg(stepLimit);
            result.errors.append(e);
            recordStep(e.message, e.line, true);
            break;
        }

        if (pos >= n) {
            // 输入已耗尽但仍未接受：报错并结束
            SyntaxError e;
            e.line = lineAt(n - 1);
            e.inputPos = n - 1;
            e.found = QStringLiteral("<输入结束>");
            e.expected = expectedAt(stateStack.last());
            e.message = QStringLiteral("输入提前结束，还有未完成的语法成分。");
            result.errors.append(e);
            recordStep(QStringLiteral("出错：输入提前结束"), e.line, true);
            break;
        }

        const QString &a = symbols.at(pos);
        const int top = stateStack.last();
        const ParseAction act = m_table.actionAt(top, a);

        switch (act.type) {
        case ActionType::Shift: {
            recordStep(QStringLiteral("移进 %1，转状态 %2").arg(a).arg(act.value), lineAt(pos),
                       false);
            ++stepIndex;

            // 建终结符叶结点；单词原文写进 value，
            // 这样语法树里显示的是 `ID: xXx` 而不是光秃秃的 `ID`
            const int node = result.tree.addNode(a, true, lineAt(pos));
            result.tree.nodes[node].value = valueAt(pos);

            stateStack.append(act.value);
            symbolStack.append(a);
            nodeStack.append(node);
            currentStatement << a;
            ++pos;
            break;
        }

        case ActionType::Reduce: {
            const int prodId = act.value;
            if (prodId < 0 || prodId >= m_table.grammar.productionCount()) {
                SyntaxError e;
                e.line = lineAt(pos);
                e.inputPos = pos;
                e.message = QStringLiteral("分析表引用了不存在的产生式号 %1。").arg(prodId);
                result.errors.append(e);
                recordStep(e.message, e.line, true);
                guard = stepLimit;   // 立即跳出
                break;
            }

            const Production &prod = m_table.grammar.production(prodId);
            const int rhsLen = prod.right.size();

            // 单步过程记录里把产生式写全，便于对照文法
            recordStep(reduceText(prodId, prod), lineAt(pos), false);
            ++stepIndex;

            // 弹出右部。
            //
            // 注意这里的下标方向 —— 这是一个非常隐蔽的坑：
            // `nodeStack` 的**末尾**是右部最右边的符号，而 `takeLast()` 也是从末尾取，
            // 因此从 `k = rhsLen-1` 倒着循环、每次取栈顶，取到的**正好**是右部第 k 项，
            // 可以直接写 `rhsNodes[k]`。
            // 若写成"正着循环 + takeLast()"，整个右部就会被左右颠倒：
            // 症状是 `a + b` 建出来的树变成"操作符只挂着 b"，左侧操作数整块消失，
            // 而分析过程本身完全正常（因此极难从"分析成功"这一点察觉）。
            QVector<int> rhsNodes(rhsLen, -1);
            for (int k = rhsLen - 1; k >= 0; --k) {
                if (!stateStack.isEmpty() && stateStack.size() > 1)
                    stateStack.removeLast();
                if (!symbolStack.isEmpty())
                    symbolStack.removeLast();
                if (!nodeStack.isEmpty())
                    rhsNodes[k] = nodeStack.takeLast();
            }

            // 语句计数：把 `;` 归约进上层时，一条顶层语句结束。
            // 注意 `;` 通常**不在**右部末尾（TINY 是
            // `stmt-sequence -> stmt-sequence SEMI statement`），
            // 所以必须扫描整个右部，不能只看最后一项。
            if (!m_statementBoundary.isEmpty() && prod.right.contains(m_statementBoundary)) {
                result.statements << currentStatement.join(QLatin1Char(' '));
                currentStatement.clear();
                result.statementsParsed += 1;
                statement += 1;
            }

            // 执行语义动作建结点
            const int newNode = applySemantics(prod, rhsNodes, result.tree);

            // GOTO
            const int g = m_table.gotoAt(stateStack.last(), prod.left);
            if (g < 0) {
                SyntaxError e;
                e.line = lineAt(pos);
                e.inputPos = pos;
                e.message = QStringLiteral("GOTO 表缺少 [状态 %1, 非终结符 %2] 的表项，"
                                           "分析表可能不完整。")
                                .arg(stateStack.last())
                                .arg(prod.left);
                result.errors.append(e);
                recordStep(e.message, e.line, true);
                guard = stepLimit;
                break;
            }

            stateStack.append(g);
            symbolStack.append(prod.left);
            nodeStack.append(newNode);
            break;
        }

        case ActionType::Accept: {
            recordStep(QStringLiteral("接受"), lineAt(pos), false);
            ++stepIndex;
            result.acceptedState = stateStack.last();
            break;
        }

        case ActionType::Error:
        default: {
            // ---- 出错：记录 + 恢复到能继续的位置 ----
            SyntaxError e;
            e.line = lineAt(pos);
            e.inputPos = pos;
            e.found = a;
            e.expected = expectedAt(stateStack.last());
            e.message = QStringLiteral("遇到 \"%1\" 无法继续。").arg(a);
            result.errors.append(e);
            recordStep(QStringLiteral("出错：遇到 %1").arg(a), e.line, true);
            ++stepIndex;

            // 恢复策略：
            //   ① 弹状态直到某个状态对当前符号有动作（或只剩状态 0）；
            //   ② 若仍无动作，丢弃当前输入符号，直到符号能推进或输入耗尽。
            bool recovered = false;
            while (stateStack.size() > 1) {
                stateStack.removeLast();
                if (!symbolStack.isEmpty())
                    symbolStack.removeLast();
                if (!nodeStack.isEmpty())
                    nodeStack.removeLast();
                if (m_table.actionAt(stateStack.last(), a).type != ActionType::Error) {
                    recovered = true;
                    break;
                }
            }

            if (!recovered) {
                // 丢弃输入符号
                while (pos < n
                       && m_table.actionAt(stateStack.last(), symbols.at(pos)).type
                              == ActionType::Error) {
                    ++pos;
                }
                if (pos >= n)
                    guard = stepLimit;   // 无法恢复，结束
            }
            break;
        }
        }

        // 接受或已记录致命错误时结束
        if (act.type == ActionType::Accept)
            break;
        if (guard >= stepLimit)
            break;
    }

    // 根结点：LR 分析接受时，符号栈里应当只剩开始符号（在最底），
    // 其对应的树根就是整棵树的根 —— 也就是树根栈的**栈顶**。
    // 注意：不能"从栈底往上找第一个非负值"，那是本轮调试踩到的真实错误：
    // 栈底是最早压入的第一个终结符叶结点，取它会得到一棵"根是这个单词"的假树。
    if (!nodeStack.isEmpty()) {
        for (int i = nodeStack.size() - 1; i >= 0; --i) {
            if (nodeStack.at(i) >= 0) {
                result.tree.root = nodeStack.at(i);
                break;
            }
        }
    }

    // 按 parent 字段重建 children。
    // 必须放在最后统一做：规约是自底向上的，建子结点时父结点还不存在；
    // 而"操作符包住左侧操作数"（`&k`）要求在建完操作符结点后回填左侧结点的 parent。
    // 按 parent 字段重建 children，然后丢弃不可达的孤岛结点。
    // 必须放在最后统一做：规约自底向上，建子结点时父结点还不存在；
    // 而"复用右部第 n 项的结点"这类动作还会给已有结点改认父结点。
    // 清掉孤岛是为了让"结点池里的结点"与"树上真正有的结点"一致 ——
    // 移进时为每个终结符都建了叶结点，纯分隔符（`;`、`then`、`end`）不会
    // 被语义动作挂进树里，留着就会让 size()/leafCount() 虚高。
    result.tree.rebuildChildren();
    result.tree.pruneUnreachable();

    result.success = result.errors.isEmpty();
    return result;
}

}  // namespace ccd
