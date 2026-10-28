# AGENT.md — 开发约定（编译原理课程设计系统）

本文件是本仓库的**长期开发约定**，任何一次会话（人或 AI）在动手改代码前必须读本文件。
任务范围与路线见 [plan.md](plan.md)。

---

## 1. 项目标识

| 项 | 值 |
|---|---|
| 项目名称 | 编译原理课程设计系统（正则→NFA→DFA→最小化DFA→词法分析程序；BNF→LALR(1)→语法树） |
| 主程序目标名 | `CompilerCourseDesign` |
| 生成物目标名 | `generated_lexer`（由本系统生成的词法分析源程序编译成的 exe） |
| 开发语言 | 仅 **C++17**（Qt Widgets）。生成的目标源码也是 C++（不得依赖 Qt） |
| GUI 框架 | **Qt 6.10.2 mingw_64**，构建系统 **qmake**（`.pro`） |
| 源码根 | 仓库根目录（即本文件所在目录） |
| 构建输出 | `build/`（不进版本管理，可随时删除重建） |

---

## 2. 构建环境（已实测通过，勿随意更换）

工具链必须三者配套，混用其它 g++ 会因 ABI/DLL 不一致失败：

```
Qt        : D:\Qt6.10\6.10.2\mingw_64          （Qt 6.10.2, MinGW 64-bit）
MinGW GCC : D:\Qt6.10\Tools\mingw1310_64\bin   （g++ 13.1.0）
CMake     : D:\Qt6.10\Tools\CMake_64\bin       （3.30.5，备用，默认不用）
```

- **不要**使用 `D:\mingw\mingw64`（g++ 15.2.0，Brecht Sanders r5 构建，与 Qt 的 MinGW 13.1.0 不配套）。
- **不要**使用 PATH 上的 `cmake 4.2.1`（版本过高，可能拒绝旧 `cmake_minimum_required`）。
- 环境变量 `PATH` 必须先放 `D:\Qt6.10\Tools\mingw1310_64\bin`，再放 `D:\Qt6.10\6.10.2\mingw_64\bin`。

### 2.1 构建命令（固定用法）

```powershell
# 一键构建（推荐）
& .\tools\build.ps1

# 等价的手工步骤
$env:Path = "D:\Qt6.10\Tools\mingw1310_64\bin;D:\Qt6.10\6.10.2\mingw_64\bin;" + $env:Path
New-Item -ItemType Directory -Force build\CompilerCourseDesign | Out-Null
Push-Location build\CompilerCourseDesign
qmake ..\..\CompilerCourseDesign.pro
mingw32-make -j8
Pop-Location
```

构建产物路径固定为：`build/CompilerCourseDesign/release/CompilerCourseDesign.exe`。
（`CONFIG += release` 时 qmake 在 Windows/MinGW 下会输出到 `release/` 子目录，脚本需兼容 `release/` 与直接输出两种情况。）

### 2.2 运行与部署

```powershell
# 拷贝 Qt 运行库到 exe 同目录（首次或换机后执行一次即可）
& .\tools\deploy.ps1
# 运行
& .\build\CompilerCourseDesign\release\CompilerCourseDesign.exe
```

编译**生成的**词法分析程序同样使用上面的 MinGW：
`D:\Qt6.10\Tools\mingw1310_64\bin\g++.exe -std=c++17 -O2 -o generated_lexer.exe generated_lexer.cpp`

### 2.3 验证要求（每次改动后）

1. `tools/build.ps1` 必须 **0 error 0 warning**（`-Wall -Wextra` 已开启）。
2. GUI 必须能启动，且不依赖手工设置的 PATH（`deploy.ps1` 已拷 DLL）。
3. 每次改动后按 plan.md 第 9 节的"阶段验收"自测对应功能。
4. **无头环境注意**：若无法启动 GUI 交互，可用 `-platform offscreen` 参数做冒烟启动测试：
   `CompilerCourseDesign.exe -platform offscreen --selftest`（`--selftest` 见 4.4）。

---

## 3. 目录结构约定

> 本节与磁盘**逐项对应**（2026-XX-XX 全项目审计后重写）。改结构必须同步改本节，
> 避免出现"文档里列着不存在的文件"这类误导。

```
<仓库根>/
├─ AGENT.md                     # 本文件：开发约定
├─ plan.md                      # 任务分析与实施计划（含进度记录）
├─ CompilerCourseDesign.pro     # qmake 工程（主程序；源文件清单必须与 src/ 一致）
├─ readme.md                    # 交付说明（目录结构、构建运行、数据格式、reference 来源申明）
├─ src/                         # 83 个源文件，只依赖 Qt（core/gui/widgets）
│  ├─ main.cpp                  # 入口：GUI / --selftest / --console-selftest / --shot
│  ├─ common/                   # 与界面无关的通用设施（只依赖 QtCore）
│  │  ├─ Types.h/.cpp           # 公共数据结构（Transition/Nfa/Dfa/CharClassAlphabet/Item/Token…）
│  │  ├─ Logger.h/.cpp          # 统一日志（界面日志窗 + 可选落盘，落盘按"每次运行截断"）
│  │  ├─ FileUtil.h/.cpp        # 文本读写、路径解析、DOT 转义（escapeForDot）
│  │  ├─ LanguageConfig.h/.cpp  # TINY / mini-C 配置中心 + 编码↔终结符名双向表
│  │  └─ SelfTest.h/.cpp        # 无界面自测套件（T1~T24 全部断言都在这里）
│  ├─ lex/                      # 第一项任务：正则 → NFA → DFA → 最小化 → 生成词法程序
│  │  ├─ RegexAst.h/.cpp        # 正则式语法树 + 解析器（转义、[ ]、~、?、+、宏、内置 eoln）
│  │  ├─ CharClass.h/.cpp       # 字符集原子收集 + 等价类划分 → 字符类字母表
│  │  ├─ TokenSpec.h/.cpp       # 单词规格文件解析（名字_编码[L]、digit=、注释/关键字分类）
│  │  ├─ RegexToNfa.h/.cpp      # Thompson 构造（双字母表模式：字符类 / 按字节）
│  │  ├─ NfaToDfa.h/.cpp        # 子集构造法 + ε 闭包
│  │  ├─ DfaMinimize.h/.cpp     # 划分法最小化
│  │  ├─ LexCodeGen.h/.cpp      # 最小化 DFA → C++ 词法分析源程序（方法一/方法二）
│  │  ├─ LexBuild.h/.cpp        # 落盘 + g++ 编译 + 运行 + 读回 .lex
│  │  └─ LexPipeline.h/.cpp     # 串起上述全部步骤（界面与自测共用的唯一入口）
│  ├─ syntax/                   # 第二项任务：BNF → 化简 → First/Follow → LALR(1) → 分析
│  │  ├─ Grammar.h/.cpp         # 文法数据结构 + 文件读写 + 增广文法
│  │  ├─ GrammarSimplify.h/.cpp # 不可达规则、无法推导出句子的规则删除
│  │  ├─ FirstFollow.h/.cpp     # First / Follow 集合
│  │  ├─ LrItems.h/.cpp         # LR(0) 项目集规范族与 DFA、闭包/GOTO
│  │  ├─ Lr1Items.h/.cpp        # LR(1) 项目集 + 同心集合并 → LALR(1)
│  │  ├─ ParseTable.h/.cpp      # ACTION/GOTO 表构造与冲突报告
│  │  ├─ LexReader.h/.cpp       # 读 .lex，把编码翻译成终结符名
│  │  ├─ Parser.h/.cpp          # 表驱动语法分析 + 语义动作建树
│  │  ├─ SemanticsAction.h/.cpp # 语法树结构定义（语义动作）的解析与判读
│  │  ├─ SyntaxTree.h/.cpp      # 树的文本/括号/DOT 输出、像素布局、与参考树比对
│  │  └─ IntermediateCode.h/.cpp# 四元式中间代码生成（选做项）
│  └─ gui/
│     ├─ MainWindow.h/.cpp      # 主窗口：菜单/工具栏/左侧功能树/中央 15 个页签
│     ├─ panels/                # 14 个功能面板（①~⑮；⑩⑪ 各用一份 LrItemsPanel 实例）
│     │  ├─ WordsEditorPanel    # ① 单词规格编辑器
│     │  ├─ NfaPanel            # ② NFA
│     │  ├─ DfaPanel            # ③④ DFA / 最小化 DFA（同一类，两个实例）
│     │  ├─ LexCodePanel        # ⑤ 生成的词法分析源程序
│     │  ├─ LexTestPanel        # ⑥ 编译与词法测试
│     │  ├─ GrammarEditorPanel  # ⑦ 文法编辑器
│     │  ├─ GrammarSimplifyPanel# ⑧ 文法化简结果
│     │  ├─ FirstFollowPanel    # ⑨ First/Follow
│     │  ├─ LrItemsPanel        # ⑩ LR(0) 项目集 / ⑪ LALR(1) 项目集（setModeIndex 切换）
│     │  ├─ ParseTablePanel     # ⑫ LALR(1) 分析表
│     │  ├─ SyntaxProcessPanel  # ⑬ 语法分析过程
│     │  ├─ SyntaxTreePanel     # ⑭ 语法树（含 SyntaxTreeView）
│     │  ├─ SyntaxTreeView      # ⑭ 的图形视图（QGraphicsView：适应窗口/滚轮缩放/拖动）
│     │  └─ IntermediateCodePanel# ⑮ 中间代码
│     └─ widgets/               # 复用控件：StateTableView（NFA/DFA 状态转换表 + DOT/PNG 导出）
├─ data/                        # 随程序分发的输入数据（构建时拷到 exe 同目录）
│  ├─ tiny_words.txt            # TINY 所有单词的正则表达式
│  ├─ tiny_grammar.txt          # TINY 所有语法（BNF）
│  ├─ tiny_semantics.txt        # TINY 语法树结构定义（语义动作，格式见 §10）
│  ├─ sample.tny                # TINY 测试源程序（覆盖全部单词与语句）
│  ├─ minic_words.txt           # mini-C 所有单词的正则表达式
│  ├─ minic_grammar.txt         # mini-C 所有语法（BNF，含 do-while）
│  ├─ minic_grammar_taskbook.txt# mini-C 任务书原文文法（含缺陷，供报告对比；代码不读它）
│  ├─ minic_semantics.txt       # mini-C 语法树结构定义
│  └─ sample.mc                 # mini-C 测试源程序
├─ tools/                       # 三个脚本（.ps1 必须 UTF-8 **带 BOM**，见 §13.3）
│  ├─ build.ps1                 # qmake + mingw32-make 一键构建，并拷贝 data/
│  ├─ deploy.ps1                # windeployqt 部署 Qt 运行库（生成可直接运行的 exe 目录）
│  └─ check_encoding.ps1        # 全仓编码规范检查（跳过 build/ 与 reference/）
├─ reference/                   # 调研资料（只作数据/格式基准，**绝不复制代码**，见 §12.4）
│  ├─ 01..15_*                  # 同课程历史项目的单词/文法/语义动作/输出对照数据
│  │                            #   （03/08/11 三个文件被 SelfTest 按 ../reference/ 读取）
│  ├─ O_* / T_*                 # 另外两个历史项目的源码与 README（仅存档，不参与构建）
│  └─ taskbook/                 # 老师给的任务材料
│     ├─ 编译原理课程设计任务书.pdf、讲稿.pdf、报告格式模板等 5 份
│     ├─ TINY语言的测试/        # 全单词、全语法、测试要求（3 个 txt）
│     └─ mini-c语言的测试/      # 同上（3 个 txt）
├─ build/                       # 构建输出与可运行产物（**可整体删除后重建**）
│  └─ CompilerCourseDesign/
│     ├─ Makefile*、.qmake.stash
│     ├─ data/                  # 构建时从 data/ 拷贝的副本
│     └─ release/               # CompilerCourseDesign.exe + Qt 运行库 + *.o
│                               # 运行产物（output/、selftest_report.txt）也落在这里，可随时删
└─ docs/                        # 交付文档（三份 docx + 插图 + 生成脚本）
   ├─ 课程设计实验报告书.docx    # 六节结构；附录 A=使用说明书、附录 B=测试报告（全文）
   ├─ 系统使用说明书.docx
   ├─ 测试报告.docx
   ├─ images/                   # 插图：界面截图 15 张（--shot 生成）+ 结构/流程图 17 张（Graphviz）
   ├─ figures/*.dot             # 插图的 DOT 源码（可重新渲染）
   └─ scripts/                  # 生成三份 docx 的 Python 脚本 + 校验脚本（docs_style / content_* /
                                #   make_* / make_figures / code_snippet / verify_docs）
```

**命名约定**
- 类名 `PascalCase`；成员变量 `m_` 前缀；普通函数/变量 `camelCase`；常量 `kPascalCase` 或全大写宏。
- 头文件用 `#pragma once`，包含顺序：本模块 → Qt → 标准库。
- 每个 `.h` 顶部必须有文件头注释块（见 5）。

---

## 4. 功能模块契约（接口约定，改动需同步更新本节）

### 4.1 第一项任务数据流

```
tiny_words.txt ──TokenSpec::parse──► QVector<RegexDef>
                                     │  每个 RegexDef: name, code, isLaunch, kind, regexText
                                     ▼
              Regex::analyzeRegex  （展开命名、处理转义/[ ]/?/~ → 加显式连接符 '.'）
                                     │  每个字符集结点记下"原子名"（宏名 / [..] / 转义字符 / ~x）
                                     ▼
              CharClassBuilder     （收集字符集原子 → 按成员签名做**等价类划分** → 字符类字母表）
                                     ▼
              Regex::toPostfix     （逆波兰表达式）
                                     ▼
              RegexToNfa::build    （Thompson 构造，邻接表 + 栈，算法见讲稿/范例；
                                    一个字符集 = 一对状态 + |C| 条平行边）
                                     ▼
              NfaToDfa::subsetConstruct （ε-closure + move → Dtran；按字符类迭代）
                                     ▼
              DfaMinimize::minimize（划分法 → 最少状态 DFA；**保留接受状态对应的单词编码**）
                                     ▼
              LexCodeGen::generate（方法一：Judgechar/GetNext/GetToken ；方法二：switch(state)）
                                     ▼
              界面展示 + 落盘 generated_lexer.cpp
```

关键契约：
- `RegexDef.kind ∈ {Normal, Comment, Keyword, Definition}`：
  - `Definition`（`digit=`、`letter=` 之类）只做宏替换，不参与自动机。
  - `Keyword` 不参与自动机，但按名字顺序占用连续编码（`L` 时），供生成的词法程序用**查表**识别。
  - `Comment` 参与自动机，但识别后**不输出编码**。
  - `Normal` 参与自动机，识别后输出编码。
- 所有 `Normal`/`Comment` 的定义通过"或"合并为**一个** NFA，接受状态携带 `{defName, code}` 标签。
- 合并后必须做 **最长匹配（maximal munch）**：在生成的词法程序中，沿 DFA 走到不能再走，回退到**最后一个接受状态**；该状态若对应多个编码，按 `TokenSpec` 中**声明顺序**（即文件中的先后）取第一个。
- **字母表是"字符类"，不是"字节"**（AGENT.md §13.30）：
  - 正则式里书写的每一处字符集（宏引用 `letter`、方括号 `[A-Za-z]`、转义单字符 `\+`、
    取反项 `~ \}`）都称为一个**字符集原子**；
  - 两个字节若属于**完全相同的原子集合**，它们在自动机任何状态下的转移都相同，
    于是合并成一个**字符类**，作为一个输入符号；
  - 类的名字：有原子与它字节集**完全相等** → 用该原子名（宏名优先）；否则叫 `<other>`；
  - 因此 `Nfa::alphabet` / `Dfa::alphabet` 里放的是**类符号键**（`@0`、`@1`…），
    人类的可读名字在 `Nfa::alphabetInfo` / `Dfa::alphabetInfo` 里；
  - `alphabetInfo.isEmpty()` 表示"按字节字母表"的经典模式（`RegexToNfa::build(rules, out,
    AlphabetMode::Byte)`，`buildFromPattern()` 固定走这条路），供回归对照用。

### 4.1.1 第一项任务的模块契约速查

| 模块 | 输入 → 输出 | 备注 |
|---|---|---|
| `TokenSpec` | 规格文本 → `QVector<RegexDef>` + 宏表 | 宏定义会把**宏名**写到自己语法树根结点的 `atomName` 上（若宏体是单个字符集） |
| `RegexAst` / `RegexParser` | 正则式文本 → 语法树 | 每个 `Charset` 结点的 `atomName`/`atomFromMacro` 记录它的"写法"，是字符类命名的依据 |
| `CharClassBuilder` | 全部规则的语法树 → `CharClassAlphabet` | 等价类划分 + 命名 + 紧凑字节集文本；`~` 的取反全集仍是 256 个字节 |
| `RegexToNfa` | 规则表 → `Nfa` | `AlphabetMode::CharClass`（默认）/ `Byte`（对照）；字符集只发一个操作数 |
| `NfaToDfa` | `Nfa` → `Dfa` | 必须把 `alphabetInfo` 一起拷到输出（否则界面/生成代码退化成按字节） |
| `DfaMinimize` | `Dfa` → `Dfa` | 同上，`alphabetInfo` 要带过去 |
| `LexCodeGen` | `Dfa` → C++ 源程序 | `charIndex()` 按字符类生成**区间判定**；`alphabetInfo` 为空时退回逐字节比较链 |
| `StateTableView`（界面控件） | `Nfa`/`Dfa` → 状态转换表 + DOT | 表格里 ε 是独立一列；**DOT 从显式边列表渲染**（含 ε 边，虚线 + `ε`），不能从"一符号一目的"的转移表凑（§13.34） |

### 4.2 第二项任务数据流

```
tiny_grammar.txt ──Grammar::load──► Grammar（产生式集 + 非终结符集；首非终结符=开始符号）
                                     ▼
              GrammarSimplify::run （1.删不可达  2.删不能推导出终结符串的规则；迭代到不动点）
                                     ▼
              FirstFollow::compute （First / Follow 集合）
                                     ▼
              LrItems::buildLr0     （I0=closure({S'→·S})，GOTO → LR(0) 项目集族 + DFA）
                                     ▼
              Lr1Items::buildLalr   （LR(1) 项目 → 同心集合并 → LALR(1) 项目集族）
                                     ▼
              ParseTable::build     （ACTION/GOTO；冲突须在界面明确报告）
                                     ▼
              Parser::parse(lexFile)（逐行压栈/规约，输出分析过程表）
                                     ▼
              SyntaxTree::build     （按 tiny_semantics.txt 的语义动作建树）──► 树状绘制
                                     ▼（选做）
              SyntaxTree/ICGen::quadruples（四元式中间代码）
```

关键契约：
- 文法文件格式：`左部 -> 右部1 | 右部2`，符号以空白分隔；`#` 表示空串（任务书 p.3）；
  **mini-C 的文法输入里空串用 `@`**（任务书 p.8），解析时 `#` 与 `@` 均视为 ε。
- 第一个非终结符即开始符号；载入后自动增广 `S' -> S`（若用户未写增广产生式）。
- 分析表冲突：若同一格子出现 shift/reduce 或 reduce/reduce 冲突，必须在界面列出冲突项（状态、符号、两条产生式），不得静默取一个。

### 4.3 单词编码文件（`.lex`）格式契约 —— 跨模块/跨任务的关键接口

由生成的词法程序输出，也是第二项任务语法分析的输入。**格式一旦确定不得随意变更**：

```
<code> <value>
```

- `code`：十进制单词编码（见第 6 节编码表），**这是任务书 p.3 示例所采用的格式**。
- `value`：
  - `ID` → `"标识符原文"`（**带双引号**）；
  - `NUM` → 数值字面量的原文（保留前导零，如 `001`）；
  - 关键字与专用符号 → **不写值**，该行只有编码一个字段。
- 多个单词可以**写在同一行**，用空格分隔（与任务书 p.3 的示例一致）；
  生成的词法程序默认**每行一个单词**（便于第二项任务逐行分析），界面另提供
  "任务书示例排版"模式把全部单词拼成一行输出，供报告截图对照。
- 注释不产生任何输出。
- 文件末尾输出一行 `EOF`（无编码），便于语法分析判断输入结束。

示例（对应任务书 p.3 的 mini-C 源程序 `int a=b-1;`）：

```
单行模式（默认，逐行一个单词）：        任务书示例模式（拼成一行）：
105                                     105 101 "a" 214 101 "b" 202 100 5 215
101 "a"
214
101 "b"
202
100 5
215
EOF
```

> **与参考实现的对照**：同课程已提交项目的 `output.lex` 采用 `NAME:value` 命名格式
> （如 `ID:xXx`、`SEMI:;`、`NUMBER:1000`、末尾 `EOF:EOF`）。其**语义与本格式完全等价**，
> 仅"编码"一项用名字而非数字表示。本项目按任务书字面要求采用**数字编码**；
> `reference/` 中保留了参考项目的 `output.lex`，用于人工对照核对编码分配是否合理
> （如 `SEMI` ↔ 215、`ID` ↔ 101、`NUMBER` ↔ 100）。


### 4.4 命令行自测开关（供无头验证用）

```
CompilerCourseDesign.exe --selftest [--case <name>]
```
`--selftest` 在 `-platform offscreen` 下顺序跑完 plan.md 第 9 节的全部验收用例，
把结果写入 `build/selftest_report.txt`，退出码 0 表示全部通过。任何核心算法改动后必须保证该开关通过。

---

## 5. 源码注释规范（任务书 p.5 硬性要求）

**每个源文件**开头必须有如下文件头（中英文皆可，统一用中文）：

```cpp
/******************************************************************************
 * (1) 版权信息 : 编译原理课程设计 - 华南师范大学计算机学院
 * (2) 文件名称 : NfaToDfa.cpp
 *     标识符   : 子集构造法
 *     摘要     : 将 NFA 状态转换表转换为 DFA 状态转换表
 * (3) 版本号   : 1.0.0       作者/修改者 : <学号 姓名>   完成日期 : 2026-XX-XX
 * (4) 版本历史 : 1.0.0  2026-XX-XX  首次实现
 *****************************************************************************/
```

- 所有**宏定义**、**非局部变量**必须加注释说明用途。
- 每个**函数前**必须有：功能说明、输入接口、输出接口、调用注意事项。
- 函数内**关键步骤**（循环、分支、算法核心）加行内注释。

---

## 6. 单词编码表（冻结，改动须全仓库同步）

> **字母表说明（§13.30 改造后）**：下面这些编码与"词法分析器输出什么"无关，
> 它们仍然是**编码**。变的只有自动机的输入符号粒度 —— 从"256 个字节"变成
> "约 20 个字符类"（`letter`、`digit`、`+` …）。字母表本身不写在这张表里，
> 它由 `lex/CharClass` 按规格文件现场算出（见 §4.1）。

### 6.1 TINY（关键字按**字面小写**比对，见注 5）

| 编码 | 单词 | 编码 | 单词 | 编码 | 单词 |
|---|---|---|---|---|---|
| 100 | NUM（`digit+`，无 `L` 后缀） | 200 | `+` | 208 | `<>` |
| 101 | ID | 201 | `-` | 209 | `>` |
| 102 | Comment（`{~}`，不输出） | 202 | `*` | 210 | `>=`（见注 1） |
| 103 | `if` | 203 | `/`（整除） | 211 | `=` |
| 104 | `then` | 204 | `%` | 212 | `{` |
| 105 | `else` | 205 | `^`（乘方，见注 2） | 213 | `}` |
| 106 | `end` | 206 | `<` | 214 | `;` |
| 107 | `repeat` | 207 | `<=` | 215 | `:=` |
| 108 | `until` | | | 216 | `(`（见注 3） |
| 109 | `read` | | | 217 | `)`（见注 3） |
| 110 | `write` | | | | |

> **注 1（补 `>=`）**：TINY 的单词表原文**没有列出 `>=`**，但其语法 BNF 的关系运算符
> （comparison-op）明确包含 `>=`。若不给编码，含 `>=` 的源程序会词法失败。
> **裁定：按 `Symbol200L` 的声明顺序把 `>=` 编为 210**，后续符号顺延
> （`=`=211、`{`=212、`}`=213、`;`=214、`:=`=215）。
> 该补充须在 `data/tiny_words.txt` 的注释与测试报告中说明。
>
> **注 2（补 `^`）**：TINY 单词表的专用符号里也没有 `^`，但语法 BNF 的 `powop -> ^`
> 用到了它，同理补入编码 205（在 `%` 之后、`<` 之前），后续符号顺延。
>
> **注 3（补括号）**：TINY 单词表同样没有列出圆括号，但 BNF 的 `factor` 含
> `( expression )`，任务书样例程序里也确实有 `(xXx + 1) * 1` 这样的写法。
> 补入 `(`=216、`)`=217（接在 `:=` 之后），使样例程序能够零词法错误地通过。
>
> **注 4（编码区间）**：因此 TINY 的专用符号实际占用 `200~217` 共 **18** 个编码，
> 参与自动机构造的定义共 **21** 条（Comment + Num + ID + 18 个符号）。
> `SelfTest` 的 T6.2 / T6.7 / T12.3 三处断言与此表逐项对齐，改表必须同步改这三处。
>
> **注 5（大小写口径）**：TINY 语言本身常被描述为"不区分大小写"，但本系统的
> 实现是**按字面精确比对**：`TokenSpec::keywordTable()` 的键就是规格文件里写的
> 原文，生成程序用 `strcmp` 查表，任何地方都没有 `tolower`。
> 因此 `if` 是关键字、`IF` 只是普通标识符（编码 101）。
> 样例程序全部用小写，所以这条差异一直没有暴露；`data/tiny_words.txt`
> 原先的"不区分大小写"注释已按实现更正，断言 `T23.8` 把"大小写敏感"固定下来。
> 若将来确实要支持不区分大小写，正确做法是在 `findKeyword()` 之前统一 `tolower`
> 输入与表项，**不是**改规格文件。
>
> **注 5b（已删除的 `caseSensitive` 字段）**：`LanguageConfig` 里曾经有一个
> `caseSensitive` 字段（TINY=false、mini-C=true），但**没有任何代码读取它**；
> 而当时还用它做了两条断言（"TINY 不区分大小写"/"mini-C 区分大小写"），
> 等于给"配置说一套、实现做另一套"背书。全项目审计时**字段与那两条断言一并删除**，
> 真实行为只由 `T23.8` 与本节注 5 表达。**不要再往配置里加"只是用来断言"的字段。**


### 6.2 mini-C（区分大小写；关键字必须小写）

| 编码 | 单词 | 编码 | 单词 | 编码 | 符号 |
|---|---|---|---|---|---|
| 100 | NUM（整数 `digit+`） | 103 | `else` | 200 | `_`（保留，见注 3） |
| 101 | ID | 104 | `if` | 201 | `+` |
| 102 | Comment（不输出） | 105 | `int` | 202 | `-` |
| 205 | NUM（小数 `digit+.digit+`，见注 1） | 106 | `float` | 203 | `*` |
| 226 | `%`（见注 1） | 107 | `double` | 204 | `/` |
| 227 | `!` 或 `~`（单独出现时，见注 2） | 108 | `return` | 206 | `^` |
| | | 109 | `void` | 207 | `.` |
| | | 110 | `do`（见注 5） | 208 | `<` |
| | | 111 | `while` | 209 | `<=` |
| | | 112 | `for` | 210 | `>` |
| | | | | 211 | `>=` |
| | | | | 212 | `==` |
| | | | | 213 | `!=` |
| | | | | 214 | `=` |
| | | | | 215 | `;` |
| | | | | 216 | `,` |
| | | | | 217 | `++` |
| | | | | 218 | `--` |
| | | | | 219 | `(` |
| | | | | 220 | `)` |
| | | | | 221 | `[` |
| | | | | 222 | `]` |
| | | | | 223 | `{` |
| | | | | 224 | `}` |
| | | | | 225 | `//`（行注解起始标记，等价 Comment） |

> **注 1（编码冲突裁定）**：任务书单词表把 `%` 列在专用符号里，编码区间为 `2xx`。
> 若按顺序把 `%` 编成 205，就与"小数 NUM=205"撞号。**最终裁定：205 = 小数 NUM，`%` = 226。**
> 单独出现的 `.` 占 207，与小数 NUM 不冲突（小数由同一个 DFA 接受态匹配整串，走最长匹配）。
> **注 2**：`!` 单独出现（不构成 `!=`）与 `~` 单独出现均不在任务书单词表内，但**不能按词法错误处理**
> （否则含 `!x` 的源程序无法通过），统一输出编码 227 并在第二项任务中作为语法错误上报。
> **注 3**：`_` 已由 ID 的正则 `(_|letter)(_|letter|digit)*` 覆盖；编码 200 **保留但永不输出**。
> **注 4（任务书笔误）**：任务书 p.7 与 `mini-c的所有单词.txt` 把 `else` 写成 `e1se`（数字 1），
> 属明显笔误，实现按 `else` 处理，并在测试报告中记录该修正。
> **注 5（任务书自相矛盾，以老师发布的单词表为准）**：
> 任务书 p.7 的关键字表为 `e1se if int float double return void while for`，**没有 `do`**；
> 但同一份任务书的产生式 13 引用了 `dowhile-stmt`，而 p.10 的产生式 16 只给出 `while-stmt`、
> **且全文没有任何产生式定义 `dowhile-stmt`**——即任务书自身不一致。
> 而老师随堂发布的 `mini-c的所有单词.txt`（见第 11 节）关键字为
> `else|if|int|float|double|return|void|do|while`（**9 个，含 `do`**），且同课程参考实现的
> `文法.txt` 明确包含 `dowhile-stmt -> do statement while ( expression ) ;`、
> 示例程序 `sample.mc` 里也确实使用了 `do ... while`。
> **因此裁定：mini-C 含 `do`，关键字共 9 个（编码 103~111）；`while` 对应 do-while 循环。**
> `data/minic_words.txt` 与 `data/minic_grammar.txt` 须据此编写；另备
> `minic_grammar_taskbook.txt` 保存任务书原文（含缺陷），在测试报告中对比说明。

**（以上为最终裁定，`data/*_words.txt` 与代码中的编码表必须与此表逐字一致。）**

---

## 7. 编码风格与质量红线

1. 禁止 `using namespace std;` 出现在头文件；源文件中若有，必须在包含之后、且仅限该文件。
2. 禁止裸 `new/delete`：优先值语义与 `std::vector`/`QVector`；Qt 对象用父子关系管理。
3. 所有从文件读入的文本按 **UTF-8 优先、失败回退 GBK** 处理（`FileUtil::readText`），
   输出统一 UTF-8。原因：任务书配套的 `TINY的所有单词.txt` 实为 GBK。
4. 算法类（`lex/`、`syntax/`）**不得包含任何 Qt GUI 头文件**（只允许 QtCore），
   以保证可脱离界面被 `--selftest` 与单元测试直接调用。
5. 算法规模上限需防护：正则状态数、DFA 状态数、LR 项目集数超过阈值（默认 20000）时给出明确错误而非崩溃。
6. 提交前自查：无 `TODO` 遗留（除非在 plan.md 中登记）、无调试 `qDebug()` 残留（改用 `Logger`）。

---

## 8. 文档与交付物对应关系（阶段二/三执行，此处先锁定位置）

| 交付物 | 位置 | 状态 |
|---|---|---|
| 源程序 | `src/`、`CompilerCourseDesign.pro` | 完成 |
| 可执行程序 | `build/CompilerCourseDesign/release/*.exe` + Qt DLL | 完成 |
| 测试文本及测试源程序 | `data/`（TINY / mini-C 两组） | 完成 |
| readme | `readme.md` | 完成 |
| 测试报告 | `docs/测试报告.docx`（11 页） | 完成 |
| 系统使用说明书 | `docs/系统使用说明书.docx`（17 页） | 完成 |
| 课程设计实验报告书 | `docs/课程设计实验报告书.docx`（83 页，附录含上述两份文档全文） | 完成 |
| 讲解视频 PPT + 脚本 | `docs/讲解PPT.pptx`（16 页 16:9）、`docs/讲解脚本.md` | 完成（**视频本身尚未录制**） |

三份文档与讲解 PPT 都由 `docs/scripts/` 下的脚本生成，可整体重建：

```powershell
cd docs\scripts
python make_figures.py        # 用 Graphviz 渲染结构图/流程图到 docs/images
python make_report.py         # 生成实验报告书
python make_manual.py         # 生成系统使用说明书
python make_test_report.py    # 生成测试报告
python make_ppt.py            # 生成讲解 PPT（含每页备注）与讲解脚本
python verify_ppt.py          # PPT 结构自检（页数/越界/字体/备注字数）
```

生成后必须用 Word 打开并更新域（目录页码），否则目录只有占位文字 —— 见 §13.39。

---

## 10. 语法树结构（语义动作）文件格式契约

`data/*_semantics.txt` 是需求 2.7「每条文法规则的语法树结构由用户定义，需提供输入窗口」
的落地形式。**文件中一行对应一条产生式，行序 = 文法文件中产生式的出现顺序**
（增广产生式 `S' -> S` 由程序追加在**末尾**，不需要用户写）。

### 10.1 为什么不用参考数据里的「纯数字三元组」写法

老师发布的 `reference/06_tiny_semantics.txt` 用的是一串纯数字（`0 -1 1 -1`）。
实测该写法**无法无歧义解析**：数字 `0` 同时承担「建左部结点」和「子结点下标 0」
两种含义，例如 `0 1 -1 2 -1` 既可读成 4 个组、也可读成 1 个组带 3 个子结点。
本项目因此改用**加字母前缀**的写法：语义完全确定，且仍然保持"只写数字下标"的紧凑性。
与参考实现的**语义结果**保持一致（见 §10.4 的对照断言）。

### 10.2 行格式

整行用 `;` 分成若干**组**，每组产生/复用一个树结点，**最后一组**的结果就是本次规约的返回值。

| 写法 | 含义 |
|---|---|
| `L` / `L c1 c2 …` | **新建**结点，标签取**产生式左部符号名**；`c…` 是要挂成子结点的右部项下标（从 0 计数） |
| `R n` / `R n c1 …` | **复用**右部第 `n` 项**已有**的结点作为本组结点（该位置没有结点时才新建，标签取右部第 `n` 个符号名）；`c…` 同上 |
| `-1 n` | 不建结点，直接把右部第 `n` 项的树根传递上去（单链产生式用它最简洁） |

* `#` 之后是行尾注释，**会被剥掉**（`FileUtil::splitMeaningfulLines()` 只过滤整行注释）；
* 空行与注释行**不占产生式序号**；
* 行数少于产生式条数时，缺失行按默认动作 `L`（建左部结点、无子结点）处理，保证任何文法都能出树；
* 组首记号不是 `L`/`R`/`-1`、或 `R` 后缺序号时报错并给出**行号**，但不中断解析。

### 10.3 三条书写要点（写错任何一条都会让树「断开」）

1. **单链产生式写 `-1 0`，不要写 `L`。**
   `L` 会建一个**没有子结点**的新结点，下层整棵子树随之失去父结点、变成不可达孤岛，
   症状是"分析成功、结点数不少，但树文本只有两三行"。
2. **双目运算写 `R1 0 2`。**
   例如 `exp -> simple-exp comparison-op simple-exp` 要写成 `R1 0 2`：
   取右部第 1 项（`comparison-op -> LT` 建出的 `<` 结点），把第 0、2 项挂成它的子结点，
   于是 `<` 结点直接代表整个 `exp` —— 与老师参考树同形（参考树里 `<` 结点带两个操作数）。
   若写成"新建"，真正的 `<` 结点反而成了孤岛。
3. **`L`/`R` 后面要把所有要保留的子结点都列出来**，漏写等于扔掉整棵子树。
   纯分隔符（`SEMI`、`then`、`end`、`until`）不必挂进树里——它们对语法树结构没有信息量，
   分析结束后 `SyntaxTreeData::pruneUnreachable()` 会把这类未被引用的叶结点清掉。

**TINY 实例**（`data/tiny_semantics.txt`，节选；完整 36 行与文法逐条对应）：

```
-1 0         # 0  program -> stmt-sequence
L 0 2        # 1  stmt-sequence -> stmt-sequence SEMI statement
-1 0         # 3  statement -> if-stmt
R0 1 3       # 8  if-stmt -> if exp then stmt-sequence end
R0 1 3 5     # 9  if-stmt -> if exp then stmt-sequence else stmt-sequence end
R0 1 3       # 10 repeat-stmt -> repeat stmt-sequence until exp
R1 0 2       # 11 assign-stmt -> ID ASSIGN exp
R1 0 2       # 14 exp -> simple-exp comparison-op simple-exp
R1 0 2       # 22 simple-exp -> simple-exp addop term
R0           # 24 addop -> PLUS
R1           # 33 factor -> LPAR exp RPAR
```

### 10.4 与参考实现的语义对照

两边文法不完全相同（本项目按任务书 BNF 意图补全了 `^` 的 `power` 产生式，
参考数据是 34 条产生式），所以不可能逐结点同构。但「哪些语法成分在树上成为结点」
这一约定必须一致。自测 `T15.6b` 断言：

* 参考树 `reference/08_tiny_tree.out`（**81 个结点**）的标签集合为
  `ASSIGN DIVIDE EQ ID LT LTEQ MINUS MOD MULTIPLY NE NUMBER PLUS RT RTEQ
   if read repeat simple-exp stmt-sequence term write`；
* 本项目自建树的标签集合覆盖其中除 `term`、`simple-exp` 外的**全部**标签
  —— 这两个是参考实现保留、而本项目用 `-1 0` 折叠掉的单链结点，差异是有意为之。

### 10.5 实现要点（易错处）

* **规约时弹出右部的下标方向**：`nodeStack` 末尾是右部最右项，`takeLast()` 也从末尾取，
  因此从 `k = rhsLen-1` 倒着循环、每次取栈顶，取到的正好是右部第 `k` 项，
  可以直接写 `rhsNodes[k]`。写成"正着循环 + takeLast()"会让整个右部左右颠倒。
* **父子关系统一记在 `parent` 上**，分析结束后由 `SyntaxTreeData::rebuildChildren()`
  一次算出 `children`。原因是规约自底向上：建父结点时子结点已存在，但"谁是谁的父亲"
  要到父结点被建出来才能定；`R n` 的"复用"更要求给已有结点**改认父结点**。
* **`-1` 既是组内的传递写法，也可以是整行**，两种都要支持。
* 产生式编号：`Grammar::augment()` 把增广产生式**追加在末尾**，所以用户产生式 `i`
  在增广后仍是 `id = i`（`Parser::semanticsOffset()` 返回 0，并有断言锁定这一点）。

---

## 11. 参考材料（调研所得，仅作数据与格式基准）

`reference/` 目录保存了从公开 GitHub 仓库下载的**同课程**历史项目材料，用途只有一个：
**提供权威的输入数据与格式基准，供逐字节／人工比对**。代码一律不复制（见 §7 与用户裁定）。

| 来源仓库 | 许可证 | 取用内容 |
|---|---|---|
| [TatttLeung/Compiler-Principles-Project](https://github.com/TatttLeung/Compiler-Principles-Project) | **无 LICENSE** | 只取**数据**：`tiny_words.txt`、`tiny_grammar.txt`、`tiny_semantics.txt`、`sample.tny`、`minic_*`、`sample.mc`、`output.lex`、`tree.out`、`SLR1Str.txt` |
| [OverdueLin/Compiler_Design_SCNU](https://github.com/Overdue-Lin/Compiler_Design_SCNU) | MIT | 只取**格式与思路**（`regex2dfa`、`lalr`、`tinySyntaxTree` 的 `.pro`/界面组织方式） |
| [MaoMao0404/Compilation-Principle-Project](https://github.com/MaoMao0404/Compilation-Principle-Project) | 无 LICENSE | 只作**图形绘制思路**参考（`graph.cpp`） |
| [Tinex233/2021-CompilationPrinciple_Lab2-LexicalGenerator](https://github.com/Tinex233/2021-CompilationPrinciple_Lab2-LexicalGenerator) | 无 LICENSE | 只作**正则/NFA/DFA 模块划分**参考 |

**红线**：
1. **禁止**把上述仓库的 `.cpp/.h` 代码复制进 `src/`；本项目的每一行代码自行编写；
2. `reference/` 中的数据文件**只用于比对与格式确认**，正式交付的数据文件写在 `data/`；
3. 参考项目的 `output.lex` 是 `NAME:value` 命名格式，与本项目的数字编码格式**不同**，
   比对时以「名字 ↔ 编码」的对应关系人工核对（见 §6 编码表注解）；
4. 若最终报告中提及这些参考材料，须如实写明来源与用途（见 plan.md 阶段 9 的文档任务）。

**已知的参考实现缺陷（本项目须避开）**：
- 参考生成的词法分析程序把输入/输出文件路径**硬编码**在源码里，必须重新编译才能换测试文件
  → 本项目生成的程序**必须**通过命令行参数接收输入源程序与输出 `.lex` 路径。
- 参考实现用的是 **SLR(1)** 分析表（见其 `SLR1Fun1/SLR1Fun2` 与 `SLR1Str.txt`），
  而任务书要求 **LALR(1)**。本项目按 plan.md 实现真正的 LR(1)+同心集合并 LALR(1)，
  并额外提供 SLR(1) 判定窗口作为对比，这是评分档位上的加分点。

---

## 12. 会话工作流（每次开工固定动作）

1. 读 `AGENT.md` → 读 `plan.md` 的当前阶段与进度表。
2. 检查上一阶段产物是否仍可构建：跑 `tools/build.ps1`。
3. 一次只推进一个阶段；改动前先更新 `plan.md` 的进度表（勾选/加注）。
4. 改完立即构建 + 跑 `--console-selftest`，把结果记入 `plan.md` 的进度表备注。
5. 收尾时更新 `plan.md` 与（如有新约定）`AGENT.md`，并提交（如已初始化 git）。

---

## 13. 环境陷阱清单（全部实测踩过，务必遵守）

这一节记录本项目在**本机 Windows + 中文路径 + Qt 6.10.2 + MinGW** 环境下踩过的全部坑。
每一条都曾导致"看起来莫名其妙"的失败，且多数带有很强的误导性，因此必须固化遵守。

### 13.1 工具链必须配套（最高优先级）

| 项 | 正确值 | 错误值（会失败） |
|---|---|---|
| Qt | `D:\Qt6.10\6.10.2\mingw_64` | —— |
| MinGW | `D:\Qt6.10\Tools\mingw1310_64`（g++ 13.1.0） | `D:\mingw\mingw64`（g++ 15.2.0，Brecht-Sanders r5 ucrt 构建） |

`windeployqt` 拷出的 `libstdc++-6.dll` / `libgcc_s_seh-1.dll` 与 Qt 配套；
若 PATH 上先出现另一个 MinGW 的同名运行库，会出现 ABI 不匹配的诡异崩溃。
`tools/build.ps1` 与 `tools/deploy.ps1` 已把正确的 bin 目录**前置**到 PATH。
注意这条规则同样适用于**程序内部**调用 g++ 的场合，但方式不同：不能靠改 PATH，
必须把程序名解析成绝对路径（见 §13.38）。

### 13.2 `.pro` 文件的两条硬规则

1. **只能用 `#` 注释，禁止 `/* ... */` 块注释**。
   qmake 的 `.pro` 解析器不认 C 风格注释；写了块注释会把**整个文件**当成未闭合注释，
   然后对每一行报 `Extra characters after test expression`，错误信息完全指不到真正原因。
2. **不能有 UTF-8 BOM**。有 BOM 时 qmake 直接报 `Unexpected UTF-8 BOM` 并拒绝解析。
   因此 `CompilerCourseDesign.pro` 必须存为 **UTF-8 无 BOM**。

### 13.3 PowerShell 脚本必须带 UTF-8 BOM

本项目用的 Windows PowerShell 5.1 在**没有 BOM** 时按系统 ANSI(GBK) 读取 `.ps1`，
中文注释会被解成乱码并导致**语法解析失败**（报"缺少 )"之类完全无关的错误）。
因此 `tools/*.ps1` 一律存为 **UTF-8 带 BOM**。

> 注意 13.2 与 13.3 方向相反：`.pro` 要无 BOM，`.ps1` 要有 BOM。改完文件请用
> `tools/check_encoding.ps1` 核对。

### 13.4 不要让 PowerShell 用管道捕获本程序的标准输出

GUI 程序（`-subsystem,windows`）的 stdout 被 PowerShell 管道捕获时，
进程会以 `0xC0000005`（访问冲突）或 `0xC0000409` 异常结束，**且没有任何输出**，
看起来像程序自己崩了 —— 实测 `--console-selftest` 与 `--selftest` 都会中招，
而同一命令用 `cmd /c "... > file 2>&1"` 就完全正常。

**正确写法**：

```powershell
cmd /c "`"$exe`" --console-selftest > `"$logFile`" 2>&1"
$code = $LASTEXITCODE
Get-Content $logFile -Encoding UTF8
```

### 13.5 `windeployqt` 不会覆盖已存在的同名 DLL（本项目最坑的一条）

若 `release/` 目录里残留着**其它 Qt 版本**（或其它 MinGW 构建）的 `Qt6Core.dll`，
`windeployqt` 会**跳过拷贝**，于是 `Qt6Core` 与 `Qt6Gui`/`Qt6Widgets` 版本不配套，
程序**双击启动即崩**（`0xC0000005`，连"进入 main"的日志都写不出来，极难定位）。

**更恶劣的是**：不同版本 `Qt6Core.dll` 的**文件大小可能完全相同**
（本例两者都是 11 100 000 字节），因此按大小比对会被骗过去，必须比对**哈希**才能发现。

**对策（已固化进 `tools/deploy.ps1`）**：部署前先删除 `release/` 下的
`Qt6*.dll`、`libstdc++-6.dll`、`libgcc_s_seh-1.dll`、`libwinpthread-1.dll`、
`D3Dcompiler_47.dll`、`opengl32sw.dll` 以及 `platforms/` 等插件子目录，再让
`windeployqt` 从锁定的 Qt 安装目录重新拷贝一份干净的。

### 13.6 offscreen 平台插件不随 `windeployqt` 部署

`windeployqt` 只部署 `platforms/qwindows.dll`，**不含 `qoffscreen.dll`**。
所以：
- 依赖 GUI 的 `--selftest` 在部署目录下**不可用**（缺插件时 Qt 会 hard crash，
  而不是优雅报错）；
- 若确需离屏跑 GUI，要手动从
  `D:\Qt6.10\6.10.2\mingw_64\plugins\platforms\` 拷贝 `qoffscreen.dll`
  到 exe 同目录的 `platforms\` 下。

**这正是自测分两层的原因**：`--console-selftest` 只建 `QCoreApplication`、
不创建任何 GUI 对象，因此不依赖平台插件，是**权威回归手段**；
`--selftest` 需要 offscreen 插件，属可选验证。

### 13.7 offscreen 渲染中文显示为方框（正常现象，不要误判）

offScreen 平台插件不使用系统字体引擎，`window.grab()` 得到的截图里
**所有中文都会渲染成方框（豆腐块）**；即使是最小 Qt 程序（只显示
"hello 中文测试"）也同样如此。因此：

- **不要**据离屏截图判断字库/字体设置有问题；
- 离屏截图只用于检查**布局**（控件位置、表格列宽、页签结构）；
- 在真实 Windows 平台下运行时中文显示正常（已由用户实机确认）。

### 13.8 中文路径对 qmake 的影响

工程位于含中文的目录（`…\2026-02 编译原理课程设计\`）时，qmake 能正常完整构建，
但它在日志里会把路径按系统 ANSI 显示成乱码（`����ԭ��γ����`），
这只是**显示问题**，不影响构建。不过要注意：

- qmake 报错信息里的路径也是乱码，排查时不要被误导；
- 独立的小测试工程若放在**纯 ASCII 目录**却引用**中文目录**下的源文件，
  qmake 会因路径编码转换而 `Failure to find: …`；小测试工程应放在**工程目录内部**
  （如 `build/regextest/`），用相对路径引用 `../../src`。

### 13.9 其它

- `tools/build.ps1` 的参数不能叫 `-Debug`（与 PowerShell 公共参数冲突），
  已改名为 `-DebugBuild`。
- GUI 的 stdout 在 Windows 上无控制台可见，排查启动期问题时用
  `main.cpp` 里的 `traceStartup()` 往 `startup.log` 追加阶段标记。
- 数据文件在 `build/<目标>/data/` 下是 `.pro` 的 `QMAKE_POST_LINK` 拷贝结果；
  **改了 `data/` 下的源文件后必须重新 make（或手动拷贝）才会生效**，
  否则会对着旧数据反复调试（本项目实际浪费过一轮）。

### 13.10 正则式中 `~` 的作用范围与连接符插入（算法类，最容易反复踩）

按任务书优先级，**`~` 是一元前缀运算符，只作用于紧随其后的一个原子**
（字符 / `[ ]` 字符集 / 内置命名）。因此：

- `\{ ~ \}` **不等于**"花括号注释"。它表示"`{` 连接（一个非 `}` 字符）"，
  既不能匹配 `{ab}`，也不能跨行。要匹配花括号注释必须写成 `\{ (~ \})* \}`。
- `endsAtom()` **不能**把 `~` 排除在外：否则 `parseAtom` 的一元运算会一路
  吞掉后面的操作数（`~eoln` 这种写法更是会被切成 `~ . eoln` 而直接报错）。
  正确做法是让 `~` 计入"能结束原子"，但在 `~` 与它**紧邻的操作数**之间
  禁止插入连接记号，并在 `parseAtom()` 里主动吃掉取反项之后的连接记号。
- 词法阶段插入的连接记号是**显式**的，所以 `Concat` 结点可能有多于 2 个子结点
  （`parseConcat` 会把 `a.b.c` 合成一个多叉结点）。后缀式必须为 `n` 个操作数
  补 `n-1` 个 `.`：`RegexAst::buildPostfix()` 与 `RegexToNfa::expandPostfix()`
  **两处都要按 `n-1` 生成**，只改一处会出现"后置式看着对、构造却报
  逆波兰式不完整"的诡异现象。

### 13.11 取反字符全集必须覆盖全部 256 个字节值

`~` 取反需要一个"全集"。若全集只到可打印 ASCII（32~126）：

- UTF-8 汉字的每个字符由 3 个 `0x80~0xFF` 的字节组成，注释自动机遇到
  汉字的第一个字节就无路可走 —— 表现为"注释里只要有中文就被拆成单个符号"；
- `~eoln` 这类写法仍能工作，所以这个缺陷只在**非 ASCII 注释**里暴露。

因此 `RegexAst::unicodeUniverse()` 返回全部 256 个字节值。

**注意区分两件事**（§13.30 改造后尤其重要）：

- **取反全集**是 256 个字节，这一条**没有变**，也不能改；
- **自动机的字母表**不再是 256 个字节，而是约 20 个**字符类**。

过去因为字母表就是字节，取反全集直接导致"字母表 256 个符号"，代价是
NFA/DFA 巨大、状态转换表 258 列、DOT 图边标签挂两百多个符号。改成字符类
字母表后这个代价消失了（TINY：`~ \}` 只需 2 个状态 + 17 条平行边）。

### 13.30 字符类必须用**等价类划分**，不能"归到最具体的原子"

想把字母表从 256 个字节缩到"宏名几个符号"，直觉做法是：给每个字节找
"包含它的最具体（字节数最少）的那个字符集"，用它当类名。**这个做法是错的。**

反例：同时存在 `[a-b]` 与 `[b-c]` 两个原子。

- 按"最具体原子"归类：`b` 归到先出现的 `[a-b]`，于是 `a` 与 `b` 同类；
- 但 `b` 还属于 `[b-c]`，而 `a` 不属于 —— 两者的转移**可以分叉**；
- 结果：同一列里两个字节走不同边，自动机直接出错，而且**不报任何错**。

正确做法（`lex/CharClass.cpp`）：对 0~255 每个字节求出"它属于哪些原子"的
**成员签名**，签名相同者才归为一类。

- 这是**唯一既能把字母表缩小、又严格保持语言不变**的做法：两个字节属于
  完全相同的原子集合 ⟺ 它们在自动机任何状态下的转移都相同（Thompson 构造
  对同一个字符集结点发的是同一对 (源,目的) 状态的平行边）。
- **守门员断言**：`T20.2` 把字符类 DFA 与按字节 DFA 各自最小化后逐字节比对
  全部 256 个转移（BFS 配对判同构），任何分叉都会被抓到。
- 命名规则：有原子与该类字节集**完全相等** → 用该原子名（宏名 > `[..]` 文本 >
  转义字符，同级取声明靠前者）；否则叫 `<other>`（多个残留类依次 `<other2>`…）。
  残留类在图上必须靠"字符类对照"图例给出精确字节集，否则读者无从判断。

### 13.31 字符集要构造成**平行边**，不能展开成 `|` 链

`expandPostfix()` 把一个 n 个字符的字符集写成 `c1 c2 | c3 | …` 时，
n 个字符会产生 n×2 个片段状态，n-1 个选择运算再产生 (n-1)×2 个状态，
合计 **4n−2 个状态**。于是：

- `letter`（52 个字符）≈ 206 个状态；
- `~ \}`（255 个字符）≈ **1018 个状态**；
- TINY 的合并 NFA 因此有 1563 个状态、DOT 图根本没法看。

正确写法（Thompson 的标准写法之一）：一个字符集 = **一对状态 + |C| 条平行
标号边**。逆波兰式里该字符集只发**一个**操作数记号 `\C:键1,键2,…`，
由 `buildFragment()` 展开成平行边。

- 前缀必须是 `\C:` 三字符：按字节模式的记号只有 `\c`（2 字符）、`\xNN`
  （4 字符）与 `\\`，若前缀只写 `\C`，字面量 `C` 的记号会被误判成字符类记号。
- 改造后 TINY 的 NFA 从 1563 个状态降到 **67** 个，DFA 400→26，
  最小化 **24 不变**（语言没变，只是脚手架没了）。

### 13.32 关键字不建自动机 ⇒ 缺 ID 规则时**静默失效**

关键字与 ID 的正则式完全重合，所以本系统**不给关键字建自动机**：
先由 ID 规则识别出标识符，再由生成程序里的 `findKeyword()` 查表改判编码。

由此产生的坑：若规格里写了 `Keyword103L=INT | DOUBLE | BOOL`，却漏了
`ID_101=letter(letter|digit)*`，那么

- 规格解析**不报错**、NFA/DFA 构造也**不报错**；
- 但词法分析时 `INT` 的每个字母都会落进"无法识别的字符"分支，
  `.lex` 里一个令牌都没有；
- 现象看起来像"取反/注释规则写坏了"，实际是缺一条 ID 规则。

因此 `LexPipeline` 在最小化之后必做**关键字可产生性检查**
（`checkKeywordProducibility()`）：对每个关键字用最简 DFA 驱动一遍，
终态编码不是 ID（101）就记一条告警 `Result::warnings`，并显示在界面日志里。

另外记牢三件事：

1. `L` 后缀的编码是 **基号 + k（k 从 0 计）**：`Keyword103L` 的第三项是 **105**；
2. 生成程序用 `strcmp` 查关键字表，**大小写敏感** —— `INT` 与 `int` 是两个词。
   `data/tiny_words.txt` 里"TINY 不区分大小写"的说法与实现不符，以 `strcmp` 为准；
3. 名字**必须**以 `Keyword`/`Reserved` 开头才会被判成关键字类，写成 `Key103L`
   会被当成"普通单词 + L 展开"，反过来给每个关键字各建一条自动机规则。

### 13.33 `Dfa` 里注释态的 `acceptCode` 是声明值，不是 -1

`NfaToDfa::construct()` 里 `out.acceptCode.append(accepting ? tag.code : -1)`，
而注释规则的 `tag.code` 是它**声明的编码**（如 102）—— 也就是说：

- 注释接受态的 `accepting == true`、`acceptCode == 102`、`acceptIsComment == true`；
- "不输出编码"这件事**只由 `acceptIsComment` 表达**，`acceptCode` 并不为 -1。

凡是要判定"这个接受态会不会输出编码"的地方，都必须
`accepting && !acceptIsComment && acceptCode >= 0`，只看 `acceptCode` 会误判。
本项目实际踩过两处：

- 自测里比对参考数据时把注释当成输出了 102；
- 界面"说明"列把注释态显示成"接受态(编码 102)"（已改为"接受态(注释，不输出编码)"）。

（生成的词法程序不受影响：它的 `g_acceptCode` 表在生成时就把注释态写成了 -1。）

### 13.12 生成的 C++ 里**绝不能**用八进制字符字面量做字节比较（高危）

`'\200'` 的类型是 **`char`（有符号）**，值为 **-128**。与 `unsigned char`
的实际字节值比较时，两者都按 `int` 提升，得到 `-128` vs `128`，**恒不相等**；
GCC 还会给出 `comparison is always false due to limited range of data type` 警告。
后果是 DFA 中所有 `0x80~0xFF` 的转移边**全部失效**。

正确写法是整型常量 `0x80`（类型 `int`、值 128）。因此代码生成器里：

- `byteOfSymbol()`：把字母表记号（可能是 `"\xNN"` 转义写法）还原为 0~255 的字节值，
  **不能**只取 `symbol.at(0)`（那会取到 `'\'`）；
- `cppByteLiteral()`：一律输出 `0xNN` 十六进制整型常量，可打印 ASCII 才输出 `'c'`。

同一条规则适用于方法一（`charIndex` 比较链）与方法二（`nextState` 的 `ch == …`）。

### 13.13 LR 构造中结束符 `$` 必须按终结符处理（算法类，影响面极大）

`$`（`kEndMarker`）**不在文法的终结符集合里**，因此
`grammar.isTerminal("$")` 返回 **false**。凡是"按符号序列求 First 集"的函数
都必须显式把 `$` 当终结符，否则会静默产生错误的**空集**：

```cpp
// 错误写法：$ 落到"当非终结符查 First 集"的分支，查不到 → 空集
if (grammar.isTerminal(s)) { result.insert(s); break; }
const QSet<QString> &f = first.value(s);
result.unite(f);
if (!nullable.contains(s)) break;

// 正确写法
if (s == kEndMarker || grammar.isTerminal(s)) { result.insert(s); break; }
```

**症状**：LR(1) 闭包里 `FIRST(βa)` 恒为空集 → `for (const QString &b : firstSet)`
一次都不执行 → 闭包一个项目都展开不出来 → 整个分析表退化成 **2 个状态**
（只剩 `S' -> ·S` 与 `S' -> S ·`）。因为不报错、不崩溃，只表现为"状态数少得离谱"，
**极易被误判成"文法太简单"或"化简把文法删空了"**。

**波及范围**：`FirstFollowResult::firstSetOfSequence()` 与 `sequenceNullable()`
必须用**同一套**"什么算终结符"的口径，否则两处判断不一致。
`FirstFollow::computeFollow()` 内部若也用到了序列求 First，同样要检查。

**守门员断言**：`SelfTest` 的 T14.3f（`FIRST(B$) == {b}`）、T14.6b2（化简后文法规模
未退化）、T14.6b4（初态项目数 > 10）、T14.6b5（初态转移边数 > 10）。
T13 那一轮**没有**暴露这个 bug，因为 T13 只验证 First/Follow 集合本身，
没走"序列 + 结束符"这个入口 —— 这正是"单元测试全绿但集成后崩"的典型样本。

### 13.14 Qt 在 Windows 下 `qDebug()` 不进 stdout，命令行自测看不到

`--console-selftest` 里用 `qDebug()` 打的调试信息**不会出现在重定向的日志文件里**
（Qt 默认把消息送往调试器）。想在无调试器的环境定位算法错误，用下面两种办法之一：

1. **推荐**：把跟踪信息缓冲进一个 `static QStringList`，由自测代码取出来
   塞进断言明细里（`ctx.check(...)` 的 detail 参数会打印到日志）；
2. 设环境变量 `QT_LOGGING_TO_CONSOLE=1` 后重跑。

不要在这上面反复试错 —— 空日志很容易被误判成"代码没被执行到"。


### 13.15 语义动作文件必须剥掉**行尾**注释（阶段 6 实际踩坑）

`FileUtil::splitMeaningfulLines()` 只过滤「整行以 `#` 或 `//` 开头」的注释，
**不处理行尾注释**。而 `data/*_semantics.txt` 每行末尾都写着 `# 序号 产生式` 便于对照，
若不自己剥掉，`#` 之后的 `0 program -> ...` 会被当成组记号，
报出「组首记号 `0` 不是 L / R」。凡是自己写的文本解析器，都要显式处理行尾注释。

### 13.16 语法树「断开成孤岛」的三类成因（阶段 6 最隐蔽的缺陷）

症状高度一致：**语法分析成功、结点数也不少，但 `toText()` 只有两三行**。
原因是除根以外其余结点没有任何父结点引用，从根走不到。三类成因：

1. **单链产生式写成 `L`（新建空结点）而不是 `-1 0`** —— 下层整棵子树失去父结点；
2. **双目运算写成"新建结点"而不是 `R1 0 2`（复用运算符结点）** ——
   真正被规约出来的运算符结点成了孤岛，左侧操作数也接不上；
3. **末尾组是「没有子结点的组」** —— 例如把 `stmt-sequence -> stmt-sequence SEMI statement`
   写成 `L 1 ; L 3`，返回的是第二组那个空壳结点，第一组建的结点被丢弃。

定位手法（已验证有效）：断言 `SyntaxTreeData::reachableCount() == size()`，
并把括号表示 `SyntaxTree::toBracket()` 打进断言明细 —— 一眼就能看出哪一侧操作数丢了。
本项目已把 `T15.5`（最小文法：左递归 + 双目运算）与 `T15.3i2`（TINY 样例）作为常驻断言。

### 13.17 规约弹出右部时的下标方向

`nodeStack` 的**末尾**是右部最右项，`takeLast()` 也从末尾取，因此

```cpp
for (int k = rhsLen - 1; k >= 0; --k)
    rhsNodes[k] = nodeStack.takeLast();     // 正确
```

写成"正着循环 + `takeLast()`"会让整个右部**左右颠倒**：`a + b` 建出来的树变成
「操作符只挂着 `b`」，左侧操作数整块消失，而分析过程完全正常（所以极难察觉）。

### 13.18 增广产生式的插入位置决定语义文件的编号对齐

本项目 `Grammar::augment()` 把 `S' -> S` **追加在末尾**，于是「前 N 条 = 用户写的产生式」
这一对应关系成立，`Parser::semanticsOffset()` 返回 0。
若改成插在下标 0，则所有产生式编号整体后移一位，按 `prod.id` 查语义动作会**每条都取到邻居的语义**
（症状同样是"树整体错位、只剩根可达"）。自测 `T15.3f0` 锁定这一约定。

### 13.19 参考语法树文件里非叶结点的名字写在 `value:` 而不是 `kind:`

`reference/08_tiny_tree.out` 的写法是：

```
BTreeNode
{
    kind: -1                 ← 非叶结点 kind 恒为 -1
    value: stmt-sequence     ← 真正的语法成分名在这里
}
BTreeNode
{
    kind: ID                 ← 叶结点：kind 是单词类别
    value: xXx               ←         value 是单词原文
}
```

解析时必须把非叶结点的 `value` 收作**标签**并清空 `value`，否则所有非叶结点标签都退化成
`-1`，标签直方图与结构签名里完全看不出语法成分名。该文件共 **81** 个结点（81 行 `kind:`），
自测按精确值断言，不要写"大于某个数"。

### 13.20 正则式文件里"运算符字符"作字面量必须转义（阶段 7 联调踩坑）

本系统正则式的运算符是 `| . * + ? ( ) [ ] ~ #`，其余字符都是字面量。
因此写"字面量就是运算符本身"的规则时必须转义：

| 想匹配 | 必须写成 | 裸写的后果 |
|---|---|---|
| `.`（小数点） | `\.` | 被当成**连接运算符**，`digit+ . digit+` 变成"两个 digit+ 的连接"（等于整数），小数规则静默失效 |
| `+`（自增 `++`） | `\+\+` | 被当成**正闭包运算符**，该选择项直接解析失败并报错 |
| `*` | `\*` | 同 `+` |
| `(` `)` `[` `]` | `\(` `\)` `\[` `\]` | 被当成分组/字符集符号 |

排查经验：这类错误分两种表现 ——
* 报错型（`++`）：流水线会明确告诉你"第 N 个选择项解析失败"，好查；
* **静默型**（`.`）：不报任何错，只是那条规则永远匹配不上，
  症状要到"源程序里的小数识别不出来"才浮现。**优先怀疑静默型**。

### 13.21 单词规格文件的编码与编码表必须逐号对齐（整类缺陷的守门员）

`data/*_words.txt` 决定**词法分析程序实际输出哪个编码**，
`LanguageConfig` 的编码表决定**语法分析把该编码翻译成哪个终结符名**。
两者错位时症状是"**词法分析零错误、语法分析却整片报错**"，
而且错误只说"遇到 X 无法继续"，完全指不到根因。

三条必须记住的规则：

1. **带 `L` 后缀的定义，第 k 个选择项拿到的编码是「基号 + k」（k 从 0 计）。**
   所以 `Symbol200L` 的第一项就是 **200**，不是 201 —— 编码完全由书写顺序决定，
   想跳过某个号只能**另起一个定义**，靠写注释是跳不过去的。
   本项目 mini-C 曾把 `%` 写在 `Symbol200L` 中间、却按注释假想它落在 226，
   结果后面的符号整体前移，`^` 落到 205 上，与"小数 `Num_205`"**撞码**。
2. **同一个名字可以对应多个编码**（mini-C 的整数 100 与小数 205 都叫 `NUM`）。
   此时逆表（名字→编码）取**编码较小**者：`reverseTerminalNames` 里要
   "已存在就不覆盖"。若让后来的覆盖前面的，按名字取编码就会取到 205。
3. **每个能输出的编码都必须有名字，编码表里的每个编码也必须能被词法分析产出。**
   自测 `T16.5 / T16.5b` 对 TINY 与 mini-C 各做一次双向核对，
   任何一侧缺口都会当场失败 —— 这两条断言是上述两个真实缺陷的守门员。

### 13.22 括号家族的名字：LLM/RLM 是圆括号，LMM/RMM 是方括号

老师参考数据 `09_minic_words.txt` 用"符号行 + 名字行"逐位对应的写法：

```
\( | \) | \[ | \]   ←→   LLM | RLM | LMM | RMM
```

即 **LLM = `(`，RLM = `)`，LMM = `[`，RMM = `]`**。
mini-C 文法里圆括号用于函数定义/调用/条件/循环，方括号用于数组。
本项目早期把 219 与 221 的名字写反了，后果是 `int f(int x){}` 这类函数定义
在语法分析时按"方括号"去找 `(`，整片报错。守门员是自测的 `T16.1`。

### 13.23 切换语言时必须一并换掉"待测源程序路径"和跨阶段缓存

`MainWindow::setLanguage()` 除了让下拉框换语言，还必须：
* 把页签⑥的待测源程序换成新语言的随附样例 ——
  否则会"用 mini-C 的自动机去跑 TINY 的 `sample.tny`"，大范围词法错误；
* 作废单词规格/NFA/DFA/分析表/语义动作/语法树等**全部跨阶段缓存** ——
  这些都是"针对某一种语言"的，换了语言就不成立；
* 清空页签⑤⑬⑭的显示。

漏掉任何一项，症状都是"切了语言之后结果看起来对、其实全是上一种语言的"。

### 13.24 `emit` 是 Qt 的关键字宏，不能用作自己的函数名

Qt 在 `qglobal.h` 里定义了 `#define emit`（Q_EMIT 的空宏）。
因此任何叫 `emit` 的成员/自由函数都会在调用处被展开掉，
报出一串莫名其妙的 `expected unqualified-id before '(' token`，
而且**报错位置全在调用点**，看不出真正原因是"函数名撞了宏"。

中间代码生成器里原本想写 `ctx.emit("+", a, b, t)`，改名为 `emitQuad` 即通过。
排查口诀：看到大批量 `expected unqualified-id before '('`，
先怀疑函数名撞了 Qt 宏（`emit`、`slots`、`signals`、`foreach`）。

### 13.25 "复用终结符结点"的语义会让终结符结点带上子结点

本项目的语义动作支持 `R n`（复用右部第 n 项的结点）。当第 n 项是**运算符终结符**
（`PLUS`、`ASSIGN`、`if`…）时，该终结符结点会被挂上子结点，于是出现
**`isTerminal == true` 但 `children` 非空**的结点。

任何"按 `isTerminal` 分流"的遍历代码都必须改成
`if (n.isTerminal && n.children.isEmpty())`，否则：

* 中间代码生成会把 `PLUS` 结点当叶结点，返回标签文本 `"PLUS"`，
  于是 `x := 1 + 2` 的四元式变成 `(:=, PLUS, -, x)` —— **整个加法段静默丢失**；
* `SyntaxTree::toText()` 之类若也只看 `isTerminal`，会把运算符当叶子打印，
  看不到它下面的操作数。

这是"树看着正常、代码生成少一半"的典型成因，且不会报任何错。

### 13.26 界面「未响应」的两类根因与治法（实测数据）

现象：点"执行完整词法流水线 / 构造 NFA"后，窗口标题出现「（未响应）」，
点不动、拖不动，过一会儿才恢复。**根因不是死循环，而是把重活放在了界面线程里**：
Windows 只要发现窗口超过约 5 秒不处理消息，就把它标记为未响应。

用自测自带的**耗时报表**（`SelfTestContext::slowStepsText()`，随报告附在末尾）
定位到的实测数据（优化前）：

```
    32.13 秒    T10.3 流水线规格解析与自动机构造全部成功
    31.80 秒    T9.2  TINY 子集构造成功
    31.25 秒    T10.21 方法二同样编译运行成功
    31.00 秒    T11.6 TINY 规格可完成到最简 DFA 的全部步骤
    30.75 秒    T10.23 任务书示例排版只输出一行单词 + 一行 EOF
```

五条各约 31 秒，全是**同一条词法流水线**跑五遍 —— 即单次流水线约 31 秒，
其中**子集构造（NFA→DFA）一步就占 31 秒**。它被调 5 次，合计 160 秒，
是 `--console-selftest` 总耗时 192 秒的绝大部分。

**治法一：先把算法本身修快（收益最大）**

`NfaToDfa::construct` 的内层循环是「DFA 状态数 × 字母表大小」次
`move` + `epsilonClosure`。TINY 是 400 × 约 256 ≈ 10 万次，而：

* `Nfa::move` 每调用一次都**线性扫描全部边**，并逐条做 `QString` 比较
  （`e.symbol != ch`）；
* `Nfa::epsilonClosure` 每调用一次都**重新建一遍 QMap 索引**。

改成在 `construct` 开头预建两份索引（`符号 → (from,to)` 与 `状态 → ε后继`）后，
单次子集构造从 31 秒降到约 9 秒。**注意没有去改 `Nfa::move` 本身** ——
它是清晰的定义式实现，仍被自测当作"参考实现"直接调用；热循环在 `NfaToDfa` 里，
就只修那里（局部化改动，零回归风险）。

顺带试过"只遍历当前状态集合真正用到的符号"（候选符号筛选），结果**没有收益**：
TINY 的取反集合覆盖全部 256 个字节值，绝大多数状态都带 256 条出边，
候选集合几乎等于整个字母表。这条弯路记下来，免得下次再试。

**治法二：让界面在长计算期间继续处理消息**

算法再快也可能有秒级步骤（还有 g++ 编译、运行生成的词法程序）。
凡是超过 1~2 秒的同步操作，都必须让消息队列有机会走一次，否则照样"未响应"。

两处改造（都用 `processEvents(QEventLoop::ExcludeUserInputEvents)`）：

1. **算法层开进度回调**：`NfaToDfa::construct(nfa, out, onProgress)` 每 32 个状态回调一次，
   界面层在回调里刷新状态栏 + 排空消息队列；
2. **子进程等待切片**：`LexBuild::runCommand` 原本 `waitForFinished(60000)` 一刀切阻塞，
   改成每 100ms 一片的循环，片间调用界面层安装的"事件泵"
   （`LexBuild::setEventPump()`，经 `LexPipeline::setEventPump()` 透传）。

**为什么一律用 `ExcludeUserInputEvents`**：只处理重绘与定时器，**不处理鼠标键盘事件**。
若把点击也处理掉，用户可能在上一轮构造还没结束时又点一次按钮，形成重入，
同一份 `m_dfa` / 各面板会被两个流程交错改写 —— 那比"未响应"更难查。

**通用的排查顺序**（下次遇到卡顿直接照这个走）：
1. 先看自测报告末尾的**慢步骤明细**，拿到"哪一步、多少秒"，不要猜；
2. 若某算法本身慢 → 优化它的内层循环（通常是"每次都线性扫描/重建索引"）；
3. 若已无优化空间或耗时来自子进程 → 加进度回调 / 事件泵，让界面活着；
4. 不要用 `processEvents()`（不带参数）—— 会引入重入，见上。

### 13.27 页签顺序会被两件事悄悄破坏（界面结构类）

现象：用户反馈"没有⑦文法编辑器的内容"。实际⑦一直在，只是**被摆到了页签栏最末尾**；
再往下查还发现**⑩ LR(0) DFA 的页签根本不存在**（页签栏一共只有 14 个）。
两个成因都与"页签是怎么加进去的"有关：

**成因一：循环之后再单独 `addTab`。**
原先 `buildTabSpecs()` 里**没有**⑦这一条，`m_grammarPanel` 是在构造循环
**之后**单独 `addTab` 的。于是顺序成了
`① ② ③ ④ ⑤ ⑥ ⑧ ⑨ ⑩ ⑪ ⑫ ⑬ ⑭ ⑮ ⑦` —— 第 7 个位置显示的是⑧。
**治法**：把⑦作为 `specs` 的一条插在⑥与⑧之间，并删掉循环后那次 `addTab`。
**通用规则**：页签顺序只由一处决定（`buildTabSpecs()` 的书写顺序），
不要在别处再 `addTab`，否则"顺序"就有了两个真相。

**成因二：两个页签共用一个控件。**
⑩ 与⑪ 原本共用 `m_lrItemsPanel`，靠面板内的模式下拉框切换。
但 **Qt 的 `QTabWidget::addTab()` 对已经在本页签栏里的控件是"移动"而不是
"新增第二个页签"** —— 第二次 `addTab(同一个控件)` 会把它从原位置摘下来、
再插到末尾。净效果是⑩的页签直接消失，只剩一个⑪。
**治法**：给⑪新建**独立实例**（`LrItemsPanel::setModeIndex()` 把两者默认模式
分别定为 LR(0) 与 LALR(1)）。控件很轻，多一个实例的代价远低于"页签莫名消失"。

**守门员断言 `G1b`**：断言"第 i 个页签的标题以带圈数字 i+1 开头"。
这条断言第一次运行就把上面两个问题**同时**抓了出来
（先报⑦错位，修掉⑦之后立刻又报⑩缺失）。
写它以替代原先按子串猜归属的做法 —— 子串判断本身也是错的：
`tabText.contains("DFA")` 会把⑩「LR(0) DFA」也当成③「DFA」，
把⑩错分到第一项任务（词法）下。归属应按标题开头的圈号判，不能按关键词猜。

### 13.28 编译/运行期间"未响应"（性能类，见 §13.26 的完整数据）

一句话结论：**凡超过 1~2 秒的同步操作，都必须让消息队列有机会走一次**，
否则 Windows 在约 5 秒后把窗口标成「未响应」。
本项目两处用 `QCoreApplication::processEvents(QEventLoop::ExcludeUserInputEvents)`
解决：① 子集构造的进度回调（每 32 个状态一次）；② `QProcess` 等待切片
（每 100ms 一片 + 事件泵）。**一律要带 `ExcludeUserInputEvents`** ——
只处理重绘与定时器，不处理鼠标键盘，避免用户重复点击造成重入。

### 13.29 导出的 .dot 含非 ASCII 字节会让 Graphviz **拒绝出图**

现象（用户反馈）：导出的 DOT 文件 Graphviz 画不出来。
实测报错：

```
Error: automaton.dot: syntax error in line 54 scanning a quoted string
       (missing endquote? longer than 16384?)
```

**根因不是"中文显示成方框"那种字体问题，而是文件根本无法解析。**
本项目自动机的字母表覆盖**全部 256 个字节值**（§13.11 的取反字符全集），
于是"按源状态→目的状态合并后的边标签"里会出现：

* NUL（0x00）—— **最致命**：Graphviz 的 C 词法器遇到它就认为字符串到此为止，
  于是报 "missing endquote"；
* CR / 0x0B / 0x0C 等控制字节 —— 会被当成换行/非法字符，把标签切断；
* 0x80~0xFF 的单字节 —— 写进 UTF-8 文件就是**非法 UTF-8**，新版本 Graphviz
  会直接判语法错误。

**治法：DOT 标签一律只输出可打印 ASCII。**
新增 `FileUtil::escapeForDot()` 作为唯一的出口闸门：

| 输入 | 输出 |
|---|---|
| 可打印 ASCII 0x20~0x7E | 原样 |
| `"` | `\"` |
| `\` | `\\` |
| 其余单字节（控制字符、0x7F、0x80~0xFF） | `<NN>` 十六进制可见表示 |
| 超出单字节的字符（中文等） | `<U+XXXX>` |

`syntax/SyntaxTree::toDot()` 与 `gui/widgets/StateTableView::buildDotText()`
都改走这个函数；两处的 `fontname` 也从 "Microsoft YaHei" 改成 "Helvetica"
（标签已经是纯 ASCII，不需要 CJK 字体了）。

**另一个必须避免的坑：不要转义两遍。**
`displaySymbol()` 原先也调用 `escapeForDot()`，而拼接标签时又转义一次，
结果图上的引号显示成 `\"`、反斜杠显示成 `\\`（多出一个可见反斜杠）。
正确分工是：`displaySymbol()` 只做"字节 → 可见 ASCII"的映射，
**DOT 转义只在最终拼接标签时做一次**。

**验证方式**（可复现）：自测会把 DOT 落盘到 `output/dot/`，
再用真实 Graphviz 跑一遍 `dot -Tpng`（自测进程内不调外部程序）：

```
dot -Tpng output/dot/selftest_automaton.dot -o a.png   → 退出码 0，约 700 KB
dot -Tpng output/dot/selftest_tree.dot      -o t.png   → 退出码 0，约 250 KB
-Tsvg / -Tpdf 同样通过
```

守门员断言：控制台侧 `T18 / T18b / T18c / T18d`（对 256 个字节值 + 中文 +
引号 + 反斜杠做转义，断言结果纯 ASCII、关键字符被可见化、只转义一次、
普通 ASCII 标签不被改动）；界面侧 `G25 / G25b`（真实 DFA 与语法树的 DOT 均为纯 ASCII）。

#### 13.29.1 唯一一处**故意**的非 ASCII：NFA 图上的 ε

NFA 图需要把 ε 边标出来，而 `ε`（U+03B5）过一遍 `escapeForDot()` 会变成
`<U+03B5>` —— 纸上就成了没意义的尖括号。因此：

- 只有 ε 边标签**不过** `escapeForDot()`，文本由
  `StateTableView::epsilonDotLabel()` 单一提供；
- 其余所有标签（字符类名、规则名、语法树结点）仍然必须是可打印 ASCII；
- 这个例外是**可一键撤销**的：把 `epsilonDotLabel()` 的返回值改成 `"eps"`，
  整张图立刻退回纯 ASCII，别处（含自测断言，都是取该函数的返回值来比对）无需改动；
- 守门员断言 `G29.5` 写的是"**除 ε 外**均为可打印 ASCII、且无 C0 控制字符"，
  而不是"纯 ASCII"—— 断言与例外保持同一口径，不靠人记。

> 判断依据：Graphviz 拒绝出图的真正原因是**裸控制字节与非法 UTF-8**
> （§13.29 的根因），一个合法的 UTF-8 希腊字母不在此列，实测能正常渲染。
> 只要不把这条口子开给"任意文本"，风险就只剩字体覆盖（`Helvetica` 在
> Windows 下映射到 Arial，含希腊字母）。

### 13.34 自动机的图必须从**边列表**渲染，不能从 DFA 的转移表凑

现象（用户反馈）：DFA 与最小化 DFA 的图正常，但 **NFA 的图"怪怪的"** ——
一大片状态看着互不相连。实测导出的 NFA DOT 里**一条 ε 边都没有**。

根因：`StateTableView::buildDotText()` 的数据源是 `m_trans`
（`QVector<QMap<QString,int>>`，即"一符号一目的状态"的转移表副本）。那是
**DFA 的结构**，对 NFA 有三处必然丢失：

1. **ε 边进不去** —— `setNfa()` 只遍历 `m_alphabet` 里的符号写 `m_trans`，
   而 ε 是在表里单独占一列的。Thompson 构造出来的 NFA，ε 边占绝大多数
   （TINY：67 个状态 / 88 条边里 43 条是 ε；mini-C：103 个状态 / 112 条边里 69 条），
   丢掉它们，图上自然只剩一堆孤立状态；
2. **一格多目的状态进不去** —— `if (dests.size() == 1)` 才写入；
3. **接受态判据写成了"有编码"** —— `m_acceptCode[state] >= 0`。注释规则的
   接受态 `acceptCode` 是 -1（§13.33），于是它在图上被画成**单圈**。

治法：填充数据时就把"要画的边"原样记成一份**边列表**
（`QVector<DotEdge>{from, to, labels, isEpsilon}`，按 `(源,目的,是否ε)` 合并去重），
DOT 只读这份列表；接受态用 `accepting` 标志（即"该状态在 `accepts` 里"），
双圈节点第二行写编码（DFA）或**规则名**（NFA，如 `Comment` / `Num` / `ID`）。

配套断言（两道，缺一不可）：

- 数据层 `T24.1~T24.5`：NFA 里确实有 ε 边、除统一初态外每个状态都有入边、
  每个非接受态都有出边、接受态数 == 规则数、沿全部边可达每一个状态；
- 渲染层 `G29.1~G29.5`：**解析导出的 DOT** 统计 ε 边数、孤立状态、双圈节点数，
  并核对"双圈数 == NFA 接受态数"。这条断言当时就抓到一个真 bug ——
  跑完"完整词法流水线"后页签②没有刷新，DOT 是 TINY 的（21 个接受态），
  而数据已经是 mini-C 的（29 个）。

### 13.35 树/图的布局坐标是**像素**，不是可以随便当像素用的"逻辑单位"

现象（用户反馈）：页签⑭ 生成的语法树"缩在左上角成一团"，无法放大看清。

根因有**两层**，都要记住：

1. **单位不一致**：`SyntaxTree::layout()` 的注释明写着坐标是"逻辑单位、非像素"
   （当时的实现里宽度 ≈ 叶子个数、高度 ≈ 层数，还有一个恒为 `1.0` 的 `unit` 字段，
   结点框只有 1.0 个单位高 —— 该字段在改造后已删除，因为它只写不读），
   而 `SyntaxTreeView` 直接 `box.x() * zoom` 把它当像素用 —— 76 个结点被画进
   约 69×14 像素的角落，剩下的全是空白。**注释是对的，实现把约定丢掉了。**
2. **"适应窗口"是假的**：`fitZoom()` 用 `width() / layout.width` 算比例，而
   `width()` 本身就是按逻辑单位算出来的控件尺寸（≈ layout.width），
   比值永远 ≈ 0.95 —— 按钮形同虚设。

正确做法：
- **布局参数由界面层按字体度量给出**（`TreeLayoutOptions`：字符宽、框高、层距、
  兄弟间隙），`syntax/` 只吃 `double`，仍然不依赖 QtGui ——
  "核心不依赖 GUI" 与 "坐标必须是像素" 这两件事不矛盾；
- **叶子占"自己的框宽 + 兄弟间隙"的横向槽**，不能固定 1.0：否则
  `variable-definition` 这类长标签的框会横向压住邻居（见 §13.36）；
- **布局结束做归一化**（整体平移到从 (0,0) 起），视图层不必猜原点；
- **视图改用 `QGraphicsView`**：`ScrollHandDrag` 拖动平移、`AnchorUnderMouse`
  锚点缩放、`setAlignment(AlignCenter)` 自动居中、`fitInView` 式适应窗口 ——
  手写滚动条数学既慢又容易错。

守门员断言：
- 数据层 `T15.3k2`：**同一层内结点框两两不重叠**（"成一团"的直接反面），
  且框宽/框高等于配置值、总宽与叶子数匹配；
- 界面层 `G14b~G14d`：自动适应窗口（比例 == fitZoom 且 < 1.0、内容落在视口内）、
  内容居中、放大到 300% 后出现滚动条可拖动。
- 注意 `G14b` 必须**先切到页签⑭**再断言 —— 非当前页的 QGraphicsView 还没被
  布局成最终尺寸，此时算出的适应比例会被下限钳住，不代表用户真正看到的画面。

### 13.36 树布局的叶子槽宽必须跟标签宽度走

只把"逻辑单位换成像素"还不够：早期布局给每个叶子固定 1.0 的横向槽，
而结点框宽度按 `标签长度 × 0.6` 算（十几个字符就是 8~11 个单位）。
也就是说**框比槽宽**，同一层的兄弟结点框必然互相压住 —— 放大多少倍都是糊的。

修法：第一遍后序遍历时，叶子的槽宽取 `自己的框宽 + siblingGap`，
内部结点取 `max(子槽宽之和, 自己的框宽 + siblingGap)`；
先序的区间切分算法不用改（它本来就按"子树宽度"分配）。
实测（TINY 的 76 结点树、框高 26px）：内容 3664×832 像素、14 层、
**同层重叠 0 对**。

### 13.37 状态栏只描述"此刻正在做什么"，不描述"开发进度"

`MainWindow` 状态栏右端曾经写着「当前进度：阶段 0（工程骨架）」。那是 **阶段 0**
留下的：当时 15 个功能页签全是占位面板，这句话用来说明"本项目做到第几阶段"。
功能全部实现后它从未更新，于是成了**永久错误**的界面文案；配套的两样东西也一起烂掉了——
`TabSpec::isPlaceholder`（14 处初始化全写 `true`，**零读取**）、
`reportNotImplemented()`（零调用），本次一并删除（同类的还有随之变成只写不读的
`setSelfTestMode()` / `m_selfTestMode`）。

同一个控件还有第二个问题：它唯一的真实用途是子集构造的进度回调
（`正在构造 DFA…已生成 N 个状态`），但构造结束后**没有人清空** ——
跑完词法流水线后状态栏就一直挂着那句过期进度。也就是说这个标签无论何时都是错的：
要么是假的阶段号，要么是过期的进度。

规则：

1. 进度类文案必须与**收尾清空**成对出现。现在统一走
   `MainWindow::setActivity(text)`（空串 = 回到空闲状态），"设进度"和"清进度"
   写在同一个函数语义里，改一处就不会漏配；事件泵
   （`processEvents(ExcludeUserInputEvents)`）也收拢在这里，见 §13.26；
2. 界面文案不得承载"开发进度"（阶段号、完成百分比）。使用者关心的是**现在能做什么**；
3. 断言守住这两条：`G32`（状态栏不得出现"阶段"字样）、`G33`（流水线结束后活动提示
   必须为空）。`G33` 是"只设不清"这类缺陷的直接守门员。
4. 一般性教训：**骨架期脚手架**（占位面板、阶段提示、"功能将于某阶段实现"的提示）
   在功能补齐后要随阶段删除，否则它们会继续以"看起来合理"的姿态误导使用者 ——
   与 §6.1 注 5b（删掉只用来断言的 `caseSensitive` 字段）是同一类教训。

### 13.38 程序内调用 g++ 必须先把裸名解析成绝对路径

现象：界面上点「编译并测试」**必定失败**，日志只有一句

```
g++: fatal error: cannot execute 'cc1plus': CreateProcess: No such file or directory
```

而 `--console-selftest` 里同样的编译全通过 —— 一条命令两种结果，极易被误判成
"生成的源码有问题"。

原因：`LexBuild::runCommand` 原来用 `setProgram("g++")`（裸名），于是 **argv[0] 就是
`"g++"`，不含任何目录**。GCC 的驱动程序要靠 argv[0] 定位自己的 `cc1plus`/`ld`；
取不到目录时它会**在 PATH 里找"自己"**来推算安装前缀。而同一段代码又为了找
`ld.exe`/`collect2.exe`，把 `D:/Qt6.10/Tools/mingw1310_64/bin` **硬编码前置**进子进程
PATH。当父进程 PATH 里 `D:\mingw\mingw64\bin` 在前时，CreateProcess 真正起来的是
g++ **15.2.0**，可 PATH 里第一个 `g++` 是 Qt 的 **13.1.0** —— 它便去 13.1.0 的前缀下
找 15.2.0 要的 `cc1plus`，于是报上面那句。自测之所以通过，是因为它给编译器传的是
**绝对路径**，argv[0] 自带目录，不必猜前缀。

规则：

1. 用 `QProcess` 起外部工具链时，**先把裸程序名用
   `QStandardPaths::findExecutable()` 解析成绝对路径**，再 `setProgram()`；
2. 补子进程 PATH 只用**解析后那个工具自己**的目录（`<prefix>/bin` 与
   `<prefix>/x86_64-w64-mingw32/bin`），**不要**写死别的 Qt/MinGW 安装位置 ——
   两套工具链的目录混进同一个 PATH，正是本条报错的直接成因；
3. 原实现为此额外存了一份 `m_compilerPathHint`（"第二个线索"），现已删除：
   解析后的绝对路径本身就是唯一线索，**同一个事实不要存两份**；
4. 断言层面靠 `G3` / `G17` 守住：摘要里必须同时出现「g++ 编译 ✓」与「.lex N 行」，
   任一步失败立即变红。本次正是由它们的连锁失败（`.lex` 为空 → 解析失败 → 树为 0 结点）
   暴露出来的。


### 13.39 生成交付文档（三份 docx）时踩到的坑

**（1）老师给的 5 份 PDF 带“禁止复制”权限位，Word 直接拒绝打开。**
现象：`Documents.Open` 对 `编译原理课程设计任务书.pdf` 报“此文档的作者已设置权限，
不允许在其他应用程序中重复使用此内容”。检查发现是 PDF 标准安全处理器（`/V 2 /R 3` = RC4-128）
且**用户口令为空**，权限位只是“禁止复制”标记。Word 会遵守它，但权限位本身是“君子协定”，
于是写了一个最小提取器：用空口令 + `/O` 值 + `/P` + `/ID[0]` 走算法 2 求出文档密钥
（R≥3 还要 MD5 迭代 50 次），再按对象号/代号派生每个对象的 key，RC4 解密对象流与内容流，
最后按 `/ToUnicode` CMap 把 Identity-H 的码位映射成文字。**要点**：内容流里的 `<hex>` 字符串
在解密后的流里已是明文，只要按 `Tf` 切换当时字体、按 `Td/Tm/ET` 断行即可；对象流（`/ObjStm`）
里的对象不再单独加密，解压后直接按 `/First`、`/N` 拆分。

**（2）读 `.doc` 模板不要用 `SaveAs2`。**
`Documents.Open` + `ComputeStatistics` 与 `Content.Text` 都正常，但 `.doc → .docx` 的
`SaveAs2` 会卡住（进程挂在不可见的格式确认对话框上，只能杀 WINWORD）。只要**文本与格式**
时用 `Content.Text`、`Paragraphs.Range.Font`、`PageSetup` 读出来即可，不要在自动化里保存。
（`.docx` 自己生成的文件可以 `Save()`，见下一条。）

**（3）python-docx 写完后必须用 Word 更新域，否则目录只有占位文字。**
`<w:updateFields w:val="true"/>` 只让 Word“打开时提示更新”；要真正写入页码，需用 COM：
`foreach ($story in $doc.StoryRanges) { $story.Fields.Update() }`、`$doc.TablesOfContents.Update()`、
`Repaginate()`、`Save()`。注意：**Word 保存时会剔除与样式相同的冗余 run 属性**（例如正文 run 的
`w:sz`/`w:ascii` 被删掉，只剩 `w:cs`），这不是格式丢失 —— 字号/字体来自 `Normal` 样式
（宋体小四、`line="360" lineRule="exact"` = 固定 18 磅），检查时要看样式而不是 run。

**（4）表格总宽必须 ≤ 14.66 cm（A4 减去左右各 3.17 cm）。**
`table` 里给每列设了 `Cm(w)` 之后，python-docx 仍按列宽排版；总宽超过版心就会压到页边距里。
`verify_docs.py` 专门检查这一条 —— 首轮就查出 9 张表超宽（最宽 17.5 cm，7 列用例表），
改成“1.0 + 2.8 + …”的窄列配 8 pt 字号才装得下。

**（5）图/表编号必须共用一个计数器。**
`Num` 每 `new` 一个就从 1 开始：附录 A（使用说明书）与附录 B（测试报告）各自新建 `Num("")`
时，两边的“表 1、表 2……”撞号。正确做法是**每份文档/每个附录只用一个 `Num` 实例**
（`[Num("A")] * 5` 这种“同一个对象传多次”），附录另用 `A-`/`B-` 前缀。

**（6）Graphviz 画流程图：中文字体用 `SimHei`，图宽会决定插入后的字号。**
`fontname="SimHei"` 在本机可用（Pango 找不到字体会打 WARNING 并回退）；结点文字要手动折行
（≤20 字/行），否则一张图会宽到 25~37 cm，缩到 14.6 cm 插入时字号只剩 4~6 pt。判断方法：
`插入宽度 = min(14.6, 21/高宽比)`，`实际字号 ≈ 11pt × 插入宽度 / 原图宽度(cm)`，低于 7 pt 就要
拆成 (a)(b) 两幅或减少结点。

**（7）界面截图用程序自带的 `--shot`，不要用外部截屏工具。**
`--shot <png> --tab <N> --demo-full` 会先把两条流水线跑完（15 个页签都有真实数据）再抓
1600×1000 的窗口；`--demo-full` 是本阶段新增的开关。外部截屏会抓到桌面、分辨率不一致，
而且无法在无显示器的机器上复现。

**（8）生成脚本要能被复跑。**
`docs/scripts/` 里的脚本分四层：`docs_style.py`（版式：页边距、宋体小四、固定行距 18 磅、
图/表编号器、封面、目录域、代码块底纹）、`content_*.py`（仅内容）、`make_*.py`（组装）、
`verify_docs.py`（自检）+ `make_figures.py`（Graphviz 出图）+ `code_snippet.py`（按函数名从
真实源文件截代码）。这样“报告里的代码与仓库不一致”“编号跳号”“表格超宽”三类问题都能被脚本挡住。

### 13.40 生成讲解 PPT（16 页、16:9）的要点与坑

**（1）python-pptx 只设拉丁字体，中文会回落到宋体。**
`run.font.name = "微软雅黑"` 只写 `a:latin`；中文用的是 `a:ea`（东亚字体）与 `a:cs`。
必须手工补：

```python
rpr = run._r.get_or_add_rPr()
for tag in ("a:ea", "a:cs"):
    el = rpr.find(qn(tag))  # 不存在则 etree.SubElement 追加
    el.set("typeface", "微软雅黑")
```

顺序要注意：先设 `font.name`（生成 `a:latin`）再补 `a:ea`/`a:cs`，否则元素顺序不符合
DrawingML 的 schema。

**（2）“文字是否溢出”必须用 PowerPoint 实测，不能靠字数估。**
PowerPoint COM 里每个形状都能给出文字实际占用的尺寸：
`shape.TextFrame.TextRange.BoundHeight / BoundWidth`，与 `shape.Height/Width` 一比即知溢出
（容差给 5 pt）。本项目 32 页形状一次就查出一处：封面副标题框高 79 pt、文字实际 93 pt。
另外 `Presentations.Open(path, ReadOnly=$true, Untitled=$false, WithWindow=$false)` +
`Slide.Export(png, "PNG", 1600, 900)` 可以把每页导出成图，用于人工抽查与“空页/图片没加载”
的客观检查（PNG 的墨迹占比）。

**（3）PowerShell 5.1 读**无 BOM**的 `.ps1` 会按 GBK 解码 —— 脚本里不要写中文路径。**
写临时校验脚本时踩了两次：
- 中文提示语变成乱码 → 解析报 `Unexpected token`；
- 更隐蔽的一次：脚本里写 `Join-Path $proj 'docs\讲解PPT.pptx'`，路径被解码成乱码，
  PowerPoint 报 `0x80070002 找不到文件`（看起来像“PPT 坏了”，其实是路径错了）。
**做法**：临时脚本全用 ASCII 提示语，并且**用 `Get-ChildItem -Filter '*.pptx'` 枚举**而不是写中文
文件名。（仓库内的三个 `.ps1` 有 BOM，不受此影响，见 §13.3。）

**（4）PPT 版式的几条硬约束（16:9 = 13.333×7.5 in）。**
- 内容区左右边距 0.9 in，即图片右边界 ≤ 12.43 in；
- 图片用「保持比例、限高」缩放（Pillow 读原图尺寸算比例），长流程图只上方幅；
- 每页：一句话标题（27~32 pt）+ ≤3 条要点（15~19 pt）+ 一个视觉；**不要抄报告段落**；
- 页脚统一“第 n / 16”，右下角页码；配色固定（`#1A5FB4` 主色 + 深灰 + 浅灰底）。

**（5）讲稿放在备注里，再导出成 md，避免两份不同步。**
`slide.notes_slide.notes_text_frame.text = 讲稿`，然后由同一份数据生成 `docs/讲解脚本.md`；
按 190 字/分钟估算时长（本项目 16 页共 2280 字 ≈ 12 分钟口播 + 2 分钟实机演示 = 15 分钟）。
脚本要能被口头念：每页 120~180 字、口语、不出现表格里才有的数字堆。
