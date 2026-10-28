# -*- coding: utf-8 -*-
"""检查 `docs/讲解PPT.pptx` 的结构性问题（不需要打开 PowerPoint）。

检查项：
  1. 页数 = 16、尺寸 13.333×7.5 英寸（16:9）；
  2. 每页都有标题文字；
  3. 所有形状都在画布内（左/上不为负，右/下不越界）；
  4. 图片不越界、不超出内容区（左右边距 0.9 英寸）；
  5. 字体只有「微软雅黑 / Consolas」两种（避免中文回落宋体）；
  6. 每页都有备注讲稿，且总字数落在 2000~3000 之间（≈12~15 分钟）；
  7. 文本框的粗估溢出（按 CJK 字宽 = 字号估算），只报明显超标者。

精确的“文字是否溢出”由 `verify_ppt_com.ps1` 用 PowerPoint 实测，本脚本只做结构兜底。
"""

import math
import os
import sys

from pptx import Presentation
from pptx.util import Emu

HERE = os.path.dirname(os.path.abspath(__file__))
DOCS = os.path.dirname(HERE)
PATH = os.path.join(DOCS, "讲解PPT.pptx")

SW_IN, SH_IN = 13.333, 7.5
MARGIN_IN = 0.9
TOL = 0.03
ALLOWED_FONTS = {"微软雅黑", "Consolas", None, ""}


def inch(v):
    return Emu(v).inches if v is not None else 0.0


def est_overflow(shape):
    """返回 (需要高度, 框高度) 的粗估，用于发现明显超框的文本框。"""
    tf = shape.text_frame
    box_w = inch(shape.width)
    needed = 0.0
    for p in tf.paragraphs:
        text = "".join(r.text for r in p.runs)
        if not text:
            needed += 0.12
            continue
        size = None
        for r in p.runs:
            if r.font.size is not None:
                size = r.font.size.pt
                break
        size = size or 18.0
        char_w_in = size / 72.0            # 中文全角 ≈ 字号；英文按 0.55 计
        latin = sum(1 for c in text if ord(c) < 128)
        cjk = len(text) - latin
        width_in = cjk * char_w_in + latin * char_w_in * 0.55
        lines = max(1, math.ceil(width_in / max(box_w, 0.3)))
        needed += lines * size * 1.28 / 72.0
    return needed, inch(shape.height)


def main():
    prs = Presentation(PATH)
    ok = True
    print(f"文件：{os.path.basename(PATH)}  大小 {os.path.getsize(PATH) // 1024} KB")
    print(f"页数：{len(prs.slides)}   尺寸：{inch(prs.slide_width):.3f} × {inch(prs.slide_height):.3f} in")
    if len(prs.slides) != 16:
        print("  [问题] 页数不是 16")
        ok = False
    if abs(inch(prs.slide_width) - SW_IN) > 0.01 or abs(inch(prs.slide_height) - SH_IN) > 0.01:
        print("  [问题] 页面尺寸不是 16:9")
        ok = False

    fonts = set()
    total_notes = 0
    for i, slide in enumerate(prs.slides, 1):
        has_title = False
        for shape in slide.shapes:
            left, top = inch(shape.left), inch(shape.top)
            w, h = inch(shape.width), inch(shape.height)
            # 越界
            if left < -TOL or top < -TOL or left + w > SW_IN + TOL or top + h > SH_IN + TOL:
                print(f"  [越界] 第 {i} 页 {shape.shape_type} "
                      f"({left:.2f},{top:.2f},{w:.2f}×{h:.2f})")
                ok = False
            if shape.shape_type == 13:  # PICTURE
                if left < MARGIN_IN - TOL or left + w > SW_IN - MARGIN_IN + TOL:
                    print(f"  [图片超内容区] 第 {i} 页 x={left:.2f} w={w:.2f}")
                    ok = False
            if shape.has_text_frame:
                txt = shape.text_frame.text.strip()
                if txt:
                    for p in shape.text_frame.paragraphs:
                        for r in p.runs:
                            if r.font.size is not None and r.font.size.pt >= 24:
                                has_title = True
                    for p in shape.text_frame.paragraphs:
                        for r in p.runs:
                            if r.font.name is not None:
                                fonts.add(r.font.name)
                    need, box = est_overflow(shape)
                    if need > box * 1.45 and need - box > 0.45:
                        print(f"  [疑似溢出] 第 {i} 页 {txt[:18]!r} 需要 {need:.2f} in / 框 {box:.2f} in")
                        ok = False
        if not has_title:
            print(f"  [缺标题] 第 {i} 页")
            ok = False
        note = slide.notes_slide.notes_text_frame.text.strip() if slide.has_notes_slide else ""
        if len(note) < 60:
            print(f"  [备注太短] 第 {i} 页：{len(note)} 字")
            ok = False
        total_notes += len(note)

    print(f"备注合计：{total_notes} 字 ≈ {total_notes / 190:.1f} 分钟（按 190 字/分钟）")
    if not 1800 <= total_notes <= 3200:
        print("  [问题] 备注字数不在 1800~3200 区间")
        ok = False
    bad = fonts - ALLOWED_FONTS
    if bad:
        print(f"  [字体] 出现预期外的字体：{sorted(bad)}")
        ok = False
    print("字体：", sorted(f for f in fonts if f))
    print("结构检查：", "通过" if ok else "存在问题")
    return 0 if ok else 1


if __name__ == "__main__":
    sys.exit(main())
