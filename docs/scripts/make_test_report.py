# -*- coding: utf-8 -*-
"""生成《测试报告》（独立文件）。

用法：python make_test_report.py
输出：docs/测试报告.docx
"""

import os
import sys

import content_test
from docs_style import Num, cover, new_document, page_break, toc

HERE = os.path.dirname(os.path.abspath(__file__))
DOCS = os.path.dirname(HERE)
OUT = os.path.join(DOCS, "测试报告.docx")


def build():
    doc = new_document()
    cover(
        doc,
        kind="测试报告",
        subtitle="词法分析器与 LALR(1) 语法分析器生成系统",
        info_rows=[
            ("被测软件", "编译原理课程设计系统 1.0.0"),
            ("测试语言", "TINY、mini-C"),
            ("测试方式", "自动化断言（算法层 304 项 + 界面层 346 项）+ 数据比对"),
            ("测试结论", "全部通过，满足任务书要求"),
            ("编写日期", "2026 年 12 月"),
        ],
        date_text="华南师范大学计算机学院",
    )
    page_break(doc)
    toc(doc)
    page_break(doc)

    c = content_test.Ctx(doc, off=0, nums=None)
    content_test.build(c, [Num("")] * 6)

    doc.save(OUT)
    print("已生成：", OUT)


if __name__ == "__main__":
    build()
    sys.exit(0)
