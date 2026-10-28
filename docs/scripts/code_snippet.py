# -*- coding: utf-8 -*-
"""从真实源文件中按函数名截取代码段，供实验报告书的“四、关键源程序代码段”使用。

这样做有两个好处：
  * 报告里的代码**必然与仓库当前代码一致**，不会出现"报告写一套、代码改一套"；
  * 生成代码段的长度可控（max_lines），满足版头格式"源码行数不超过其他内容行数 1/4"。
"""

import os
import re

HERE = os.path.dirname(os.path.abspath(__file__))
DOCS = os.path.dirname(HERE)
REPO = os.path.dirname(DOCS)


def _read(rel_path):
    with open(os.path.join(REPO, rel_path), "r", encoding="utf-8") as fh:
        return fh.read().split("\n")


def snippet(rel_path, signature, max_lines=16, note="……（以下略）"):
    """截取 `signature` 所在函数的开头若干行（到配对的花括号为止）。

    signature 只需是该函数定义行的一段唯一子串，例如 "bool NfaToDfa::construct"。
    """
    lines = _read(rel_path)
    start = None
    for i, line in enumerate(lines):
        if signature in line and "(" in line and not line.lstrip().startswith("//"):
            start = i
            break
    if start is None:
        raise RuntimeError(f"{rel_path} 中找不到函数：{signature}")

    # 从签名行的左花括号开始配对花括号
    depth = 0
    opened = False
    end = start
    for i in range(start, len(lines)):
        code = lines[i].split("//")[0]
        for ch in code:
            if ch == "{":
                depth += 1
                opened = True
            elif ch == "}":
                depth -= 1
        if opened and depth == 0:
            end = i
            break
    body = lines[start:end + 1]
    truncated = len(body) > max_lines
    body = body[:max_lines]
    if truncated:
        body = body + ["    " + note]
    # 去掉公共缩进
    indents = [len(x) - len(x.lstrip(" ")) for x in body if x.strip()]
    cut = min(indents) if indents else 0
    return "\n".join(x[cut:] for x in body)


def count_lines(code):
    return len([x for x in code.split("\n") if x.strip()])
