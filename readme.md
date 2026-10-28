# 编译原理课程设计 —— 词法分析器与 LALR(1) 语法分析器生成系统

本仓库是《编译原理》课程设计的完整提交内容：**源程序 + 可执行程序 + 测试文本与测试源程序 +
readme + 测试报告 + 系统使用说明书 + 课程设计实验报告书**。开发语言为 C++17，图形界面使用
Qt 6.10.2（Widgets），构建工具为 qmake + MinGW GCC 13.1.0。

---

## 1. 系统能做什么

| 任务 | 输入 | 输出 |
|---|---|---|
| 第一项：词法分析生成器 | 某一语言的**单词正则表达式**文本（`data/*_words.txt`） | 可编译运行的 C/C++ 词法分析程序 + 单词编码文件 `*.lex` |
| 第二项：LALR(1) 语法分析生成器 | 某一语言的 **BNF 文法**文本（`data/*_grammar.txt`）+ 第一项输出的 `.lex` | LALR(1) 分析表、逐步语法分析过程、语法树 |
| 选做：中间代码 | 语法树 | 四元式表（可导出） |

两项任务都已完成，并以 **TINY** 与 **mini-C** 两种语言分别测试通过；第一阶段用 TINY 的
词法与语法测试，第二阶段用 mini-C 做整体测试（遵循任务书“先 Tiny、后 Mini-C”的要求）。

---

## 2. 目录结构

```
├─ CompilerCourseDesign.pro    qmake 工程文件（83 个源文件，UTF-8 无 BOM）
├─ src/                        源程序
│  ├─ common/                  公共层（不依赖界面）：数据结构、文件/路径、日志、语言配置、自测框架
│  ├─ lex/                     第一项任务：单词规格解析 → 正则解析 → NFA → DFA → 最小化 → 代码生成
│  ├─ syntax/                  第二项任务：文法载入/化简 → First/Follow → LR(0) → LALR(1) → 分析表
│  │                           → 语法分析 → 语法树 → 四元式
│  ├─ gui/                     图形界面层：主窗口 + 15 个功能页签 + 复用控件
│  └─ main.cpp                 程序入口（图形界面 / 自测 / 截图三种模式）
├─ data/                       随程序分发的输入数据（构建时复制到可执行文件同目录）
│  ├─ tiny_words.txt           TINY 所有单词的正则表达式
│  ├─ tiny_grammar.txt         TINY 所有语法（BNF）
│  ├─ tiny_semantics.txt       TINY 的语法树结构（语义动作）
│  ├─ sample.tny               TINY 测试源程序
│  ├─ minic_words.txt / minic_grammar.txt / minic_semantics.txt / sample.mc   同上（mini-C）
│  └─ minic_grammar_taskbook.txt  mini-C 任务书原文文法（含缺陷，供报告对比；程序不读它）
├─ tools/                      三个脚本（PowerShell，UTF-8 **带 BOM**）
│  ├─ build.ps1                一键构建（qmake + mingw32-make，并复制 data/）
│  ├─ deploy.ps1               部署 Qt 运行库（windeployqt），使 release/ 可直接运行
│  └─ check_encoding.ps1       全仓库编码与 BOM 规范检查
├─ docs/                       交付文档
│  ├─ 课程设计实验报告书.docx   六节结构 + 附录 A（使用说明书）/ 附录 B（测试报告）
│  ├─ 系统使用说明书.docx       运行环境、界面总览、逐步操作、常见问题、命令行
│  ├─ 测试报告.docx             测试策略、用例与结果、缺陷记录、结论
│  ├─ images/                   文档插图（界面截图 15 张、结构与流程图 17 张）
│  ├─ figures/                  插图的 Graphviz DOT 源码（可重新渲染）
│  └─ scripts/                  生成上述三份 docx 的 Python 脚本（可复现）
├─ reference/                  调研资料（**只作数据/格式基准，绝不参与构建**，见第 6 节）
└─ build/                      构建输出与可运行产物（可整体删除后重建）
```

---

## 3. 构建与运行

```powershell
# 1) 构建（约 90 秒）
powershell -ExecutionPolicy Bypass -File tools\build.ps1

# 2) 首次/换机后部署 Qt 运行库（只需一次）
powershell -ExecutionPolicy Bypass -File tools\deploy.ps1

# 3) 运行
& .\build\CompilerCourseDesign\release\CompilerCourseDesign.exe
```

工具链固定为：Qt `D:\Qt6.10\6.10.2\mingw_64` + Qt 自带 MinGW `D:\Qt6.10\Tools\mingw1310_64`
（g++ 13.1.0）。**不要**混用其它 MinGW（如 g++ 15.2.0），否则 ABI 不匹配。

除图形界面外，程序还支持三种命令行模式（便于验收与生成文档截图）：

```powershell
CompilerCourseDesign.exe --console-selftest          # 仅算法自测（304 项断言）
CompilerCourseDesign.exe --selftest                  # 算法自测 + 界面自测（另有 346 项界面断言）
CompilerCourseDesign.exe --shot 图.png --tab 13 --demo-full   # 把第 14 个页签渲染为 PNG
```

> 界面截图需要 `-platform offscreen` 或直接在无显示器环境下运行；`--demo-full` 会先把两条
> 流水线跑一遍，使 15 个页签都带真实数据。

### 运行期产物

程序运行产物写在可执行文件目录的 `output/` 下，可随时删除：

```
output/lexbuild/   <语言>_lexer.cpp / .exe / *.lex（生成的词法程序与单词编码）
output/dot/        自动机与语法树的 DOT 文件
output/*.log       日志
```

---

## 4. 数据文件格式（用户可自行修改）

1. **单词规格文件**（`*_words.txt`）：一行一条定义，`#` 后为注释。三种写法：
   - 宏定义：`digit=[0-9]`、`letter=[A-Za-z]`（只做替换，不建自动机）；
   - 需要建自动机的规则：`Num_100=(\+|-)?digit+`，下划线后的数字即单词编码；
     编码后加 `L` 表示连续分配（如 `Symbol_200L=…`）；
   - 关键字表：`Keyword103L=if|then|else|…`（不建自动机，由生成的程序查表改判）。
2. **文法文件**（`*_grammar.txt`）：`左部 -> 右部1 | 右部2`，符号以空白分隔，`#`（mini-C 用
   `@`）表示空串；**第一个非终结符即开始符号**。
3. **语义动作文件**（`*_semantics.txt`）：一行对应一条产生式，行序与文法文件一致；`;` 分隔
   若干“建结点组”，`L c1 c2…` 表示建一个以左部符号为标签的结点，`R k c1…` 表示标签取右部
   第 k 项，`-1 0` 表示不建结点、直接传递。
4. **单词编码文件**（`.lex`）：每行 `<编码> <值>`；`ID` 的值带双引号，`NUM` 保留原文，关键字与
   符号只写编码；注释不输出；末行 `EOF`。

---

## 5. 文档

| 文档 | 文件 | 说明 |
|---|---|---|
| 课程设计实验报告书 | `docs/课程设计实验报告书.docx` | 按任务书“版头及正文格式”的六节书写；第三节按“五段式”逐功能说明；附录 A/B 为使用说明书与测试报告全文 |
| 系统使用说明书 | `docs/系统使用说明书.docx` | 面向使用者：环境、启动、界面、逐步操作、数据格式、常见问题、命令行 |
| 测试报告 | `docs/测试报告.docx` | 面向验收：测试策略、用例与结果、关键记录、缺陷分析、结论 |

三份文档由 `docs/scripts/` 下的 Python 脚本生成（`make_report.py`、`make_manual.py`、
`make_test_report.py`），插图由 `make_figures.py`（Graphviz）与程序自带的 `--shot` 生成，
因此**文档中的状态数、行数、截图都能重新复现**。

---

## 6. 关于 `reference/` 目录（重要申明）

`reference/` 是开发前的**调研资料**，用途严格限定为“**数据与格式基准**”：

- `reference/01..15_*`：同课程历史项目随机附带的单词、文法、语义动作、样例源程序与输出对照
  数据。其中 `03_tiny_output.lex`、`08_tiny_tree.out`、`11_minic_output.lex` 被自测程序按
  `../reference/` 路径读取，用于**逐词比对**（证明本项目的单词编码与参考实现一致）。
- `reference/O_*`、`T_*`：另外两个历史项目的源码与 README，**仅存档、不参与构建、未复制任何
  代码进 `src/`**。本项目所有源程序 100% 自行编写。
- `reference/taskbook/`：老师发布的《任务书》《讲稿》《实验报告书版头及正文格式》《书写格式
  模板范例》《算法流程图的画法》以及 TINY / mini-C 两组测试数据（原始文件，未做修改）。

**本项目的代码不复制 `reference/` 中任何 `.cpp/.h`**；引用到的只有“数据格式”与“输出基准”，
具体用在哪些断言里，已在 `AGENT.md` 第 12 节与《测试报告》附录 B 中逐条说明。

---

## 7. 开发与维护提示

- 每个源文件带四要素文件头（版权、文件名/标识符/摘要、版本与作者、版本历史）；每个函数前有
  功能说明、输入输出与注意事项；宏与非局部变量均有注释（任务书 p.5 的硬性要求）。
- 修改核心算法后必须重跑：`--console-selftest`（304 项）、`--selftest`（346 项）、
  `tools/check_encoding.ps1`（编码规范）与 `tools/build.ps1`（要求 0 error 0 warning）。
- 已知陷阱（两套 MinGW 混用导致 `cc1plus` 找不到、中文路径与 DOT 转义、Qt 页签复用控件等）
  全部记录在 `AGENT.md` 第 13 节，改代码前建议先看一遍。
