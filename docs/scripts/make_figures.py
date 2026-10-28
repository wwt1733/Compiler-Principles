# -*- coding: utf-8 -*-
"""生成实验报告书需要的结构图 / 数据流图 / 算法流程图。

图形用 Graphviz DOT 描述并渲染成 PNG（无网格线，符合
reference/taskbook/算法流程图的画法-含错误示例.pdf 的要求：
单入口单出口、只用顺序 / 判断 / 循环三种基本结构、图上不带网格线）。

用法：python make_figures.py
输出：docs/figures/*.dot（可复现的源码）与 docs/images/*.png（插入文档的位图）
"""

import os
import subprocess
import sys

DOT = r"F:\Graphviz\bin\dot.exe"
HERE = os.path.dirname(os.path.abspath(__file__))
DOCS = os.path.dirname(HERE)
FIG_DIR = os.path.join(DOCS, "figures")
IMG_DIR = os.path.join(DOCS, "images")

FONT = "SimHei"

HEAD = f"""digraph G {{
  graph [rankdir=TB, fontname="{FONT}", fontsize=12, nodesep=0.38, ranksep=0.42,
         margin=0.16, bgcolor="white", pad=0.15];
  node  [fontname="{FONT}", fontsize=11, penwidth=1.1, color="#333333"];
  edge  [fontname="{FONT}", fontsize=10, penwidth=1.1, color="#333333",
         arrowsize=0.75, labeldistance=1.7];
"""


def _wrap(text, width=20):
    """把过长的结点文字按字符数折行（\\n 是 DOT 的换行转义）。

    折行的目的是压缩图的**宽度**：图最终要缩到 13~14.6 cm 宽插入 A4 文档，
    原图越宽、字号被缩得越小。宽 < 20 字时不动。
    """
    out = []
    for line in text.split("\\n"):
        if len(line) <= width:
            out.append(line)
            continue
        buf = ""
        for ch in line:
            buf += ch
            if len(buf) >= width:
                out.append(buf)
                buf = ""
        if buf:
            out.append(buf)
    return "\\n".join(out)


def box(uid, text, shape="box", extra=""):
    text = _wrap(text)
    return f'  {uid} [label="{text}", shape={shape}{(", " + extra) if extra else ""}];\n'


def edge(a, b, label=None, style=None, constraint=None, extra=""):
    parts = []
    if label:
        parts.append(f'label="{label}"')
    if style:
        parts.append(f"style={style}")
    if constraint is not None:
        flag = constraint if isinstance(constraint, str) else ("true" if constraint else "false")
        parts.append(f"constraint={flag}")
    if extra:
        parts.append(extra)
    attrs = f" [{', '.join(parts)}]" if parts else ""
    return f"  {a} -> {b}{attrs};\n"


def flow(name, title, nodes, edges, rankdir="TB", dpi=200, extra_graph=""):
    """nodes: 列表 (id, 文本, 形状)；首尾用 rounded 表示开始/结束。"""
    body = [HEAD.replace("rankdir=TB", f"rankdir={rankdir}")]
    # 标题单独放在 graph label 里会让图变高，这里改成语义化的注释节点（不显示）
    for uid, text, shape in nodes:
        style = ""
        if shape == "start":
            shape, style = "box", 'style="rounded,filled", fillcolor="#E8F0FE"'
        elif shape == "dec":
            shape = "diamond"
            style = 'style="filled", fillcolor="#FFF4E0"'
        elif shape == "io":
            shape = "parallelogram"
            style = 'style="filled", fillcolor="#EDF7ED"'
        elif shape == "sub":
            shape, style = "box", 'style="rounded"'
        elif shape == "note":
            shape, style = "note", 'style="filled", fillcolor="#F5F5F5"'
        body.append(box(uid, text, shape, style))
    body.extend(edges)
    body.append(f"  graph [dpi={dpi}];\n")
    if extra_graph:
        body.append(extra_graph)
    body.append("}\n")
    src = "".join(body)
    dot_path = os.path.join(FIG_DIR, name + ".dot")
    png_path = os.path.join(IMG_DIR, name + ".png")
    with open(dot_path, "w", encoding="utf-8") as fh:
        fh.write(src)
    cmd = [DOT, "-Tpng", "-Gdpi=%d" % dpi, dot_path, "-o", png_path]
    proc = subprocess.run(cmd, capture_output=True, text=True)
    if proc.returncode != 0:
        raise RuntimeError(f"{name} 渲染失败：{proc.stderr.strip()}")
    warn = proc.stderr.strip()
    size = os.path.getsize(png_path)
    print(f"  {name:22s} {os.path.basename(png_path):24s} {size // 1024:5d} KB"
          + (f"  [警告] {warn}" if warn else ""))
    return png_path


def chain_chart(base, items, max_nodes=9):
    """纯顺序流程（无分支）自动拆成 (a)(b) 两幅，避免整图过高导致字太小。"""
    if len(items) <= max_nodes:
        nodes = [(f"n{i}", text, shape) for i, (text, shape) in enumerate(items)]
        edges = [edge(nodes[i][0], nodes[i + 1][0]) for i in range(len(nodes) - 1)]
        return [flow(base, base, nodes, edges)]
    out = []
    parts = [items[:max_nodes], items[max_nodes:]]
    for pi, part in enumerate(parts):
        suffix = "a" if pi == 0 else "b"
        nodes = []
        if pi > 0:
            nodes.append(("entry", "接图（a）", "start"))
        for i, (text, shape) in enumerate(part):
            nodes.append((f"n{i}", text, shape))
        if pi == 0:
            nodes.append(("exitx", "接图（b）", "start"))
        edges = [edge(nodes[i][0], nodes[i + 1][0]) for i in range(len(nodes) - 1)]
        out.append(flow(base + suffix, base + suffix, nodes, edges))
    return out


def build():
    os.makedirs(FIG_DIR, exist_ok=True)
    os.makedirs(IMG_DIR, exist_ok=True)

    # ---------------- 系统总体结构图 ----------------
    nodes = [
        ("ui", "图形界面层（Qt Widgets）\\n主窗口 / 15 个功能页签 / 日志窗", "sub"),
        ("t1", "第一项任务：词法分析生成器", "box"),
        ("t2", "第二项任务：LALR(1) 语法分析生成器", "box"),
        ("t3", "选做：四元式中间代码生成", "box"),
        ("c", "公共基础层\\nFileUtil / Logger / LanguageConfig / Types", "sub"),
    ]
    edges = [
        edge("ui", "t1"), edge("ui", "t2"), edge("ui", "t3"),
        edge("t1", "c"), edge("t2", "c"), edge("t3", "c"),
        edge("t1", "t2", "输出 .lex（编码文件）", style="dashed", constraint="false"),
    ]
    flow("fig_structure", "系统总体结构", nodes, edges)

    # 词法流水线里的各模块，单独画一棵子树
    nodes = [
        ("ui", "① 单词规格编辑器（tiny_words.txt / minic_words.txt）", "box"),
        ("spec", "TokenSpec：解析规格\\n名字_编码[L]、digit=、Comment/Keyword 分类", "box"),
        ("re", "RegexParser + CharClassBuilder\\n正则式语法树 + 字符类等价类划分", "box"),
        ("nfa", "RegexToNfa：Thompson 构造\\n（NFA 状态转换表）", "box"),
        ("dfa", "NfaToDfa：子集构造 + ε 闭包\\n（DFA 状态转换表）", "box"),
        ("min", "DfaMinimize：划分法最小化", "box"),
        ("gen", "LexCodeGen：生成 C++ 词法分析程序\\n方法一 Judgechar / 方法二 switch(state)", "box"),
        ("build", "LexBuild：g++ 编译 → 运行生成的可执行程序", "box"),
        ("lex", "输出 sample.lex（单词编码文件）", "io"),
    ]
    edges = [
        edge("ui", "spec"), edge("spec", "re"), edge("re", "nfa"), edge("nfa", "dfa"),
        edge("dfa", "min"), edge("min", "gen"), edge("gen", "build"), edge("build", "lex"),
    ]
    flow("fig_dataflow_lex", "任务一处理流程", nodes, edges)

    nodes = [
        ("ui", "⑦ 文法编辑器（tiny_grammar.txt / minic_grammar.txt）", "box"),
        ("gram", "Grammar：载入 BNF、识别开始符号、增广 S'→S", "box"),
        ("simp", "GrammarSimplify：删不可达规则、删不能推导的规则", "box"),
        ("ff", "FirstFollow：First / Follow 集合", "box"),
        ("lr0", "LrItems：LR(0) 项目集规范族 + GOTO 函数", "box"),
        ("lalr", "Lr1Items：LR(1) 项目 → 同心集合并 → LALR(1)", "box"),
        ("table", "ParseTable：ACTION / GOTO 表 + 冲突报告", "box"),
        ("parse", "Parser：表驱动分析（读 .lex）+ 语义动作建树", "box"),
        ("tree", "语法树（树状呈现 / 文本 / DOT 导出）", "io"),
        ("ic", "IntermediateCode：四元式（选做）", "box"),
    ]
    edges = [
        edge("ui", "gram"), edge("gram", "simp"), edge("simp", "ff"), edge("ff", "lr0"),
        edge("lr0", "lalr"), edge("lalr", "table"), edge("table", "parse"), edge("parse", "tree"),
        edge("tree", "ic"),
        edge("lexsrc", "parse", "sample.lex", style="dashed"),
    ]
    nodes.append(("lexsrc", "任务一输出的 sample.lex", "note"))
    flow("fig_dataflow_syntax", "任务二处理流程", nodes, edges)

    # ---------------- 1.1 正则式解析 ----------------
    chain_chart("fig_1_1_regex", [
        ("开始", "start"),
        ("逐行读入规格文本，跳过空行与注释行", "box"),
        ("识别“名字_编码[L]”，登记编码与 L（连续编码）标志", "box"),
        ("按四类登记：Definition（宏）/ Comment / Keyword / Normal", "box"),
        ("宏定义 digit=、letter= 存入宏表，不参与自动机", "box"),
        ("对 Normal / Comment 的正则式做词法分析", "box"),
        ("转义 \\\\x 还原为单字符；[..] 展开为字符集原子", "box"),
        ("~x 记为补集；? 与 + 展开为基本运算的组合", "box"),
        ("递归展开命名宏（检测循环引用）；相邻操作数间补连接符“.”", "box"),
        ("按 闭包 > 连接 > 选择 的优先级转成逆波兰式", "io"),
        ("得到 (规则, 后缀式) 列表，并收集全部字符集原子", "box"),
        ("按“成员签名”把 256 个字节划成等价类，得到字符类字母表", "box"),
        ("结束", "start"),
    ])

    # ---------------- 1.2 Thompson 构造 NFA ----------------
    nodes = [
        ("s", "开始", "start"),
        ("a", "建立空栈 S 与全局状态计数器 i = 0", "box"),
        ("b", "对每条参与自动机的规则：追加其“或”于统一初态", "box"),
        ("c", "从左到右扫描后缀式，取当前符号 c", "box"),
        ("d", "c 是字符类原子？", "dec"),
        ("d1", "新建状态 i、i+1；对类中每个字节加边；压入 (i, i+1)；i += 2", "box"),
        ("e", "c 是连接符 . ？", "dec"),
        ("e1", "弹出 (s1,s2)、(s3,s4)；加边 s2→s3(ε)；压入 (s1,s4)", "box"),
        ("f", "c 是选择符 | ？", "dec"),
        ("f1", "弹出两个片段；新建 i、i+1；四条 ε 边汇合两片段", "box"),
        ("g", "c 是闭包 * ？", "dec"),
        ("g1", "弹出片段 (s1,s2)；新建 i、i+1；四条 ε 边形成回路", "box"),
        ("h", "还有符号？", "dec"),
        ("i", "取栈底片段 (start, accept)，把 accept 标为接受态\\n并记录该规则的单词编码", "box"),
        ("j", "NFA 状态转换表（含 ε 边）+ 邻接表输出", "io"),
        ("end", "结束", "start"),
    ]
    edges = [
        edge("s", "a"), edge("a", "b"), edge("b", "c"),
        edge("c", "d"), edge("d", "d1", "是"), edge("d", "e", "否"),
        edge("d1", "h"), edge("e", "e1", "是"), edge("e", "f", "否"),
        edge("e1", "h"), edge("f", "f1", "是"), edge("f", "g", "否"),
        edge("f1", "h"), edge("g", "g1", "是"), edge("g", "c", "否（原子）"),
        edge("g1", "h"), edge("h", "c", "是，继续"), edge("h", "i", "否"),
        edge("i", "j"), edge("j", "end"),
    ]
    flow("fig_1_2_nfa", "Thompson 构造 NFA", nodes, edges)

    # ---------------- 1.3 子集构造 ----------------
    nodes = [
        ("s", "开始", "start"),
        ("a", "Dstates ← { ε-closure({NFA 初态}) }，作为未标记的唯一 DFA 状态", "box"),
        ("b", "与 NFA 合并后的字符类字母表一起传入", "box"),
        ("c", "有无未标记的 DFA 状态 T？", "dec"),
        ("d", "标记 T", "box"),
        ("e", "取下一个字符类 a 与 T 中的 NFA 状态作 move(T, a)", "box"),
        ("f", "U = ε-closure(move(T, a))", "box"),
        ("g", "U 非空？", "dec"),
        ("h", "U 已在 Dstates 中？", "dec"),
        ("h1", "把 U 作为新 DFA 状态加入 Dstates（未标记）", "box"),
        ("i", "记录转移 δ(T, a) = U；若 U 含接受态则继承其单词编码", "box"),
        ("j", "还有字符类？", "dec"),
        ("k", "DFA 状态转换表：状态 / 字符类列 / 接受编码", "io"),
        ("end", "结束", "start"),
    ]
    edges = [
        edge("s", "a"), edge("a", "b"), edge("b", "c"),
        edge("c", "d", "是"), edge("d", "e"), edge("e", "f"), edge("f", "g"),
        edge("g", "h", "是"), edge("g", "j", "否"), edge("h", "h1", "否"),
        edge("h", "i", "是"), edge("h1", "i"), edge("i", "j"),
        edge("j", "e", "是"), edge("j", "c", "否"), edge("c", "k", "否"),
        edge("k", "end"),
    ]
    flow("fig_1_3_dfa", "子集构造法构造 DFA", nodes, edges)

    # ---------------- 1.4 最小化 DFA ----------------
    nodes = [
        ("s", "开始", "start"),
        ("a", "把状态划分为“接受组”与“非接受组”两个初态组", "box"),
        ("b", "接受组内按单词编码再细分（不同编码不可合并）", "box"),
        ("c", "当前划分 P = {P1, P2, …, Pn}", "box"),
        ("d", "取一个组 G，逐个字符类检查组内状态的转移目标落在哪个组", "box"),
        ("e", "组内状态可再分？", "dec"),
        ("e1", "按转移目标所在组把 G 拆成若干子组，替换进 P", "box"),
        ("f", "还有未检查的组？", "dec"),
        ("g", "本次扫描有拆分发生？", "dec"),
        ("h", "每个组选一个代表状态，重写转移与接受编码", "box"),
        ("i", "最小化 DFA（状态数最少）+ 划分过程记录", "io"),
        ("end", "结束", "start"),
    ]
    edges = [
        edge("s", "a"), edge("a", "b"), edge("b", "c"), edge("c", "d"), edge("d", "e"),
        edge("e", "e1", "是"), edge("e1", "f"), edge("e", "f", "否"),
        edge("f", "d", "是"), edge("f", "g", "否"), edge("g", "c", "是（继续细化）"),
        edge("g", "h", "否"), edge("h", "i"), edge("i", "end"),
    ]
    flow("fig_1_4_min", "划分法最小化 DFA", nodes, edges)

    # ---------------- 1.5 词法分析源程序生成 ----------------
    nodes = [
        ("s", "开始", "start"),
        ("a", "取出最小化 DFA 的状态转换表与字符类字母表", "box"),
        ("b", "按选择的方法生成骨架：方法一 Judgechar/GetNext/GetToken\\n或方法二 switch(state)", "box"),
        ("c", "为每个字符类生成 charIndex()：按区间判定返回类下标", "box"),
        ("d", "生成识别循环：读字符 → charIndex → 查转移 → 更新 state", "box"),
        ("e", "进入死状态且上次处于接受态？", "dec"),
        ("f", "回退到最后一个接受状态的位置（最长匹配）", "box"),
        ("g", "按该状态携带的编码输出 <code> <value>；\\n注释规则不输出；ID 加双引号", "box"),
        ("h", "识别关键字：查关键字表把 ID 改写为关键字编码", "box"),
        ("i", "还有输入字符？", "dec"),
        ("j", "输出一行 EOF；关闭输出文件", "io"),
        ("k", "生成的 C++ 源程序经 g++ 编译为可执行文件", "box"),
        ("end", "结束", "start"),
    ]
    edges = [
        edge("s", "a"), edge("a", "b"), edge("b", "c"), edge("c", "d"), edge("d", "e"),
        edge("e", "f", "是"), edge("e", "g", "否：按当前接受态输出"),
        edge("f", "g"), edge("g", "h"), edge("h", "i"),
        edge("i", "d", "是"), edge("i", "j", "否"), edge("j", "k"), edge("k", "end"),
    ]
    flow("fig_1_5_codegen", "词法分析源程序生成与最长匹配", nodes, edges)

    # ---------------- 2.1 文法化简 ----------------
    nodes = [
        ("s", "开始", "start"),
        ("a", "载入 BNF：左部 -> 右部1 | 右部2；首非终结符即开始符号", "io"),
        ("b", "自动增广：追加 S' -> S（已有则跳过）", "box"),
        ("c", "标记所有能推出纯终结符串的非终结符（迭代到不动点）", "box"),
        ("d", "删除右部含“未标记非终结符”的产生式；重复直到不再变化", "box"),
        ("e", "从开始符号出发，标记所有可达的非终结符", "box"),
        ("f", "删除左部不可达的产生式", "box"),
        ("g", "有规则被删除？", "dec"),
        ("h", "重新计算 First/Follow 所需的符号表", "box"),
        ("i", "化简后的文法 + 被删除规则清单（含原因）", "io"),
        ("end", "结束", "start"),
    ]
    edges = [
        edge("s", "a"), edge("a", "b"), edge("b", "c"), edge("c", "d"), edge("d", "e"),
        edge("e", "f"), edge("f", "g"), edge("g", "c", "是：再化简一轮"),
        edge("g", "h", "否"), edge("h", "i"), edge("i", "end"),
    ]
    flow("fig_2_1_simplify", "文法化简", nodes, edges)

    # ---------------- 2.2 First / Follow ----------------
    chain_chart("fig_2_2_firstfollow", [
        ("开始", "start"),
        ("First(X) 置空；若 X 是终结符则 First(X) = {X}", "box"),
        ("对每条产生式 X → Y1Y2…Yn，自左向右扫描右部", "box"),
        ("把 First(Yi) \\\\ {ε} 并入 First(X)", "box"),
        ("Yi 能推出 ε？能则继续看 Yi+1，不能则停止本产生式", "dec"),
        ("右部全部可空时，把 ε 并入 First(X)", "box"),
        ("本轮 First 有变化则迭代（直到不动点）", "box"),
        ("Follow(S) 加入输入结束符；对 A → αBβ，把 First(β)\\\\{ε} 并入 Follow(B)", "box"),
        ("β 为空或可空时，把 Follow(A) 并入 Follow(B)", "box"),
        ("本轮 Follow 有变化则迭代（直到不动点）", "box"),
        ("输出两张表：非终结符 → First / Follow 集合", "io"),
        ("结束", "start"),
    ])

    # ---------------- 2.3 LR(0) 项目集 ----------------
    nodes = [
        ("s", "开始", "start"),
        ("a", "I0 = closure({ S' -> ·S })", "box"),
        ("b", "项目集规范族 C = { I0 }，待处理队列 = [I0]", "box"),
        ("c", "取出一个项目集 I", "box"),
        ("d", "对 I 中每个“· ”后面是符号 X 的项目求 GOTO(I, X)", "box"),
        ("e", "GOTO 结果 = closure(所有 A -> αX·β 的项目)", "box"),
        ("f", "闭包：把 B -> ·γ 加入，直到不再变化（此处不含向前看符号）", "box"),
        ("g", "该状态已存在？", "dec"),
        ("h", "新建 DFA 状态并加入 C 与待处理队列", "box"),
        ("i", "记录转移 δ(I, X) = 新状态（X 可为终结符或非终结符）", "box"),
        ("j", "还有项目集？", "dec"),
        ("k", "LR(0) 项目集族 + 状态转移表 / DFA 图", "io"),
        ("end", "结束", "start"),
    ]
    edges = [
        edge("s", "a"), edge("a", "b"), edge("b", "c"), edge("c", "d"), edge("d", "e"),
        edge("e", "f"), edge("f", "g"), edge("g", "h", "否"), edge("h", "i"),
        edge("g", "i", "是"), edge("i", "j"), edge("j", "c", "是"), edge("j", "k", "否"),
        edge("k", "end"),
    ]
    flow("fig_2_3_lr0", "LR(0) 项目集规范族与 GOTO", nodes, edges)

    # ---------------- 2.4 LALR(1) ----------------
    nodes = [
        ("s", "开始", "start"),
        ("a", "以 I0 = closure({ [S' -> ·S, $] }) 开始", "box"),
        ("b", "闭包传播：对 [A -> α·Bβ, a]，对 FIRST(βa) 中每个 b 加入 [B -> ·γ, b]", "box"),
        ("c", "求 GOTO(I, X)：对每个 [A -> α·Xβ, a] 得到 [A -> αX·β, a]，再求闭包", "box"),
        ("d", "新建 LR(1) 项目集并入队", "box"),
        ("e", "还有未处理 LR(1) 项目集？", "dec"),
        ("f", "合并“同心集”：核心项目（去掉向前看串）相同的集合合并为一个 LALR(1) 状态", "box"),
        ("g", "合并后向前看符号取并集；若并集改变需重新传播到不动点", "box"),
        ("h", "按合并结果重写状态转移表", "box"),
        ("i", "LALR(1) 项目集族（与 LR(1) 对照展示）+ 转移表", "io"),
        ("end", "结束", "start"),
    ]
    edges = [
        edge("s", "a"), edge("a", "b"), edge("b", "c"), edge("c", "d"), edge("d", "e"),
        edge("e", "c", "是"), edge("e", "f", "否"), edge("f", "g"), edge("g", "h"),
        edge("h", "i"), edge("i", "end"),
    ]
    flow("fig_2_4_lalr", "LR(1) 项目集与 LALR(1) 同心集合并", nodes, edges)

    # ---------------- 2.5 分析表 ----------------
    nodes = [
        ("s", "开始", "start"),
        ("a", "取 LALR(1) 项目集族、GOTO 转移与 Follow 集合", "box"),
        ("b", "对每个项目集 I 的每个项目：", "box"),
        ("c", "项目为 [A -> α·aβ, b] 且 a 是终结符？", "dec"),
        ("c1", "ACTION[I, a] = 移进 GOTO(I, a) 得到的状态号", "box"),
        ("d", "项目为 [A -> α·, b]？", "dec"),
        ("d1", "ACTION[I, b] = 规约 A -> α（产生式编号）", "box"),
        ("e", "项目为 [S' -> S·, $]？", "dec"),
        ("e1", "ACTION[I, $] = 接受（acc）", "box"),
        ("f", "非终结符 X 的 GOTO(I, X) 是否存在？", "dec"),
        ("f1", "GOTO[I, X] = GOTO(I, X)", "box"),
        ("g", "同一表格出现两个动作？", "dec"),
        ("g1", "记录冲突：状态、符号、两条候选；\\n按“移进优先 / 编号小优先”消解并标注", "box"),
        ("h", "输出 ACTION / GOTO 二维表 + 冲突明细", "io"),
        ("end", "结束", "start"),
    ]
    edges = [
        edge("s", "a"), edge("a", "b"), edge("b", "c"), edge("c", "c1", "是"),
        edge("c", "d", "否"), edge("c1", "g"), edge("d", "d1", "是"), edge("d", "e", "否"),
        edge("d1", "g"), edge("e", "e1", "是"), edge("e", "f", "否"), edge("e1", "g"),
        edge("f", "f1", "是"), edge("f", "g", "否"), edge("f1", "g"),
        edge("g", "g1", "是"), edge("g", "h", "否"), edge("g1", "h"), edge("h", "end"),
    ]
    flow("fig_2_5_table", "LALR(1) 分析表构造与冲突处理", nodes, edges)

    # ---------------- 2.6 表驱动分析 + 建树 ----------------
    nodes = [
        ("s", "开始", "start"),
        ("a", "读入 .lex：每行 <code> <value>，末行为 EOF", "io"),
        ("b", "把编码翻译成文法里的终结符名字（101 → ID）", "box"),
        ("c", "状态栈 = [0]、符号栈 = []、结点栈 = []；输入指针指向首符号", "box"),
        ("e", "取 state = 栈顶状态、a = 当前输入符号，查 ACTION[state, a]", "box"),
        ("d", "动作为移进 / 规约 / 接受 / 报错？", "dec"),
        ("act", "移进：压状态与符号，建叶结点\\n规约 A→β：弹 |β| 项，按语义动作建内部结点，查 GOTO 压栈\\n"
                "接受：返回根结点　报错：输出位置与期望符号集合", "box"),
        ("more", "输入读完了吗？", "dec"),
        ("i", "语法树根结点 → 树状绘制 / 文本 / DOT 导出", "io"),
        ("end", "结束", "start"),
    ]
    edges = [
        edge("s", "a"), edge("a", "b"), edge("b", "c"), edge("c", "e"), edge("e", "d"),
        edge("d", "act"), edge("act", "more"), edge("more", "e", "否：继续分析"),
        edge("more", "i", "是"), edge("i", "end"),
    ]
    flow("fig_2_6_parse", "表驱动语法分析与语法树构造", nodes, edges)

    # ---------------- 2.7 四元式 ----------------
    nodes = [
        ("s", "开始", "start"),
        ("a", "以语法树根结点为输入，做归纳翻译", "box"),
        ("b", "按结点标签分派：变量定义 / 赋值 / 表达式 / 控制流 / 函数边界", "box"),
        ("c", "二元运算：先求左右子表达式的值，再发射 (op, 左, 右, 临时变量)", "box"),
        ("d", "赋值：发射 (=, 右值, -, 变量名)", "box"),
        ("e", "if / while / for：发射条件跳转与转移四元式（(j<,), (j,), (label,)）", "box"),
        ("f", "函数定义 / 调用：发射 (func, 名, 参数个数, -) 与 (call, 名, 实参个数, 临时)", "box"),
        ("g", "还有未翻译的结点？", "dec"),
        ("h", "输出四元式表（序号, op, arg1, arg2, result）+ 文本/TSV 导出", "io"),
        ("end", "结束", "start"),
    ]
    edges = [
        edge("s", "a"), edge("a", "b"), edge("b", "c"), edge("c", "d"), edge("d", "e"),
        edge("e", "f"), edge("f", "g"), edge("g", "b", "是"), edge("g", "h", "否"),
        edge("h", "end"),
    ]
    flow("fig_2_7_ir", "四元式中间代码生成", nodes, edges)


def main():
    if not os.path.exists(DOT):
        print(f"找不到 Graphviz：{DOT}", file=sys.stderr)
        return 1
    print("开始渲染图形：")
    build()
    print("完成。")
    return 0


if __name__ == "__main__":
    sys.exit(main())
