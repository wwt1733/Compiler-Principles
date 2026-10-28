# -*- coding: utf-8 -*-
"""生成《课程设计实验报告书》。

结构严格按 reference/taskbook/编译原理课程设计实验报告书 版头及正文格式.doc 的六节：
一、课程设计内容　二、课程设计目的　三、课程设计文档
四、实验总结（心得体会）　五、参考文献　六、课程设计自评
并按任务书“将按软件工程规范书写的文档附加在这里”的要求，把
《系统使用说明书》与《测试报告》作为附录 A、附录 B 全文附在报告之后。

用法：python make_report.py
输出：docs/课程设计实验报告书.docx
"""

import os
import sys

import content_manual
import content_report_a as A
import content_report_b as B
import content_report_c as C
import content_test
from docs_style import Num, cover, h1, new_document, page_break, toc

HERE = os.path.dirname(os.path.abspath(__file__))
DOCS = os.path.dirname(HERE)
OUT = os.path.join(DOCS, "课程设计实验报告书.docx")


def build():
    doc = new_document()

    # ---------------- 封面 ----------------
    cover(
        doc,
        kind="课程设计实验报告书",
        subtitle="题目：词法分析器与 LALR(1) 语法分析器生成系统的设计与实现",
        info_rows=[
            ("学　　院", "计算机学院"),
            ("专　　业", "计算机科学与技术"),
            ("班　　级", "【请填写】"),
            ("学　　号", "【请填写】"),
            ("姓　　名", "【请填写】"),
            ("指导教师", "【请填写】"),
        ],
        date_text="2026 年 12 月",
    )

    page_break(doc)
    toc(doc)
    page_break(doc)

    # ---------------- 正文六节 ----------------
    num3 = Num("3")
    A.section1(doc)
    A.section2(doc)
    A.section3_intro(doc, num3)
    A.req_table(doc, num3)
    A.input_format(doc, num3)
    A.output_contract(doc, num3)
    A.ui_requirements(doc, num3)
    A.overview(doc, num3)
    A.data_structures(doc, num3)
    B.section34(doc, num3)
    C.section35(doc, num3)
    C.section4(doc)
    C.section5(doc)
    C.section6(doc, num3)

    # ---------------- 附录 A：系统使用说明书 ----------------
    page_break(doc)
    h1(doc, "附录 A  系统使用说明书")
    c1 = content_manual.Ctx(doc, off=1, nums=None)
    content_manual.build(c1, [Num("A")] * 5)

    # ---------------- 附录 B：测试报告 ----------------
    page_break(doc)
    h1(doc, "附录 B  测试报告")
    c2 = content_test.Ctx(doc, off=1, nums=None)
    content_test.build(c2, [Num("B")] * 6)

    doc.save(OUT)
    print("已生成：", OUT)


if __name__ == "__main__":
    build()
    sys.exit(0)
