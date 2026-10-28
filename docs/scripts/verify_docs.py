# -*- coding: utf-8 -*-
"""检查三份文档的结构性问题：表格超宽、图片超宽、图/表编号连续性、空表格等。

用法：python verify_docs.py
"""

import glob
import os
import re
import sys

from docx import Document
from docx.shared import Cm

HERE = os.path.dirname(os.path.abspath(__file__))
DOCS = os.path.dirname(HERE)
TEXT_CM = 21.0 - 3.17 * 2       # 14.66 cm
TOL = 0.15


def table_width_cm(tbl):
    widths = []
    for row in tbl.rows:
        for cell in row.cells:
            widths.append(cell.width.cm if cell.width is not None else None)
    widths = [w for w in widths if w]
    if not widths:
        return None
    ncols = len(tbl.columns)
    if ncols == 0:
        return None
    return sum(widths[:ncols]) if len(widths) >= ncols else None


def main():
    ok = True
    for path in sorted(glob.glob(os.path.join(DOCS, "*.docx"))):
        doc = Document(path)
        name = os.path.basename(path)
        print("=" * 70)
        print(name)

        # ---- 表格宽度 ----
        for i, tbl in enumerate(doc.tables, 1):
            w = table_width_cm(tbl)
            if w is None:
                continue
            if w > TEXT_CM + TOL:
                print(f"  [超宽表格] 第 {i} 张：{w:.2f} cm > {TEXT_CM:.2f} cm")
                ok = False
            if len(tbl.rows) < 1:
                print(f"  [空表格] 第 {i} 张")
                ok = False

        # ---- 图片宽度 ----
        for i, shape in enumerate(doc.inline_shapes, 1):
            w = shape.width.cm
            if w > TEXT_CM + TOL:
                print(f"  [超宽图片] 第 {i} 张：{w:.2f} cm")
                ok = False

        # ---- 图/表编号连续性 ----
        texts = [p.text.strip() for p in doc.paragraphs]
        for kind in ("图", "表"):
            nums = []
            for t in texts:
                m = re.match(rf"^{kind} (\d+(?:-\d+)?)  ", t)
                if m:
                    nums.append(m.group(1))
            if nums:
                seq = []
                for n in nums:
                    if "-" in n:
                        ch, idx = n.split("-")
                        seq.append((int(ch), int(idx)))
                    else:
                        seq.append((0, int(n)))
                dup = {x for x in seq if seq.count(x) > 1}
                gaps = []
                for a, b in zip(seq, seq[1:]):
                    if a[0] == b[0] and b[1] != a[1] + 1:
                        gaps.append((a, b))
                print(f"  {kind}：{len(nums)} 个编号，"
                      + (f"重复 {sorted(dup)}" if dup else "无重复")
                      + ("，" + f"跳号 {gaps[:3]}" if gaps else "，连续"))
                if dup:
                    ok = False

        # ---- 标题层级 ----
        heads = [p for p in doc.paragraphs if p.style.name.startswith("Heading")]
        lvl = {}
        for h in heads:
            lvl[h.style.name] = lvl.get(h.style.name, 0) + 1
        print("  标题：", lvl)

        # ---- 异常字符 ----
        bad = sum(1 for p in doc.paragraphs if "\ufffd" in p.text)
        bad += sum(1 for t in doc.tables for r in t.rows for c in r.cells if "\ufffd" in c.text)
        if bad:
            print(f"  [乱码] {bad} 处")
            ok = False

        # ---- 正文首段是否为空 / 字体检查（抽样） ----
        east = set()
        for p in doc.paragraphs[:200]:
            for r in p.runs:
                if r.font.name:
                    east.add(r.font.name)
        print("  字体（抽样）：", sorted(east)[:6])

    print("=" * 70)
    print("检查完成：", "全部通过" if ok else "存在问题")
    return 0 if ok else 1


if __name__ == "__main__":
    sys.exit(main())
