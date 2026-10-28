# -*- coding: utf-8 -*-
"""生成《系统使用说明书》（独立文件）。

用法：python make_manual.py
输出：docs/系统使用说明书.docx
"""

import os
import sys

import content_manual
from docs_style import Num, cover, new_document, page_break, toc

HERE = os.path.dirname(os.path.abspath(__file__))
DOCS = os.path.dirname(HERE)
OUT = os.path.join(DOCS, "系统使用说明书.docx")


def build():
    doc = new_document()
    cover(
        doc,
        kind="系统使用说明书",
        subtitle="词法分析器与 LALR(1) 语法分析器生成系统",
        info_rows=[
            ("软件名称", "编译原理课程设计系统"),
            ("版本号", "1.0.0"),
            ("适用语言", "TINY、mini-C"),
            ("开发环境", "C++17 + Qt 6.10.2 (MinGW 13.1.0)"),
            ("编写日期", "2026 年 12 月"),
        ],
        date_text="华南师范大学计算机学院",
    )
    page_break(doc)
    toc(doc)
    page_break(doc)

    c = content_manual.Ctx(doc, off=0, nums=None)
    content_manual.build(c, [Num("")] * 5)

    doc.save(OUT)
    print("已生成：", OUT)


if __name__ == "__main__":
    build()
    sys.exit(0)
