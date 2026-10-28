/******************************************************************************
 * (1) 版权信息 : 编译原理课程设计 - 华南师范大学计算机学院
 * (2) 文件名称 : IntermediateCode.cpp
 *     标识符   : IntermediateCode
 *     摘要     : 四元式中间代码生成的实现（第二项任务 2.11，选做）。
 * (3) 版本号   : 1.0.0       作者/修改者 : <学号 姓名>   完成日期 : 2026-XX-XX
 * (4) 版本历史 : 1.0.0  2026-XX-XX  首次实现（阶段 8）
 *****************************************************************************/

#include "syntax/IntermediateCode.h"

#include <QMap>

namespace ccd {

namespace {

/** @brief 空槽的占位符 */
const char *kEmpty = "-";

/**
 * @brief 终结符名 → 四元式里使用的运算符符号。
 *
 * 语法树的结点标签用的是**文法里的终结符名**（`PLUS`、`LTEQ`…），
 * 而四元式里要写人能看懂的符号（`+`、`<=`…），因此需要一张对照表。
 * 两个语言共用同一张表：TINY 与 mini-C 的运算符终结符名是一致的。
 */
const QMap<QString, QString> &operatorSymbols()
{
    static const QMap<QString, QString> table = {
        {QStringLiteral("PLUS"), QStringLiteral("+")},
        {QStringLiteral("MINUS"), QStringLiteral("-")},
        {QStringLiteral("MULTIPLY"), QStringLiteral("*")},
        {QStringLiteral("DIVIDE"), QStringLiteral("/")},
        {QStringLiteral("MOD"), QStringLiteral("%")},
        {QStringLiteral("POWER"), QStringLiteral("^")},
        {QStringLiteral("LT"), QStringLiteral("<")},
        {QStringLiteral("LTEQ"), QStringLiteral("<=")},
        {QStringLiteral("RT"), QStringLiteral(">")},
        {QStringLiteral("RTEQ"), QStringLiteral(">=")},
        {QStringLiteral("EQ"), QStringLiteral("==")},
        {QStringLiteral("NE"), QStringLiteral("!=")},
        {QStringLiteral("ASSIGN"), QStringLiteral(":=")},
    };
    return table;
}

/**
 * @brief 纯粹"继续往下走"的结构性结点（本身不产生四元式）。
 *
 * 这些标签只表示语法层次（语句序列、表达式层次、定义序列…），
 * 归纳翻译时按子结点顺序依次处理即可。
 */
bool isStructural(const QString &label)
{
    static const QSet<QString> set = {
        QStringLiteral("program"),
        QStringLiteral("definition-list"),
        QStringLiteral("definition"),
        QStringLiteral("stmt-sequence"),
        QStringLiteral("statement-list"),
        QStringLiteral("statement"),
        QStringLiteral("compound-stmt"),
        QStringLiteral("local-definitions"),
        QStringLiteral("expression-stmt"),
        QStringLiteral("simple-exp"),
        QStringLiteral("simple-expression"),
        QStringLiteral("additive-expression"),
        QStringLiteral("term"),
        QStringLiteral("power"),
        QStringLiteral("factor"),
        QStringLiteral("exp"),
        QStringLiteral("arguments"),
        QStringLiteral("argument-list"),
        QStringLiteral("parameters"),
        QStringLiteral("parameter-list"),
    };
    return set.contains(label);
}

/** @brief 二元算术 / 关系运算符所在结点 */
bool isBinaryOperator(const QString &label)
{
    return operatorSymbols().contains(label) && label != QStringLiteral("ASSIGN");
}

/**
 * @brief 一个叶子/变量的显示名。
 *
 * 语法树的叶结点 `label` 是**终结符名**（`ID`、`NUM`），
 * 真正的单词原文放在 `value` 里；四元式要的是原文。缺失时退回标签，
 * 保证不会产出空串（否则四元式会变成 `(+ , , , t1)` 这种不可读的样子）。
 */
QString leafText(const SyntaxNode &n)
{
    if (!n.value.isEmpty())
        return n.value;
    return n.label.isEmpty() ? QStringLiteral("?") : n.label;
}

/**
 * @brief 遍历上下文：临时变量与标号的分配、以及产出的四元式。
 */
struct GenContext {
    const SyntaxTreeData *tree = nullptr;
    QVector<Quadruple> quads;
    QStringList warnings;
    int tempSeq = 0;
    int labelSeq = 0;

    /** @brief 新临时变量 `t1`、`t2`… */
    QString newTemp() { return QStringLiteral("t%1").arg(++tempSeq); }
    /** @brief 新标号 `L1`、`L2`… */
    QString newLabel() { return QStringLiteral("L%1").arg(++labelSeq); }

    /** @brief 追加一条四元式 */
    void emitQuad(const QString &op, const QString &a1, const QString &a2,
              const QString &res, int line = 0, const QString &note = QString())
    {
        Quadruple q;
        q.op = op;
        q.arg1 = a1.isEmpty() ? QString::fromLatin1(kEmpty) : a1;
        q.arg2 = a2.isEmpty() ? QString::fromLatin1(kEmpty) : a2;
        q.result = res.isEmpty() ? QString::fromLatin1(kEmpty) : res;
        q.line = line;
        q.note = note;
        quads.append(q);
    }

    /** @brief 标签为 label 的结点（可能不存在，返回 -1） */
    int childByLabel(int node, const QString &label) const
    {
        for (int c : tree->nodes.at(node).children) {
            if (c >= 0 && c < tree->nodes.size() && tree->nodes.at(c).label == label)
                return c;
        }
        return -1;
    }
};

// 前向声明：翻译一个结点，返回它"算出来的地方"（变量名或临时变量名）
QString translate(GenContext &ctx, int node);

/** @brief 翻译一串子结点，返回最后一个表达式的结果名 */
QString translateChildren(GenContext &ctx, int node)
{
    QString last;
    for (int c : ctx.tree->nodes.at(node).children) {
        if (c < 0 || c >= ctx.tree->nodes.size())
            continue;
        last = translate(ctx, c);
    }
    return last;
}

/**
 * @brief 翻译"取值"上下文里的表达式。
 *
 * 与 `translate()` 的区别：`translate()` 可能返回变量名本身（左值），
 * 而这里保证拿到的是可以当右值用的东西（叶结点原文 / 临时变量）。
 */
QString translateValue(GenContext &ctx, int node);

/** @brief 取变量名：结点可能是 `variable` 结点，也可能直接是 `ID` 叶结点 */
QString variableName(GenContext &ctx, int node)
{
    const SyntaxNode &n = ctx.tree->nodes.at(node);
    if (n.label == QStringLiteral("variable") || n.label == QStringLiteral("ID")) {
        // `variable -> ID`：自己就是叶结点；`variable -> ID LMM exp RMM`：第一个子结点是 ID
        if (!n.value.isEmpty())
            return n.value;
        for (int c : n.children) {
            if (c >= 0 && c < ctx.tree->nodes.size()
                && ctx.tree->nodes.at(c).label == QStringLiteral("ID"))
                return leafText(ctx.tree->nodes.at(c));
        }
        return QStringLiteral("?");
    }
    return translateValue(ctx, node);
}

/**
 * @brief 翻译数组下标：把 `variable` 的第二个子结点算成下标值。
 * @return 形如 `arr[t1]` 的地址描述。
 */
QString arrayAddress(GenContext &ctx, int node)
{
    const SyntaxNode &n = ctx.tree->nodes.at(node);
    if (n.children.size() < 2)
        return variableName(ctx, node);

    const int idNode = n.children.at(0);
    const int idxNode = n.children.at(1);
    const QString name = (idNode >= 0 && idNode < ctx.tree->nodes.size())
                             ? leafText(ctx.tree->nodes.at(idNode))
                             : QStringLiteral("?");
    const QString idx = translateValue(ctx, idxNode);
    const QString t = ctx.newTemp();
    ctx.emitQuad(QStringLiteral("[]="), name, idx, t, ctx.tree->nodes.at(node).line,
             QStringLiteral("取数组元素"));
    return t;
}

/** @brief 翻译一个表达式结点，返回其值（变量名 / 常量 / 临时变量） */
QString translateValue(GenContext &ctx, int node)
{
    if (node < 0 || node >= ctx.tree->nodes.size())
        return QString::fromLatin1(kEmpty);

    const SyntaxNode &n = ctx.tree->nodes.at(node);

    // 真正的叶结点（标识符 / 数字 / 关键字）：直接用单词原文。
    //
    // 注意这里必须同时判断 `children.isEmpty()`：
    // 本项目"复用终结符结点"的语义动作（`R1 0 2`）会让**运算符/关键字的终结符结点
    // 带上子结点** —— 例如 `E -> E PLUS T` 得到的 PLUS 结点就是一个
    // `isTerminal = true` 但带左右两个操作数的结点。
    // 若只判断 `isTerminal`，它会被当成叶结点、返回标签文本 "PLUS"，
    // 于是 `x := 1 + 2` 的四元式会变成 `(:=, PLUS, -, x)`（加法整段丢失）。
    if (n.isTerminal && n.children.isEmpty())
        return leafText(n);

    // 结构性结点：继续往下
    if (isStructural(n.label))
        return translateChildren(ctx, node);

    // 括号表达式（mini-C `factor -> LLM expression RLM` 已被语义动作折叠掉，此处兜底）
    if (n.label == QStringLiteral("LLM") || n.label == QStringLiteral("LPAR"))
        return translateChildren(ctx, node);

    // 数组元素取值
    if (n.label == QStringLiteral("variable") && n.children.size() >= 2)
        return arrayAddress(ctx, node);

    // 普通变量
    if (n.label == QStringLiteral("variable") || n.label == QStringLiteral("ID"))
        return variableName(ctx, node);

    // 一元负号（文法里没有专门产生式，这里兜底处理 `- x` 的情形）
    if (n.label == QStringLiteral("MINUS") && n.children.size() == 1) {
        const QString a = translateValue(ctx, n.children.at(0));
        const QString t = ctx.newTemp();
        ctx.emitQuad(QStringLiteral("uminus"), a, QString(), t, n.line, QStringLiteral("取负"));
        return t;
    }

    // 二元运算（算术 / 关系）
    if (isBinaryOperator(n.label) && n.children.size() >= 2) {
        const QString a = translateValue(ctx, n.children.at(0));
        const QString b = translateValue(ctx, n.children.at(1));
        const QString t = ctx.newTemp();
        ctx.emitQuad(operatorSymbols().value(n.label, n.label), a, b, t, n.line);
        return t;
    }

    // 函数调用
    if (n.label == QStringLiteral("call")) {
        const QString name = variableName(ctx, n.children.isEmpty() ? node : n.children.at(0));
        // 参数表：`arguments -> argument-list`，逐个把实参算出来再 param
        if (n.children.size() >= 2) {
            const int argsNode = n.children.at(1);
            QVector<int> argList;
            if (argsNode >= 0 && argsNode < ctx.tree->nodes.size()) {
                // argument-list 是左递归链：把它展平成"从左到右"的顺序
                QVector<int> stack;
                stack.append(argsNode);
                QVector<int> collected;
                while (!stack.isEmpty()) {
                    const int cur = stack.takeLast();
                    if (cur < 0 || cur >= ctx.tree->nodes.size())
                        continue;
                    const SyntaxNode &an = ctx.tree->nodes.at(cur);
                    if (an.label == QStringLiteral("expression")
                        || an.label == QStringLiteral("argument-list")) {
                        // argument-list -> argument-list DOU expression
                        // 子结点顺序即 [前一段, 本段表达式]
                        for (int i = an.children.size() - 1; i >= 0; --i)
                            stack.append(an.children.at(i));
                    } else if (an.isTerminal) {
                        continue;   // 逗号等分隔符
                    } else {
                        collected.append(cur);
                    }
                }
                argList = collected;
            }
            for (int a : argList) {
                const QString v = translateValue(ctx, a);
                ctx.emitQuad(QStringLiteral("param"), v, QString(), QString(), n.line,
                         QStringLiteral("传实参"));
            }
        }
        const QString t = ctx.newTemp();
        ctx.emitQuad(QStringLiteral("call"), name,
                 QString::number(n.children.size() >= 2 ? 1 : 0), t, n.line);
        return t;
    }

    // 兜底：当成结构性结点继续往下，避免整棵树在这里断掉
    return translateChildren(ctx, node);
}

/**
 * @brief 翻译一个"语句"结点（可能产出控制流四元式，无返回值）。
 */
QString translate(GenContext &ctx, int node)
{
    if (node < 0 || node >= ctx.tree->nodes.size())
        return QString::fromLatin1(kEmpty);

    const SyntaxNode &n = ctx.tree->nodes.at(node);
    const int line = n.line;

    // ---- 赋值 ----
    if (n.label == QStringLiteral("ASSIGN")) {
        if (n.children.size() >= 2) {
            const int lhs = n.children.at(0);
            const int rhs = n.children.at(1);
            const QString value = translateValue(ctx, rhs);
            const SyntaxNode &ln = ctx.tree->nodes.at(lhs);
            if (ln.label == QStringLiteral("variable") && ln.children.size() >= 2) {
                // 数组元素赋值：先算下标，再写回
                const int idNode = ln.children.at(0);
                const QString name = leafText(ctx.tree->nodes.at(idNode));
                const QString idx = translateValue(ctx, ln.children.at(1));
                ctx.emitQuad(QStringLiteral("[]="), value, idx, name, line,
                         QStringLiteral("数组元素赋值"));
            } else {
                ctx.emitQuad(QStringLiteral(":="), value, QString(), variableName(ctx, lhs),
                         line);
            }
        }
        return QString();
    }

    // ---- 条件语句（可能带 else）----
    if (n.label == QStringLiteral("if")) {
        // 子结点：条件表达式、then 语句、（可选）else 语句
        if (n.children.isEmpty())
            return QString();
        const QString cond = translateValue(ctx, n.children.at(0));

        if (n.children.size() >= 3) {
            const QString lElse = ctx.newLabel();
            const QString lEnd = ctx.newLabel();
            ctx.emitQuad(QStringLiteral("iffalse"), cond, QString(), lElse, line,
                     QStringLiteral("条件为假则跳到 else"));
            translate(ctx, n.children.at(1));
            ctx.emitQuad(QStringLiteral("goto"), QString(), QString(), lEnd, line);
            ctx.emitQuad(QStringLiteral("label"), QString(), QString(), lElse, line);
            translate(ctx, n.children.at(2));
            ctx.emitQuad(QStringLiteral("label"), QString(), QString(), lEnd, line);
        } else if (n.children.size() >= 2) {
            const QString lEnd = ctx.newLabel();
            ctx.emitQuad(QStringLiteral("iffalse"), cond, QString(), lEnd, line,
                     QStringLiteral("条件为假则跳过 then 分支"));
            translate(ctx, n.children.at(1));
            ctx.emitQuad(QStringLiteral("label"), QString(), QString(), lEnd, line);
        }
        return QString();
    }

    // ---- TINY 的 repeat…until ----
    if (n.label == QStringLiteral("repeat")) {
        const QString lTop = ctx.newLabel();
        ctx.emitQuad(QStringLiteral("label"), QString(), QString(), lTop, line,
                 QStringLiteral("repeat 循环入口"));
        if (!n.children.isEmpty())
            translate(ctx, n.children.at(0));
        if (n.children.size() >= 2) {
            const QString cond = translateValue(ctx, n.children.at(1));
            // until 的条件为假时继续循环
            ctx.emitQuad(QStringLiteral("if"), cond, QString(), lTop, line,
                     QStringLiteral("until 条件成立则退出"));
        }
        return QString();
    }

    // ---- mini-C 的 do…while ----
    if (n.label == QStringLiteral("do")) {
        const QString lTop = ctx.newLabel();
        ctx.emitQuad(QStringLiteral("label"), QString(), QString(), lTop, line,
                 QStringLiteral("do 循环入口"));
        if (!n.children.isEmpty())
            translate(ctx, n.children.at(0));
        if (n.children.size() >= 2) {
            const QString cond = translateValue(ctx, n.children.at(1));
            ctx.emitQuad(QStringLiteral("if"), cond, QString(), lTop, line,
                     QStringLiteral("while 条件成立则继续循环"));
        }
        return QString();
    }

    // ---- 返回 ----
    if (n.label == QStringLiteral("return")) {
        const QString v = n.children.isEmpty() ? QString() : translateValue(ctx, n.children.at(0));
        ctx.emitQuad(QStringLiteral("return"), v, QString(), QString(), line);
        return QString();
    }

    // ---- TINY 的 read / write ----
    if (n.label == QStringLiteral("read")) {
        const QString v = n.children.isEmpty() ? QString() : variableName(ctx, n.children.at(0));
        ctx.emitQuad(QStringLiteral("read"), QString(), QString(), v, line);
        return QString();
    }
    if (n.label == QStringLiteral("write")) {
        const QString v = n.children.isEmpty() ? QString() : translateValue(ctx, n.children.at(0));
        ctx.emitQuad(QStringLiteral("write"), v, QString(), QString(), line);
        return QString();
    }

    // ---- 变量定义 ----
    if (n.label == QStringLiteral("variable-definition")) {
        if (n.children.size() >= 2) {
            const QString type = leafText(ctx.tree->nodes.at(n.children.at(0)));
            const QString name = variableName(ctx, n.children.at(1));
            if (n.children.size() >= 3) {
                const QString size = leafText(ctx.tree->nodes.at(n.children.at(2)));
                ctx.emitQuad(QStringLiteral("array"), type, size, name, line,
                         QStringLiteral("数组定义"));
            } else {
                ctx.emitQuad(QStringLiteral("decl"), type, QString(), name, line,
                         QStringLiteral("变量定义"));
            }
        }
        return QString();
    }

    // ---- 函数定义 ----
    if (n.label == QStringLiteral("function-definition")) {
        if (n.children.size() >= 2) {
            const QString type = leafText(ctx.tree->nodes.at(n.children.at(0)));
            const QString name = variableName(ctx, n.children.at(1));
            ctx.emitQuad(QStringLiteral("func"), type, QString(), name, line,
                     QStringLiteral("函数定义开始"));
            // 形参表
            if (n.children.size() >= 3) {
                const int paramsNode = n.children.at(2);
                QVector<int> stack;
                stack.append(paramsNode);
                while (!stack.isEmpty()) {
                    const int cur = stack.takeLast();
                    if (cur < 0 || cur >= ctx.tree->nodes.size())
                        continue;
                    const SyntaxNode &pn = ctx.tree->nodes.at(cur);
                    if (pn.label == QStringLiteral("parameter")) {
                        if (pn.children.size() >= 2) {
                            ctx.emitQuad(QStringLiteral("formal"),
                                     leafText(ctx.tree->nodes.at(pn.children.at(0))),
                                     QString(), variableName(ctx, pn.children.at(1)),
                                     pn.line, QStringLiteral("形参"));
                        }
                        continue;
                    }
                    if (pn.isTerminal)
                        continue;
                    for (int i = pn.children.size() - 1; i >= 0; --i)
                        stack.append(pn.children.at(i));
                }
            }
            // 函数体
            if (n.children.size() >= 4)
                translate(ctx, n.children.at(3));
            ctx.emitQuad(QStringLiteral("endfunc"), QString(), QString(), name, line,
                     QStringLiteral("函数定义结束"));
        }
        return QString();
    }

    // ---- 真正的叶结点（无处可去）：什么都不产生 ----
    if (n.isTerminal && n.children.isEmpty())
        return QString();

    // ---- 结构性结点：按子结点顺序继续 ----
    if (isStructural(n.label) || n.children.isEmpty()) {
        // 形参结点如果在函数体之外被单独碰到，也要产出 formal
        if (n.label == QStringLiteral("parameter") && n.children.size() >= 2) {
            ctx.emitQuad(QStringLiteral("formal"),
                     leafText(ctx.tree->nodes.at(n.children.at(0))), QString(),
                     variableName(ctx, n.children.at(1)), line, QStringLiteral("形参"));
            return QString();
        }
        return translateChildren(ctx, node);
    }

    // ---- 未知结点：记一条告警（不静默丢弃，否则用户会以为代码生成漏了）----
    if (!n.isTerminal) {
        ctx.warnings << QStringLiteral("第 %1 行：结点“%2”没有对应的中间代码翻译规则，"
                                       "已按结构性结点继续处理其子结点。")
                            .arg(line)
                            .arg(n.label);
    }
    return translateChildren(ctx, node);
}

}  // namespace

/** 功能：规范写法。输入：无。输出：文本。 */
QString Quadruple::toText() const
{
    QString text = QStringLiteral("(%1, %2, %3, %4)").arg(op, arg1, arg2, result);
    if (!note.isEmpty())
        text += QStringLiteral("   // ") + note;
    return text;
}

/**
 * 功能：从语法树生成四元式。
 * 输入：tree。输出：生成结果。
 * 注意：
 *  1. 生成是**树驱动**的：结点标签决定翻译规则，因此用户改语义动作文件
 *     就会改中间代码；
 *  2. 遇到没有翻译规则的结点不静默跳过，而是记一条告警并继续处理其子结点 ——
 *     否则"代码生成少了半截"会很难发现；
 *  3. 树为空或根下标非法时返回空结果（不抛异常），由调用方决定如何提示。
 */
IntermediateCode IntermediateCode::generate(const SyntaxTreeData &tree)
{
    IntermediateCode code;
    if (tree.root < 0 || tree.root >= tree.nodes.size()) {
        code.warnings << QStringLiteral("语法树为空，无法生成中间代码。");
        return code;
    }

    GenContext ctx;
    ctx.tree = &tree;
    translate(ctx, tree.root);

    code.quads = ctx.quads;
    code.warnings = ctx.warnings;
    code.tempCount = ctx.tempSeq;
    code.labelCount = ctx.labelSeq;
    return code;
}

/** 功能：纯文本形式。输入：withHeader。输出：文本。 */
QString IntermediateCode::toText(bool withHeader) const
{
    QStringList out;
    if (withHeader) {
        out << QStringLiteral("序号   四元式 (运算符, 运算对象1, 运算对象2, 结果)");
        out << QStringLiteral("--------------------------------------------------------------");
    }
    // 注意：这里刻意复用 `Quadruple::toText()` 而不是自己在格式串里拼列宽 ——
    // 早先的写法用了 `%1`…`%6` 六个占位符却只给 5 个实参，结果每行末尾都多出
    // 一个字面量 `%6`，而槽位内容看着又是对的，很容易误判成"显示没问题"。
    for (int i = 0; i < quads.size(); ++i) {
        out << QStringLiteral("%1  %2")
                   .arg(QString::number(i + 1).rightJustified(3), quads.at(i).toText());
    }
    if (quads.isEmpty())
        out << QStringLiteral("（没有生成任何四元式）");
    return out.join(QLatin1Char('\n'));
}

/** 功能：整理成界面表。输入：headers、rows。输出：无。 */
void IntermediateCode::buildTable(QStringList &headers, QList<QStringList> &rows) const
{
    headers = {QStringLiteral("序号"), QStringLiteral("运算符"),
               QStringLiteral("运算对象1"), QStringLiteral("运算对象2"),
               QStringLiteral("结果"), QStringLiteral("源程序行"),
               QStringLiteral("说明")};
    rows.clear();
    for (int i = 0; i < quads.size(); ++i) {
        const Quadruple &q = quads.at(i);
        rows.append({QString::number(i + 1), q.op, q.arg1, q.arg2, q.result,
                     q.line > 0 ? QString::number(q.line) : QString(), q.note});
    }
}

/** 功能：一句话摘要。输入：无。输出：文本。 */
QString IntermediateCode::summary() const
{
    return QStringLiteral("四元式 %1 条；临时变量 %2 个、标号 %3 个；告警 %4 条")
        .arg(quads.size())
        .arg(tempCount)
        .arg(labelCount)
        .arg(warnings.size());
}

}  // namespace ccd
