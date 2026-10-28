# =============================================================================
# CompilerCourseDesign.pro -- 编译原理课程设计系统 主程序工程文件
# -----------------------------------------------------------------------------
# 版权信息 : 编译原理课程设计 - 华南师范大学计算机学院
# 文件名称 : CompilerCourseDesign.pro
# 摘要     : 定义编译原理课程设计系统（词法分析生成器 + LALR(1) 语法分析
#            生成器）的构建配置：Qt6 Widgets、C++17、告警全开，以及把
#            data/ 下的语言数据文件复制到构建输出目录。
# 版本号   : 1.0.0     作者/修改者 : <学号 姓名>   完成日期 : 2026-XX-XX
# 版本历史 : 1.0.0  2026-XX-XX  首次建立工程骨架
#
# 编码注意事项（重要，踩过的坑）：
#   1. qmake 的 .pro 解析器 **不支持 C 风格块注释**，只认行首 # 注释。
#      若用块注释写文件头，qmake 会把整个文件当作未闭合注释处理，
#      并报出一串 "Extra characters after test expression" 错误。
#   2. qmake 读取 .pro 时若发现 UTF-8 BOM 会直接报 "Unexpected UTF-8 BOM"。
#      因此本文件必须保存为 **UTF-8 无 BOM**（系统 ANSI/GBK 也可以）。
#   3. C++ 源码（src/*.cpp、src/*.h）不受上述限制，统一 UTF-8 无 BOM。
# =============================================================================

QT       += core gui widgets

CONFIG   += c++17
CONFIG   -= console

# 告警全开：AGENT.md §2.3 要求 0 error 0 warning
QMAKE_CXXFLAGS += -Wall -Wextra

TARGET   = CompilerCourseDesign
TEMPLATE = app

# ---- 源文件 ----------------------------------------------------------------
INCLUDEPATH += $$PWD/src

HEADERS += \
    src/common/Types.h \
    src/common/Logger.h \
    src/common/FileUtil.h \
    src/common/LanguageConfig.h \
    src/common/SelfTest.h \
    src/lex/RegexAst.h \
    src/lex/CharClass.h \
    src/lex/TokenSpec.h \
    src/lex/RegexToNfa.h \
    src/lex/NfaToDfa.h \
    src/lex/DfaMinimize.h \
    src/lex/LexCodeGen.h \
    src/lex/LexBuild.h \
    src/lex/LexPipeline.h \
    src/syntax/Grammar.h \
    src/syntax/GrammarSimplify.h \
    src/syntax/FirstFollow.h \
    src/syntax/LrItems.h \
    src/syntax/Lr1Items.h \
    src/syntax/ParseTable.h \
    src/syntax/SemanticsAction.h \
    src/syntax/LexReader.h \
    src/syntax/Parser.h \
    src/syntax/SyntaxTree.h \
    src/syntax/IntermediateCode.h \
    src/gui/MainWindow.h \
    src/gui/widgets/StateTableView.h \
    src/gui/panels/WordsEditorPanel.h \
    src/gui/panels/NfaPanel.h \
    src/gui/panels/DfaPanel.h \
    src/gui/panels/LexCodePanel.h \
    src/gui/panels/LexTestPanel.h \
    src/gui/panels/GrammarEditorPanel.h \
    src/gui/panels/FirstFollowPanel.h \
    src/gui/panels/GrammarSimplifyPanel.h \
    src/gui/panels/LrItemsPanel.h \
    src/gui/panels/ParseTablePanel.h \
    src/gui/panels/SyntaxProcessPanel.h \
    src/gui/panels/SyntaxTreePanel.h \
    src/gui/panels/SyntaxTreeView.h \
    src/gui/panels/IntermediateCodePanel.h

SOURCES += \
    src/main.cpp \
    src/common/Types.cpp \
    src/common/Logger.cpp \
    src/common/FileUtil.cpp \
    src/common/LanguageConfig.cpp \
    src/common/SelfTest.cpp \
    src/lex/RegexAst.cpp \
    src/lex/CharClass.cpp \
    src/lex/TokenSpec.cpp \
    src/lex/RegexToNfa.cpp \
    src/lex/NfaToDfa.cpp \
    src/lex/DfaMinimize.cpp \
    src/lex/LexCodeGen.cpp \
    src/lex/LexBuild.cpp \
    src/lex/LexPipeline.cpp \
    src/syntax/Grammar.cpp \
    src/syntax/GrammarSimplify.cpp \
    src/syntax/FirstFollow.cpp \
    src/syntax/LrItems.cpp \
    src/syntax/Lr1Items.cpp \
    src/syntax/ParseTable.cpp \
    src/syntax/SemanticsAction.cpp \
    src/syntax/LexReader.cpp \
    src/syntax/Parser.cpp \
    src/syntax/SyntaxTree.cpp \
    src/syntax/IntermediateCode.cpp \
    src/gui/MainWindow.cpp \
    src/gui/widgets/StateTableView.cpp \
    src/gui/panels/WordsEditorPanel.cpp \
    src/gui/panels/NfaPanel.cpp \
    src/gui/panels/DfaPanel.cpp \
    src/gui/panels/LexCodePanel.cpp \
    src/gui/panels/LexTestPanel.cpp \
    src/gui/panels/GrammarEditorPanel.cpp \
    src/gui/panels/FirstFollowPanel.cpp \
    src/gui/panels/GrammarSimplifyPanel.cpp \
    src/gui/panels/LrItemsPanel.cpp \
    src/gui/panels/ParseTablePanel.cpp \
    src/gui/panels/SyntaxProcessPanel.cpp \
    src/gui/panels/SyntaxTreePanel.cpp \
    src/gui/panels/SyntaxTreeView.cpp \
    src/gui/panels/IntermediateCodePanel.cpp

# ---- 随程序分发的数据文件 ---------------------------------------------------
# 说明：使用 QMAKE_POST_LINK 在链接完成后，把源码目录 data/ 下的全部语言数据
#       （单词正则式、BNF 文法、语义动作、测试源程序、任务书原文文法）整体
#       复制到构建输出目录的 data/ 下，使程序在 exe 同目录即可找到随附数据。
# 输入：$$PWD/data/         输出：$$OUT_PWD/data/
# 注意：
#   1. 只用一条 xcopy 命令，避免用 QMAKE_EXTRA_COMPILERS 时每个目标必须
#      唯一命名所带来的脆弱性（qmake 的 = 续行与 $${} 展开在此处不稳定）；
#   2. xcopy 的 /Y 覆盖同名文件、/I 把目标当目录、/E 含空子目录；
#   3. 若源码目录没有 data/，则不生成任何命令。
GOAL_DATA_DIR = $$shell_path($$PWD/data)
exists($$PWD/data) {
    DESTDIR_DATA = $$shell_path($$OUT_PWD/data)
    QMAKE_POST_LINK += $$quote(cmd /c if not exist \"$$DESTDIR_DATA\" mkdir \"$$DESTDIR_DATA\"$$escape_expand(\n\t))
    QMAKE_POST_LINK += $$quote(cmd /c xcopy /Y /E /I \"$$GOAL_DATA_DIR\" \"$$DESTDIR_DATA\" >nul$$escape_expand(\n\t))
}