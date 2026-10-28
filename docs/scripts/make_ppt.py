# -*- coding: utf-8 -*-
"""生成讲解视频用的 PPT（16 页，16:9）与逐页讲稿。

设计原则（对应用户要求“只挑重点”）：
  * 每页一个**结论式标题** + 至多 3 条要点 + 一个视觉（大数字 / 截图 / 图）；
  * 需求清单全表、五段式细节、68 条用例等“体量大但不必口播”的内容一律留在报告里；
  * 每页备注写 120~180 字口播稿，全篇约 2500 字（≈13 分钟），另留 1~2 分钟给实机演示。

用法：python make_ppt.py
输出：docs/讲解PPT.pptx、docs/讲解脚本.md
"""

import os
import sys

from PIL import Image
from lxml import etree
from pptx import Presentation
from pptx.dml.color import RGBColor
from pptx.enum.shapes import MSO_SHAPE
from pptx.enum.text import MSO_ANCHOR, MSO_AUTO_SIZE, PP_ALIGN
from pptx.oxml.ns import qn
from pptx.util import Inches, Pt

HERE = os.path.dirname(os.path.abspath(__file__))
DOCS = os.path.dirname(HERE)
IMG = os.path.join(DOCS, "images")
PPTX = os.path.join(DOCS, "讲解PPT.pptx")
SCRIPT = os.path.join(DOCS, "讲解脚本.md")

FONT = "微软雅黑"
MONO = "Consolas"
BLUE = RGBColor(0x1A, 0x5F, 0xB4)
DARK = RGBColor(0x26, 0x26, 0x26)
GRAY = RGBColor(0x59, 0x59, 0x59)
LGRAY = RGBColor(0xF2, 0xF2, 0xF2)
RED = RGBColor(0xC0, 0x39, 0x2B)
WHITE = RGBColor(0xFF, 0xFF, 0xFF)

SW, SH = 13.333, 7.5          # 16:9（英寸）
MARGIN = 0.9
TOTAL = 16


# ---------------------------------------------------------------------------
# 基础工具
# ---------------------------------------------------------------------------
def set_run(run, size=18, bold=False, color=DARK, font=FONT):
    f = run.font
    f.size = Pt(size)
    f.bold = bold
    f.color.rgb = color
    f.name = font
    rpr = run._r.get_or_add_rPr()
    for tag in ("a:ea", "a:cs"):
        el = rpr.find(qn(tag))
        if el is None:
            el = etree.SubElement(rpr, qn(tag))
        el.set("typeface", font)


def textbox(slide, left, top, width, height, text, size=18, bold=False, color=DARK,
            align=PP_ALIGN.LEFT, anchor=MSO_ANCHOR.TOP, font=FONT, spacing=1.15,
            space_after=10, wrap=True):
    box = slide.shapes.add_textbox(Inches(left), Inches(top), Inches(width), Inches(height))
    tf = box.text_frame
    tf.word_wrap = wrap
    tf.auto_size = MSO_AUTO_SIZE.NONE
    tf.vertical_anchor = anchor
    tf.margin_left = 0
    tf.margin_right = 0
    tf.margin_top = 0
    tf.margin_bottom = 0
    lines = text.split("\n") if isinstance(text, str) else list(text)
    for i, line in enumerate(lines):
        p = tf.paragraphs[0] if i == 0 else tf.add_paragraph()
        p.alignment = align
        p.line_spacing = spacing
        p.space_after = Pt(space_after)
        _write_marked(p, line, size, bold, color, font)
    return box


def _write_marked(paragraph, line, size, bold, color, font):
    """支持 `**加粗**` 标记。"""
    parts = line.split("**")
    for i, seg in enumerate(parts):
        if not seg:
            continue
        r = paragraph.add_run()
        r.text = seg
        set_run(r, size=size, bold=(bold or i % 2 == 1), color=color, font=font)


def rect(slide, left, top, width, height, fill, line=None, shape=MSO_SHAPE.RECTANGLE):
    sp = slide.shapes.add_shape(shape, Inches(left), Inches(top), Inches(width), Inches(height))
    sp.fill.solid()
    sp.fill.fore_color.rgb = fill
    if line is None:
        sp.line.fill.background()
    else:
        sp.line.color.rgb = line
        sp.line.width = Pt(0.75)
    sp.shadow.inherit = False
    return sp


def title_bar(slide, title, kicker=None):
    rect(slide, 0, 0, SW, 1.02, BLUE)
    textbox(slide, MARGIN, 0.22, SW - MARGIN * 2, 0.6, title, size=27, bold=True,
            color=WHITE, anchor=MSO_ANCHOR.MIDDLE)
    if kicker:
        textbox(slide, MARGIN, 1.12, SW - MARGIN * 2, 0.35, kicker, size=13, color=GRAY)


def footer(slide, idx):
    rect(slide, MARGIN, SH - 0.62, SW - MARGIN * 2, 0.012, RGBColor(0xD0, 0xD0, 0xD0))
    textbox(slide, MARGIN, SH - 0.55, 9.5, 0.3,
            "《编译原理》课程设计 · 词法分析器与 LALR(1) 语法分析器生成系统",
            size=10, color=GRAY)
    textbox(slide, SW - MARGIN - 1.4, SH - 0.55, 1.4, 0.3, f"{idx} / {TOTAL}", size=10,
            color=GRAY, align=PP_ALIGN.RIGHT)


def bullets(slide, items, top=1.75, left=MARGIN, width=6.1, size=19, gap=0.72):
    for i, item in enumerate(items):
        textbox(slide, left, top + i * gap, width, gap, "▍ " + item, size=size,
                color=DARK, spacing=1.1)


def big_numbers(slide, pairs, top=2.0, left=MARGIN, width=None, num_size=54, label_size=14,
                per_row=3, row_gap=1.55):
    """pairs: [(数字, 说明), …] 横向排布。"""
    if width is None:
        width = SW - MARGIN * 2
    cell = width / per_row
    for i, (num, label) in enumerate(pairs):
        r, c = divmod(i, per_row)
        x = left + c * cell
        y = top + r * row_gap
        textbox(slide, x, y, cell - 0.2, 0.85, num, size=num_size, bold=True, color=BLUE,
                align=PP_ALIGN.CENTER, spacing=0.95)
        textbox(slide, x, y + 0.85, cell - 0.2, 0.6, label, size=label_size, color=GRAY,
                align=PP_ALIGN.CENTER, spacing=1.05)


def image_fit(slide, name, left, top, max_w, max_h, caption=None, cap_size=12):
    path = os.path.join(IMG, name)
    with Image.open(path) as im:
        iw, ih = im.size
    ratio = min(max_w / iw, max_h / ih)
    w, h = iw * ratio, ih * ratio
    x = left + (max_w - w) / 2
    y = top + (max_h - h) / 2
    slide.shapes.add_picture(path, Inches(x), Inches(y), Inches(w), Inches(h))
    if caption:
        textbox(slide, left, top + max_h + 0.06, max_w, 0.3, caption, size=cap_size,
                color=GRAY, align=PP_ALIGN.CENTER)
    return x, y, w, h


def code_box(slide, left, top, width, height, lines, size=13):
    rect(slide, left, top, width, height, LGRAY, line=RGBColor(0xDD, 0xDD, 0xDD))
    textbox(slide, left + 0.22, top + 0.18, width - 0.44, height - 0.36,
            "\n".join(lines), size=size, color=DARK, font=MONO, spacing=1.0, space_after=2)


def table_box(slide, left, top, width, rows, col_ratios, size=13, row_h=0.55, header=True):
    n_rows = len(rows)
    shape = slide.shapes.add_table(n_rows, len(rows[0]), Inches(left), Inches(top),
                                   Inches(width), Inches(row_h * n_rows))
    tbl = shape.table
    for i, ratio in enumerate(col_ratios):
        tbl.columns[i].width = Inches(width * ratio)
    for r, row in enumerate(rows):
        tbl.rows[r].height = Inches(row_h)
        for c, value in enumerate(row):
            cell = tbl.cell(r, c)
            cell.margin_left = Inches(0.08)
            cell.margin_right = Inches(0.08)
            cell.margin_top = Inches(0.02)
            cell.margin_bottom = Inches(0.02)
            cell.vertical_anchor = MSO_ANCHOR.MIDDLE
            cell.fill.solid()
            if header and r == 0:
                cell.fill.fore_color.rgb = BLUE
                color, bold = WHITE, True
            else:
                cell.fill.fore_color.rgb = WHITE if r % 2 else LGRAY
                color, bold = DARK, False
            tf = cell.text_frame
            tf.word_wrap = True
            p = tf.paragraphs[0]
            p.alignment = PP_ALIGN.CENTER if (c == 0 or len(str(value)) < 8) else PP_ALIGN.LEFT
            run = p.add_run()
            run.text = str(value)
            set_run(run, size=size, bold=bold, color=color)
    return tbl


def slide_new(prs):
    return prs.slides.add_slide(prs.slide_layouts[6])


def notes(slide, text):
    slide.notes_slide.notes_text_frame.text = text


# ---------------------------------------------------------------------------
# 16 页内容
# ---------------------------------------------------------------------------
def s01_cover(prs, idx):
    s = slide_new(prs)
    rect(s, 0, 0, 0.28, SH, BLUE)
    textbox(s, 1.25, 1.15, 10.8, 0.5, "华南师范大学计算机学院 · 编译原理课程设计", size=16, color=GRAY)
    textbox(s, 1.25, 1.85, 11.0, 1.0, "编译原理课程设计", size=46, bold=True, color=DARK)
    textbox(s, 1.25, 2.9, 11.2, 1.5, "词法分析器与 LALR(1) 语法分析器\n生成系统的设计与实现",
            size=30, bold=True, color=BLUE, spacing=1.2)
    rect(s, 1.25, 4.45, 2.2, 0.05, BLUE)
    textbox(s, 1.25, 4.75, 11.0, 1.5,
            "姓　　名：【请填写】　　　学　　号：【请填写】\n"
            "班　　级：【请填写】　　　指导教师：【请填写】",
            size=17, color=DARK, spacing=1.5)
    textbox(s, 1.25, 6.45, 11.0, 0.4,
            "C++17 · Qt 6.10.2 · MinGW GCC 13.1.0 · 2026 年 12 月", size=13, color=GRAY)
    notes(s, "老师好，我是【姓名】。我做的题目是「词法分析器与 LALR(1) 语法分析器生成系统」，"
             "任务书里两项必做任务、第三项 mini-C 整体测试和选做的四元式都做完了。"
             "接下来 15 分钟，我不逐条念功能，只讲三件事：为什么这么设计、关键算法怎么落地、"
             "以及怎么证明它是对的。中间会有两段实机演示。")
    footer(s, idx)
    return s


def s02_agenda(prs, idx):
    s = slide_new(prs)
    title_bar(s, "今天只讲四件事")
    items = [
        ("①  任务与完成度", "1 分钟", "两项任务 + mini-C + 选做，全部完成"),
        ("②  三个关键决定", "5 分钟", "字符类字母表 / LALR(1) 项目集 / 表驱动建树"),
        ("③  系统演示", "3 分钟", "词法一键流水线、语法三步走"),
        ("④  测试与总结", "3 分钟", "304 + 346 条断言，16 条缺陷闭环"),
    ]
    for i, (head, dur, desc) in enumerate(items):
        col = i % 2
        row = i // 2
        x = MARGIN + col * 5.85
        y = 1.75 + row * 2.55
        rect(s, x, y, 5.4, 2.15, LGRAY)
        rect(s, x, y, 0.1, 2.15, BLUE)
        textbox(s, x + 0.35, y + 0.22, 4.8, 0.5, head, size=22, bold=True, color=DARK)
        textbox(s, x + 0.35, y + 0.78, 4.8, 0.34, dur, size=14, bold=True, color=BLUE)
        textbox(s, x + 0.35, y + 1.2, 4.8, 0.7, desc, size=15, color=GRAY, spacing=1.2)
    notes(s, "我按四个部分讲：第一部分花 1 分钟交代做了什么；第二部分是重点，用 5 分钟讲三个"
             "关键技术决定；第三部分 3 分钟演示系统；最后 3 分钟讲测试结果和我认为的不足。")
    footer(s, idx)
    return s


def s03_tasks(prs, idx):
    s = slide_new(prs)
    title_bar(s, "任务书要求：两项必做 + mini-C + 选做，全部完成")
    rows = [
        ["任务", "任务书要求", "对应实现", "状态"],
        ["第一项", "1.1~1.11  词法分析生成器", "①~⑥ 六个页签，两种转换方法", "完成"],
        ["第二项", "2.1~2.10  LALR(1) 语法分析", "⑦~⑭ 八个页签，含语义动作编辑器", "完成"],
        ["第三项", "以 mini-C 做整体测试", "自编 sample.mc（79 个单词）", "完成"],
        ["选做", "2.11  生成中间代码", "⑮ 四元式（TINY 40 条 / mini-C 27 条）", "完成"],
    ]
    table_box(s, MARGIN, 1.5, SW - MARGIN * 2, rows, [0.12, 0.34, 0.4, 0.14], size=14, row_h=0.72)
    textbox(s, MARGIN, 5.35, SW - MARGIN * 2, 1.2,
            "**测试顺序严格按任务书**：先用 TINY 的词法与语法各测一轮，再用 mini-C 做全系统测试。",
            size=17, color=DARK)
    notes(s, "先看完成度。任务书第一项 11 条、第二项 11 条要求，加上第三项 mini-C 测试和选做的"
             "中间代码，全部实现，界面上一共 15 个功能页签，与要求一一对应。"
             "测试顺序也按任务书来：先 TINY 后 mini-C。")
    footer(s, idx)
    return s


def s04_arch(prs, idx):
    s = slide_new(prs)
    title_bar(s, "三层分离：算法层不认识界面，所以能自动验证")
    image_fit(s, "fig_structure.png", MARGIN, 1.45, 6.6, 4.6, "系统总体结构：界面层 / 任务层 / 公共层")
    bullets(s, [
        "**算法只依赖 QtCore**，不出现任何 QWidget",
        "界面只做「取参数 → 调算法 → 显示结果」",
        "自测走**与按钮点击完全相同**的代码路径",
        "15 个页签顺序由断言 G1b 守住",
    ], top=1.55, left=7.75, width=4.9, size=17, gap=0.95)
    notes(s, "整个系统分三层。最上面是界面层，中间是两条任务流水线，下面是公共层。"
             "这里有一个刻意的决定：算法层只依赖 QtCore，一行界面代码都没有。"
             "带来的好处是它能脱离窗口跑，所以我可以把全部算法写成命令行断言；"
             "而界面自测又是直接调用按钮背后的同一个函数，等于同一份逻辑被验证了两次。")
    footer(s, idx)
    return s


def s05_class(prs, idx):
    s = slide_new(prs)
    title_bar(s, "关键决定一：把 256 个字节压成 18 个字符类", "按“正则里写了哪些字符集”做等价类划分")
    big_numbers(s, [("256 → 18", "输入符号（字符类）"), ("1563 → 67", "NFA 状态数"),
                    ("256 / 256", "转移逐一比对一致")], top=1.7, num_size=40, row_gap=1.3)
    image_fit(s, "fig_1_3_dfa.png", 7.7, 1.55, 4.9, 4.3, "子集构造（ε 闭包 + move）")
    bullets(s, [
        "正则里每一处字符集 = 一个**原子**（宏、`[..]`、转义字符、`~x`）",
        "两个字节若属于**完全相同的原子集合**，在所有状态下转移都相同",
        "于是合并成**一个输入符号**，状态表从 258 列降到 20 列",
    ], top=3.35, left=MARGIN, width=6.4, size=16, gap=0.85)
    notes(s, "这是我认为最关键的一个决定。任务书里 letter=[A-Za-z] 是 52 个字符，如果按 256 个"
             "字节建自动机，子集构造要 31 秒，状态表 258 列根本没法看。"
             "我的做法是把正则里每一处字符集当成一个「原子」，两个字节只要在所有原子里出现的情况"
             "完全相同，它们在自动机里就是同一个输入符号。TINY 压成了 18 个类，NFA 从 1563 个"
             "状态降到 67 个。这不是近似——我用 256 个转移逐项比对过，识别语言完全一样。")
    footer(s, idx)
    return s


def s06_pipeline(prs, idx):
    s = slide_new(prs)
    title_bar(s, "一条流水线走完词法：67 → 26 → 24")
    steps = [("67", "NFA"), ("26", "DFA"), ("24", "最小化 DFA")]
    for i, (num, label) in enumerate(steps):
        x = MARGIN + i * 3.1
        rect(s, x, 1.7, 2.5, 1.55, LGRAY)
        textbox(s, x, 1.85, 2.5, 0.8, num, size=40, bold=True, color=BLUE, align=PP_ALIGN.CENTER)
        textbox(s, x, 2.62, 2.5, 0.4, label, size=14, color=GRAY, align=PP_ALIGN.CENTER)
        if i < 2:
            textbox(s, x + 2.5, 2.1, 0.6, 0.6, "→", size=30, bold=True, color=GRAY,
                    align=PP_ALIGN.CENTER)
    image_fit(s, "fig_dataflow_lex.png", 9.6, 1.5, 2.75, 4.6, "词法流水线", cap_size=11)
    bullets(s, [
        "**子集构造**：ε 闭包 + move，按字符类迭代，状态按发现顺序编号",
        "**最小化**：划分法；接受态**按单词编码再细分**，不同编码不可合并",
        "划分过程与合并关系都显示在④页签，可逐轮对照",
    ], top=3.75, left=MARGIN, width=8.4, size=17, gap=0.9)
    notes(s, "流水线本身是教科书流程：Thompson 构造出 67 个状态的 NFA，子集构造收缩到 26 个，"
             "划分法再压到 24 个。这里只有一个容易做错的地方：最小化时不能只按「接受/非接受」分组，"
             "否则编码 100 和 101 的接受态会被合并，识别结果就错了。所以初始划分里我又按编码细分了一次。"
             "④ 页签把每一轮划分都列出来了，可以对着看。")
    footer(s, idx)
    return s


def s07_codegen(prs, idx):
    s = slide_new(prs)
    title_bar(s, "生成的词法程序：最长匹配 + 关键字查表")
    code_box(s, MARGIN, 1.5, 6.5, 2.35, [
        "109            ← read（关键字编码）",
        '101 "xXx"      ← 标识符，值带引号',
        "215            ← :=",
        "100 001        ← 整数，保留前导零",
        "EOF",
    ], size=13)
    textbox(s, MARGIN, 4.0, 6.5, 0.35, "sample.lex（两项任务之间的唯一接口）", size=12, color=GRAY)
    bullets(s, [
        "两种转换方法**都实现**：Judgechar/GetToken 与 switch(state)",
        "走不动时**回退到最后一个接受态**（最长匹配）",
        "ID 再**查关键字表改判**，注释不输出",
        "生成的程序自包含，用命令行参数，换目录也能跑",
    ], top=1.5, left=7.75, width=4.8, size=16, gap=1.05)
    notes(s, "词法程序的输出格式我冻结成「编码 + 值」：ID 带引号、NUM 保留前导零、关键字和符号"
             "只写编码、注释不输出、末行 EOF。任务书要求转换方法一或方法二二选一，我把两种都实现了，"
             "可以对照。识别时按最长匹配走，关键字最后用查表把 ID 改判成关键字编码，"
             "这样 if 就不会被当成标识符。")
    footer(s, idx)
    return s


def s08_demo_lex(prs, idx):
    s = slide_new(prs)
    title_bar(s, "演示①：TINY 词法一键跑通", "① 打开规格 → 一键流水线 → ④ 看最小化 → ⑥ 看 .lex")
    image_fit(s, "tab04_mindfa.png", MARGIN, 1.6, 5.6, 4.3,
              "④ 最小化 DFA：状态表 + 划分过程与合并关系")
    image_fit(s, "tab06_lextest.png", 6.8, 1.6, 5.6, 4.3,
              "⑥ 编译与词法测试：编译输出 / 运行输出 / .lex 内容")
    textbox(s, MARGIN, 6.15, SW - MARGIN * 2, 0.5,
            "TINY 样例 96 个单词；mini-C 样例 79 个单词，均与老师给的参考输出**逐词一致**。",
            size=15, color=BLUE, align=PP_ALIGN.CENTER)
    notes(s, "【此处切到系统实机演示，约 60~90 秒】操作顺序：选 TINY，点工具栏「完整词法流水线」，"
             "程序会自动解析规格、构造 NFA/DFA、最小化、生成源码、调 g++ 编译并跑 sample.tny。"
             "先看 ④ 页签的合并关系，再看 ⑥ 页签的单词编码——96 行，末行 EOF。"
             "演示完回到 PPT。")
    footer(s, idx)
    return s


def s09_lalr(prs, idx):
    s = slide_new(prs)
    title_bar(s, "关键决定二：LR(1) 再合并同心集，得到 LALR(1)", "任务书要求 LALR(1)，不是 SLR(1)")
    big_numbers(s, [("58", "LALR(1) 状态"), ("357 / 75", "ACTION / GOTO 表项"), ("0", "冲突数")],
                top=1.6, num_size=40, row_gap=1.3)
    image_fit(s, "fig_2_4_lalr.png", 8.0, 1.5, 4.6, 4.4, "LR(1) 项目集 → 同心集合并", cap_size=11)
    bullets(s, [
        "LR(0) 项目 → 加**向前看符号**变 LR(1) → **同核心项目集合并**变 LALR(1)",
        "合并后向前看取并集，需**传播到不动点**",
        "⑫ 页签可切 **LR(0) / SLR(1) / LALR(1)** 对照：只有 LALR(1) 无冲突",
    ], top=3.3, left=MARGIN, width=6.6, size=16, gap=0.92)
    notes(s, "第二项任务的分量在这里。任务书要的是 LALR(1)。我的实现是完整的 LR(1)：每个项目带"
             "向前看符号，然后把核心相同的项目集合并成 LALR(1)，合并后向前看取并集还要继续传播"
             "到不动点。TINY 最后是 58 个状态，ACTION 357 项、GOTO 75 项，**没有冲突**。"
             "为了说明这不是运气，我又实现了 LR(0) 和 SLR(1) 两种填表方式，在 ⑫ 页签可以切换对比，"
             "它们是有冲突的。")
    footer(s, idx)
    return s


def s10_parsetable(prs, idx):
    s = slide_new(prs)
    title_bar(s, "分析表可用：298 步分析，建出 76 个结点的树")
    image_fit(s, "tab12_parsetable.png", MARGIN, 1.55, 7.1, 4.5, "⑫ ACTION/GOTO 表与冲突明细")
    big_numbers(s, [("298", "分析步数"), ("76", "语法树结点"), ("69", "叶结点")],
                top=1.7, left=8.3, width=4.2, num_size=32, label_size=12, per_row=1,
                row_gap=1.45)
    bullets(s, [
        "表驱动：移进 / 规约 / 接受 / 报错四类动作",
        "**语义动作文件**定义每条文法规则建什么结点，用户可编辑",
    ], top=5.0, left=8.3, width=4.3, size=13, gap=0.62)
    notes(s, "分析表是标准的 ACTION/GOTO。填表时有个原则：出现冲突必须列出来，不能悄悄选一个。"
             "mini-C 的文法本身有 1 处冲突，⑫ 页签下方会明确给出状态、符号和两条候选产生式。"
             "语法分析阶段读第一项任务产出的 .lex，TINY 样例 298 步分析完，建出 76 个结点的树。"
             "树的结构不是写死在代码里的，而是由「语义动作文件」一行一条规则定义，界面上可以编辑。")
    footer(s, idx)
    return s


def s11_tree(prs, idx):
    s = slide_new(prs)
    title_bar(s, "语法树是“画”出来的：能缩放、能拖动、能导出")
    image_fit(s, "tab14_syntaxtree.png", MARGIN, 1.45, 8.1, 4.85,
              "⑭ 语法树：左侧树状绘制（自动适应窗口），右侧语义动作编辑器")
    bullets(s, [
        "**适应窗口**：默认按视口算比例，缩小到能看全",
        "滚轮**以光标为锚点**缩放（2%~800%），按住左键拖动平移",
        "三种导出：缩进文本 / 括号表示 / DOT",
        "曾经「缩在左上角成一团」——改为**像素布局 + QGraphicsView**",
    ], top=1.6, left=9.3, width=3.3, size=14, gap=1.15)
    notes(s, "语法树这块我改过一版。最初是手绘控件，76 个结点被画在 69×14 像素的角落里，"
             "放大也看不清。原因有两个：布局返回的是逻辑单位却被当成像素用，「适应窗口」又用控件"
             "自身尺寸算比例，等于永远 95%。后来改成像素布局，量字体宽度算结点框，再把视图换成 "
             "QGraphicsView，就有了滚轮缩放和拖动。这也说明——界面问题也得靠断言守，现在有 G14b~d 三条。")
    footer(s, idx)
    return s


def s12_quads(prs, idx):
    s = slide_new(prs)
    title_bar(s, "选做：语法树 → 四元式（40 条 / 27 条）")
    image_fit(s, "tab15_quads.png", MARGIN, 1.5, 7.4, 4.6, "⑮ 中间代码：四元式表与统计")
    bullets(s, [
        "**归纳翻译**：先算子结点，再按标签生成代码",
        "赋值 / 二元运算 / 控制流 / 函数边界分别有模板",
        "空树**给告警不崩溃**，可导出制表符文本",
        "TINY 40 条（临时变量 6、标号 11）；mini-C 27 条",
    ], top=1.65, left=8.6, width=4.0, size=15, gap=1.05)
    notes(s, "选做部分我做的是四元式。做法是对语法树做归纳翻译：先递归翻译子结点，再按当前结点"
             "的标签生成代码，赋值、二元运算、if/while/for、函数边界各有模板，标号和临时变量自动编号。"
             "TINY 生成 40 条，mini-C 生成 27 条，都能导出成表格。")
    footer(s, idx)
    return s


def s13_demo_syntax(prs, idx):
    s = slide_new(prs)
    title_bar(s, "演示②：语法分析三步走", "⑫ 分析表 → ⑬ 分析过程 → ⑭ 语法树")
    image_fit(s, "tab12_parsetable.png", 0.9, 1.6, 3.7, 3.5, "⑫ 分析表")
    image_fit(s, "tab13_parseprocess.png", 4.82, 1.6, 3.7, 3.5, "⑬ 逐行分析过程")
    image_fit(s, "tab14_syntaxtree.png", 8.68, 1.6, 3.7, 3.5, "⑭ 语法树")
    textbox(s, MARGIN, 5.4, SW - MARGIN * 2, 0.9,
            "从文法到树只需三步：载入并化简 → 生成分析表 → 读 .lex 分析建树；"
            "mini-C 同样流程，得到 92 个结点的树。",
            size=15, color=DARK, align=PP_ALIGN.CENTER)
    notes(s, "【此处切到系统实机演示，约 60 秒】切到 mini-C，点「完整语法流水线」：先看 ⑫ 分析表，"
             "注意冲突明细里那 1 处；再看 ⑬ 逐行的状态栈、符号栈、输入和动作；最后 ⑭ 的树，"
             "mini-C 是 92 个结点。演示完回到 PPT。")
    footer(s, idx)
    return s


def s14_tests(prs, idx):
    s = slide_new(prs)
    title_bar(s, "敢改代码，是因为有断言", "两层自动化 + 与老师参考数据逐词比对")
    big_numbers(s, [("304", "算法断言"), ("346", "界面断言"), ("96 / 79", "单词逐词一致"),
                    ("16", "缺陷全部闭环")], top=1.65, num_size=38, label_size=13,
                per_row=4, row_gap=1.35)
    bullets(s, [
        "算法层 `--console-selftest`；界面层 `--selftest` 走真实按钮路径",
        "**性能**：子集构造 31 秒 → 约 1.5 秒；整套自测 192 秒 → 17~39 秒",
        "典型缺陷：两套 MinGW 导致 cc1plus 找不到、字符类改造、页签被搬走",
        "构建 0 error 0 warning；全仓 99 个文本文件编码检查 0 不合规",
    ], top=3.9, left=MARGIN, width=11.5, size=17, gap=0.78)
    notes(s, "最后是「怎么证明它是对的」。我把测试做成两层：算法层 304 条断言，界面层 346 条，"
             "都带编号、能一条命令重跑。数据上说两个：生成的 .lex 和老师给的参考输出逐词比对，"
             "96 和 79 个单词全一致；性能上子集构造从 31 秒优化到 1.5 秒。"
             "16 条记录在案的真实缺陷，每条都有断言守住，不会再犯。")
    footer(s, idx)
    return s


def s15_summary(prs, idx):
    s = slide_new(prs)
    title_bar(s, "亮点与不足")
    rect(s, MARGIN, 1.5, 5.6, 4.6, LGRAY)
    textbox(s, MARGIN + 0.35, 1.65, 5.0, 0.4, "我认为的三个亮点", size=18, bold=True, color=BLUE)
    bullets(s, [
        "**可无头验证**：算法与界面分离，回归一条命令",
        "**字符类字母表**：规模降一个数量级，语言等价可证",
        "**文档可复现**：表格数据、截图、代码段均由脚本生成",
    ], top=2.2, left=MARGIN + 0.35, width=4.9, size=15, gap=1.15)
    rect(s, 6.9, 1.5, 5.6, 4.6, LGRAY)
    textbox(s, 7.25, 1.65, 5.0, 0.4, "三点不足", size=18, bold=True, color=RED)
    bullets(s, [
        "mini-C 那 1 处冲突只**报告并默认消解**，没做严格模式",
        "中间代码只做了**四元式**，没做三元组/伪代码",
        "界面样式偏朴素，高 DPI 未专门优化",
    ], top=2.2, left=7.25, width=4.9, size=15, gap=1.15)
    notes(s, "总结一下。我认为三个亮点是可无头验证、字符类字母表、文档可复现；"
             "不足也照实说：mini-C 那处冲突我只做到报告加默认消解，没实现「拒绝生成并提示改文法」；"
             "选做只做了四元式；界面样式比较朴素。")
    footer(s, idx)
    return s


def s16_end(prs, idx):
    s = slide_new(prs)
    rect(s, 0, 0, SW, SH, BLUE)
    textbox(s, MARGIN, 2.1, SW - MARGIN * 2, 1.0, "谢谢，请老师批评指正", size=40, bold=True,
            color=WHITE, align=PP_ALIGN.CENTER)
    rect(s, SW / 2 - 1.1, 3.35, 2.2, 0.05, WHITE)
    textbox(s, MARGIN, 3.7, SW - MARGIN * 2, 1.6,
            "一键复现：tools\\build.ps1 → CompilerCourseDesign.exe → --selftest\n"
            "三份文档：实验报告书 / 系统使用说明书 / 测试报告（附录 A、B）",
            size=16, color=WHITE, align=PP_ALIGN.CENTER, spacing=1.6)
    notes(s, "我的讲解结束。系统可以一键构建、一键自测；三份文档在 docs 目录里，"
             "其中使用说明书和测试报告全文附在报告书的附录 A、B。谢谢老师。")
    footer(s, idx)
    return s


BUILDERS = [s01_cover, s02_agenda, s03_tasks, s04_arch, s05_class, s06_pipeline, s07_codegen,
            s08_demo_lex, s09_lalr, s10_parsetable, s11_tree, s12_quads, s13_demo_syntax,
            s14_tests, s15_summary, s16_end]

# 讲稿里的时长（与讲稿 md 一起输出，便于对着录）
TIMES = ["0:00-0:15", "0:15-0:45", "0:45-1:45", "1:45-2:55", "2:55-4:15", "4:15-5:25",
         "5:25-6:35", "6:35-8:15", "8:15-9:45", "9:45-10:50", "10:50-11:45", "11:45-12:35",
         "12:35-13:45", "13:45-15:05", "15:05-15:40", "15:40-16:00"]


def main():
    prs = Presentation()
    prs.slide_width = Inches(SW)
    prs.slide_height = Inches(SH)

    script_lines = ["# 讲解视频脚本（与 `讲解PPT.pptx` 逐页对应）", "",
                    "> 目标时长 15 分钟：口播约 13 分钟 + 两处实机演示约 2 分钟。"
                    "每页文字都对应 PPT 备注，可直接照读。", ""]

    for i, builder in enumerate(BUILDERS, 1):
        slide = builder(prs, i)
        note = slide.notes_slide.notes_text_frame.text
        script_lines.append(f"## 第 {i} 页 · {TIMES[i - 1]}")
        script_lines.append("")
        head, best = None, 0.0
        for shp in slide.shapes:
            if shp.has_text_frame and shp.text_frame.text.strip():
                size = 0.0
                for par in shp.text_frame.paragraphs:
                    for run in par.runs:
                        if run.font.size is not None:
                            size = max(size, run.font.size.pt)
                if size > best:            # 取字号最大的那段作为页面标题
                    best = size
                    head = shp.text_frame.text.strip().split("\n")[0]
        script_lines.append(f"**页面标题**：{head}")
        script_lines.append("")
        script_lines.append(note)
        script_lines.append("")

    prs.save(PPTX)
    with open(SCRIPT, "w", encoding="utf-8") as fh:
        fh.write("\n".join(script_lines))

    total_chars = sum(len(s.notes_slide.notes_text_frame.text) for s in prs.slides)
    print(f"已生成：{PPTX}（{len(prs.slides)} 页，16:9）")
    print(f"已生成：{SCRIPT}（备注合计 {total_chars} 字，约 {total_chars / 200:.1f} 分钟）")


if __name__ == "__main__":
    sys.exit(main())
