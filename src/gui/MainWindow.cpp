/******************************************************************************
 * (1) 版权信息 : 编译原理课程设计 - 华南师范大学计算机学院
 * (2) 文件名称 : MainWindow.cpp
 *     标识符   : MainWindow
 *     摘要     : 主窗口的实现。建立菜单栏/工具栏/状态栏、左侧功能树、中央
 *                功能页签与底部日志窗口，并负责语言数据载入与各功能面板之间
 *                的调度。本文件只做界面编排，不含编译原理算法。
 * (3) 版本号   : 1.0.0       作者/修改者 : <学号 姓名>   完成日期 : 2026-XX-XX
 * (4) 版本历史 : 1.0.0  2026-XX-XX  建立界面骨架、语言切换、日志面板
 *****************************************************************************/

#include "gui/MainWindow.h"

#include <QAction>
#include <QApplication>
#include <QComboBox>
#include <QDesktopServices>
#include <QDir>
#include <QDockWidget>
#include <QFileDialog>
#include <QFileInfo>
#include <QFont>
#include <QFontDatabase>
#include <QLabel>
#include <QMenu>
#include <QMenuBar>
#include <QMessageBox>
#include <QCoreApplication>
#include <QEventLoop>
#include <QPlainTextEdit>
#include <QSplitter>
#include <QStatusBar>
#include <QTabWidget>
#include <QToolBar>
#include <QTreeWidget>
#include <QTreeWidgetItem>
#include <QUrl>

#include "common/FileUtil.h"
#include "common/LanguageConfig.h"
#include "common/Logger.h"
#include "gui/panels/DfaPanel.h"
#include "gui/panels/FirstFollowPanel.h"
#include "gui/panels/GrammarEditorPanel.h"
#include "gui/panels/GrammarSimplifyPanel.h"
#include "gui/panels/IntermediateCodePanel.h"
#include "gui/panels/LexCodePanel.h"
#include "gui/panels/LexTestPanel.h"
#include "gui/panels/LrItemsPanel.h"
#include "gui/panels/NfaPanel.h"
#include "gui/panels/ParseTablePanel.h"
#include "gui/panels/SyntaxProcessPanel.h"
#include "gui/panels/SyntaxTreePanel.h"
#include "gui/panels/WordsEditorPanel.h"
#include "gui/widgets/StateTableView.h"
#include "lex/DfaMinimize.h"
#include "lex/LexPipeline.h"
#include "lex/NfaToDfa.h"
#include "lex/RegexToNfa.h"
#include "syntax/FirstFollow.h"
#include "syntax/GrammarSimplify.h"
#include "syntax/LexReader.h"
#include "syntax/Lr1Items.h"
#include "syntax/LrItems.h"
#include "syntax/ParseTable.h"
#include "syntax/Parser.h"
#include "syntax/SyntaxTree.h"

namespace ccd {

namespace {

/** 一个功能页签的描述：标题、对应任务书要求编号、计划内容 */
struct TabSpec {
    QString title;
    QString requirement;
    QString detail;
};

/**
 * 功能：构建全部功能页签的清单。
 * 输入：无。
 * 输出：页签描述列表（顺序即界面页签顺序）。
 * 注意：与 plan.md §5 的"界面页签 ↔ 需求编号"对应表严格一致，改动须同步更新文档。
 */
QVector<TabSpec> buildTabSpecs()
{
    QVector<TabSpec> specs;

    // ② NFA
    specs.append({QStringLiteral("② NFA"),
                  QStringLiteral("要求 1.4"),
                  QStringLiteral("以状态转换表呈现由全部单词正则式合并构造出的 NFA："
                                 "状态、ε 转移、各输入符号上的转移、接受状态及其对应单词编码。")});
    // ③ DFA
    specs.append({QStringLiteral("③ DFA"),
                  QStringLiteral("要求 1.5"),
                  QStringLiteral("以状态转换表呈现子集构造法（ε 闭包 + move）得到的 DFA，"
                                 "并显示每个 DFA 状态对应的 NFA 状态集合。")});
    // ④ 最小化 DFA
    specs.append({QStringLiteral("④ 最小化DFA"),
                  QStringLiteral("要求 1.6"),
                  QStringLiteral("以状态转换表呈现划分法（Hopcroft）最小化后的 DFA，"
                                 "并对比最小化前后的状态数。")});
    // ⑤ 词法分析源程序
    specs.append({QStringLiteral("⑤ 词法分析源程序"),
                  QStringLiteral("要求 1.7"),
                  QStringLiteral("由最小化 DFA 生成 C/C++ 词法分析源程序，"
                                 "只允许采用讲稿中的转换方法一（Judgechar/GetNext/GetToken）"
                                 "或转换方法二（switch(state)），此处可查看与保存生成的源码。")});
    // ⑥ 编译与词法测试
    specs.append({QStringLiteral("⑥ 编译与词法测试"),
                  QStringLiteral("要求 1.8 / 1.9"),
                  QStringLiteral("对生成的词法分析源程序调用 C/C++ 编译器生成可执行程序，"
                                 "再以该语言的源程序（如 sample.tny）测试，"
                                 "输出并显示单词编码文件 sample.lex。")});
    // ⑦ 文法编辑器
    // 注意：⑦ 必须**排在这里**（⑥ 之后、⑧ 之前），与标题上的圈号一致。
    // 本项目早期漏了这一条，改成在循环之后单独 addTab，
    // 结果⑦被摆到页签栏最右边（顺序成了 ①…⑥ ⑧…⑮ ⑦），
    // 用户看到"第 7 个页签是⑧ 文法化简结果"，以为⑦没做出来。
    specs.append({QStringLiteral("⑦ 文法编辑器"),
                  QStringLiteral("要求 2.1"),
                  QStringLiteral("提供输入窗口编辑 BNF 文法：可从随附数据文件载入、"
                                 "也可手工修改；支持另存、恢复，并一键触发"
                                 "「化简 → First/Follow → 项目集 → 分析表」整条流水线。")});
    // ⑧ 文法化简结果
    specs.append({QStringLiteral("⑧ 文法化简结果"),
                  QStringLiteral("要求 2.2"),
                  QStringLiteral("显示文法简化结果：被删除的不可达规则、"
                                 "被删除的无法推导出句子的规则，以及化简后的文法全文。")});
    // ⑨ First / Follow
    specs.append({QStringLiteral("⑨ First/Follow"),
                  QStringLiteral("要求 2.3"),
                  QStringLiteral("以两张表格分别呈现每个非终结符号的 First 集合与 Follow 集合。")});
    // ⑩ LR(0) DFA
    specs.append({QStringLiteral("⑩ LR(0) DFA"),
                  QStringLiteral("要求 2.4"),
                  QStringLiteral("呈现文法对应的 LR(0) 项目集规范族 DFA："
                                 "项目集列表、状态之间的转移（点-边数据）及图形。")});
    // ⑪ LALR(1) 项目集
    specs.append({QStringLiteral("⑪ LALR(1) 项目集"),
                  QStringLiteral("要求 2.4 补充"),
                  QStringLiteral("呈现由 LR(1) 项目集合并同心集后得到的 LALR(1) 项目集，"
                                 "并给出同心集合并的对照关系。")});
    // ⑫ LALR(1) 分析表
    specs.append({QStringLiteral("⑫ LALR(1) 分析表"),
                  QStringLiteral("要求 2.5"),
                  QStringLiteral("以表格形式呈现 ACTION / GOTO 分析表；"
                                 "若存在移进-规约或规约-规约冲突，逐条列出冲突明细。")});
    // ⑬ 语法分析过程
    specs.append({QStringLiteral("⑬ 语法分析过程"),
                  QStringLiteral("要求 2.6"),
                  QStringLiteral("打开任务一输出的单词编码文件，用 LALR(1) 分析方法"
                                 "逐行显示语法分析过程（步骤、状态栈、符号栈、剩余输入、动作）。")});
    // ⑭ 语法树
    specs.append({QStringLiteral("⑭ 语法树"),
                  QStringLiteral("要求 2.7 / 2.8"),
                  QStringLiteral("提供语法树结构定义（语义动作）编辑窗口，"
                                 "在分析过程中建树并以树状形式呈现，支持缩放与导出图片。")});
    // ⑮ 中间代码（选做）
    specs.append({QStringLiteral("⑮ 中间代码(选做)"),
                  QStringLiteral("要求 2.11（选做）"),
                  QStringLiteral("在语义动作中生成四元式中间代码，以表格呈现并可导出。")});

    return specs;
}

}  // namespace

/**
 * 功能：构造主窗口。
 * 输入：parent —— 父控件。
 * 输出：无。
 * 注意：构造完成后必须调用 initialize()，否则语言数据不会载入。
 */
MainWindow::MainWindow(QWidget *parent) : QMainWindow(parent)
{
    buildActionsAndMenus();
    buildCentralArea();
    buildLogDock();
    updateLanguageIndicators();
}

/**
 * 功能：初始化主窗口。
 * 输入：language —— 默认语言。
 * 输出：无。
 * 注意：
 *  1. 先把日志器的信号接到日志面板，保证后续所有日志都能显示；
 *  2. 载入①单词规格与⑦文法两个编辑器；
 *  3. 不弹出任何模态对话框，避免启动时阻塞（无头自测需要）。
 */
void MainWindow::initialize(LanguageId language)
{
    // 连接日志：历史日志一次性回填，之后的日志实时追加
    for (const QString &line : Logger::instance()->history())
        appendLogLine(line);
    connect(Logger::instance(), &Logger::messageLogged, this, &MainWindow::appendLogLine);

    const int index = static_cast<int>(language);
    if (index >= 0 && index < m_languageCombo->count())
        m_languageCombo->setCurrentIndex(index);

    // 显式载入一次语言数据。
    // 注意：不能只依赖下拉框的 currentIndexChanged 信号——当目标下标恰好等于
    // 当前下标（初始即为 0）时信号不会发出，编辑框就会是空的，后续"解析规格"
    // 必然失败。这里显式调用一次，保证无论下标是否变化数据都已就位。
    m_language = language;
    m_wordsPanel->loadLanguageWords(m_language);
    m_grammarPanel->loadLanguageGrammar(m_language, m_grammarPanel->useFixedGrammar());
    updateLanguageIndicators();

    LOG_INFO(QStringLiteral("编译原理课程设计系统启动完成（Qt %1）。")
                 .arg(QString::fromLatin1(qVersion())));
    LOG_INFO(QStringLiteral("当前测试语言：%1；随附数据目录：%2")
                 .arg(LanguageConfigRegistry::get(m_language).displayName,
                      FileUtil::toNativePath(FileUtil::dataDir())));
}

/**
 * 功能：切换到指定序号的中央功能页签。
 * 输入：index —— 页签序号（从 0 开始）。
 * 输出：切换成功为 true；越界返回 false。
 * 注意：供 --shot 截图与功能树导航使用。
 */
bool MainWindow::selectTab(int index)
{
    if (!m_tabWidget || index < 0 || index >= m_tabWidget->count())
        return false;
    m_tabWidget->setCurrentIndex(index);
    return true;
}

/**
 * 功能：【自测用】程序化执行一次"解析规格 → NFA → DFA → 最小化 DFA"的界面流程。
 * 输入：outSummary —— 输出流程摘要。
 * 输出：成功为 true。
 * 注意：本方法调用的是与按钮点击相同的槽函数，因此可真实反映界面功能；
 *       不弹出任何对话框（解析与构造的错误都以日志形式记录）。
 */
bool MainWindow::runLexicalUiCheck(QString &outSummary)
{
    outSummary.clear();

    // 第 1 步：解析单词规格（等价于点击"解析规格"按钮）
    onParseWordsRequested();
    if (!m_tokenSpecValid) {
        outSummary = QStringLiteral("单词规格解析失败");
        return false;
    }
    const int defCount = m_tokenSpec.defs().size();
    const int autoCount = m_tokenSpec.automatonDefs().size();
    const int kwCount = m_tokenSpec.keywordDefs().size();

    // 第 2 步：级联构造 NFA → DFA → 最小化 DFA
    // （等价于点击页签②的"由当前规格构造 NFA"按钮）
    if (!buildAndShowAutomata()) {
        outSummary = QStringLiteral("自动机级联构造失败");
        return false;
    }

    // 第 3 步：核对三个页签都已拿到数据
    if (!m_nfaPanel || !m_dfaPanel || !m_minDfaPanel)
        return false;

    outSummary = QStringLiteral("定义 %1 个（宏 %2、关键字定义 %3、参与自动机 %4）；"
                                "NFA %5 状态 → DFA %6 状态 → 最小化 DFA %7 状态")
                     .arg(defCount)
                     .arg(m_tokenSpec.macros().size())
                     .arg(kwCount)
                     .arg(autoCount)
                     .arg(m_nfaValid ? m_nfa.stateCount : -1)
                     .arg(m_dfaValid ? m_dfa.stateCount : -1)
                     .arg(m_minDfaValid ? m_minDfa.stateCount : -1);
    return m_nfaValid && m_dfaValid && m_minDfaValid;
}

// ---------------------------------------------------------------------------
// 界面搭建
// ---------------------------------------------------------------------------

/**
 * 功能：建立动作、菜单栏与工具栏。
 * 输入：无。
 * 输出：无。
 * 注意：菜单结构为 文件 / 任务一(词法) / 任务二(语法) / 视图 / 帮助。
 */
void MainWindow::buildActionsAndMenus()
{
    // ---- 动作 ----
    m_actReloadData = new QAction(QStringLiteral("重新载入当前语言数据"), this);
    m_actReloadData->setShortcut(QKeySequence(QStringLiteral("Ctrl+R")));
    connect(m_actReloadData, &QAction::triggered, this, [this] {
        m_wordsPanel->loadLanguageWords(m_language);
        m_grammarPanel->loadLanguageGrammar(m_language, m_grammarPanel->useFixedGrammar());
    });

    m_actOpenDataDir = new QAction(QStringLiteral("打开数据目录"), this);
    connect(m_actOpenDataDir, &QAction::triggered, this, &MainWindow::onOpenDataDir);

    m_actExit = new QAction(QStringLiteral("退出"), this);
    m_actExit->setShortcut(QKeySequence::Quit);
    connect(m_actExit, &QAction::triggered, this, &QWidget::close);

    m_actParseWords = new QAction(QStringLiteral("解析单词规格"), this);
    connect(m_actParseWords, &QAction::triggered, this, &MainWindow::onParseWordsRequested);

    m_actLexPipeline = new QAction(QStringLiteral("执行完整词法流水线"), this);
    connect(m_actLexPipeline, &QAction::triggered, this,
            &MainWindow::onFullLexPipelineRequested);

    m_actSimplifyGrammar = new QAction(QStringLiteral("载入并化简文法"), this);
    connect(m_actSimplifyGrammar, &QAction::triggered, this,
            &MainWindow::onLoadAndSimplifyRequested);

    m_actSyntaxPipeline = new QAction(QStringLiteral("执行完整语法流水线"), this);
    connect(m_actSyntaxPipeline, &QAction::triggered, this,
            &MainWindow::onFullSyntaxPipelineRequested);

    m_actClearLog = new QAction(QStringLiteral("清空日志"), this);
    connect(m_actClearLog, &QAction::triggered, this, &MainWindow::onClearLog);

    m_actAbout = new QAction(QStringLiteral("关于本系统"), this);
    connect(m_actAbout, &QAction::triggered, this, &MainWindow::onAbout);

    m_actAboutQt = new QAction(QStringLiteral("关于 Qt"), this);
    connect(m_actAboutQt, &QAction::triggered, this, &MainWindow::onAboutQt);

    // ---- 菜单栏 ----
    QMenu *fileMenu = menuBar()->addMenu(QStringLiteral("文件(&F)"));
    fileMenu->addAction(m_actReloadData);
    fileMenu->addAction(m_actOpenDataDir);
    fileMenu->addSeparator();
    fileMenu->addAction(m_actExit);

    QMenu *lexMenu = menuBar()->addMenu(QStringLiteral("任务一：词法分析(&L)"));
    lexMenu->addAction(m_actParseWords);
    lexMenu->addAction(m_actLexPipeline);

    QMenu *syntaxMenu = menuBar()->addMenu(QStringLiteral("任务二：语法分析(&S)"));
    syntaxMenu->addAction(m_actSimplifyGrammar);
    syntaxMenu->addAction(m_actSyntaxPipeline);

    QMenu *viewMenu = menuBar()->addMenu(QStringLiteral("视图(&V)"));
    viewMenu->addAction(m_actClearLog);

    QMenu *helpMenu = menuBar()->addMenu(QStringLiteral("帮助(&H)"));
    helpMenu->addAction(m_actAbout);
    helpMenu->addAction(m_actAboutQt);

    // ---- 工具栏 ----
    QToolBar *toolBar = addToolBar(QStringLiteral("主工具栏"));
    toolBar->setObjectName(QStringLiteral("mainToolBar"));
    toolBar->setMovable(false);

    m_languageCombo = new QComboBox(toolBar);
    for (const LanguageConfig &cfg : LanguageConfigRegistry::all())
        m_languageCombo->addItem(cfg.displayName);
    m_languageCombo->setToolTip(QStringLiteral("选择测试语言（任务书要求先测 TINY，再测 mini-C）"));
    toolBar->addWidget(new QLabel(QStringLiteral(" 测试语言："), toolBar));
    toolBar->addWidget(m_languageCombo);
    toolBar->addSeparator();
    toolBar->addAction(m_actReloadData);
    toolBar->addSeparator();
    toolBar->addAction(m_actLexPipeline);
    toolBar->addAction(m_actSyntaxPipeline);

    connect(m_languageCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), this,
            &MainWindow::onLanguageChanged);

    // ---- 状态栏 ----
    // 三段：当前语言 / 实际使用的数据文件 / 当前操作提示（空闲为空串）。
    // 第三段只允许描述"此刻正在做什么"，不描述"开发进度" —— 早期它显示
    // "当前进度：阶段 0（工程骨架）"，功能全部实现后就成了永久错误的文案。
    m_statusLanguage = new QLabel(this);
    m_statusDataFile = new QLabel(this);
    m_statusActivity = new QLabel(this);
    statusBar()->addPermanentWidget(m_statusLanguage);
    statusBar()->addPermanentWidget(m_statusDataFile, 1);
    statusBar()->addPermanentWidget(m_statusActivity);
}

/**
 * 功能：建立中央功能页签与左侧功能树。
 * 输入：无。
 * 输出：无。
 * 注意：
 *  1. 15 个页签全部是真实面板，页签标题与面板的对应关系在下方 if 链里逐项写明；
 *  2. 页签的**顺序**由 `buildTabSpecs()` 决定，新增/调整页签必须同时改两处 ——
 *     过去就出现过"⑦ 漏在 specs 里、被 addTab 到末尾"与"⑩⑪ 共用同一控件导致
 *     一个页签消失"两个缺陷，现由断言 `G1b`（标题必须是 ①→⑮）守住；
 *  3. 中央用 QSplitter 让用户可拖动调整功能树宽度。
 */
void MainWindow::buildCentralArea()
{
    m_wordsPanel = new WordsEditorPanel(this);
    m_nfaPanel = new NfaPanel(this);
    m_dfaPanel = new DfaPanel(QStringLiteral("③ DFA"), QStringLiteral("要求 1.5"),
                              QStringLiteral("由 NFA 构造 DFA（子集构造法）"), this);
    m_minDfaPanel = new DfaPanel(QStringLiteral("④ 最小化 DFA"), QStringLiteral("要求 1.6"),
                                 QStringLiteral("最小化 DFA（划分法）"), this);
    m_minDfaPanel->setDetailTitle(QStringLiteral("状态合并关系与划分过程"));
    m_lexCodePanel = new LexCodePanel(this);
    m_lexTestPanel = new LexTestPanel(this);
    m_grammarPanel = new GrammarEditorPanel(this);
    m_simplifyPanel = new GrammarSimplifyPanel(this);
    m_firstFollowPanel = new FirstFollowPanel(this);
    m_lrItemsPanel = new LrItemsPanel(this);
    // ⑪ 必须用**独立实例**：Qt 的 addTab() 对已在页签栏里的控件是"移动"而非
    // "新增第二个页签"，共用一个面板会让⑩ LR(0) DFA 的页签直接消失。
    m_lrItemsPanel2 = new LrItemsPanel(this);
    m_parseTablePanel = new ParseTablePanel(this);
    m_syntaxProcessPanel = new SyntaxProcessPanel(this);
    m_syntaxTreePanel = new SyntaxTreePanel(this);
    m_icPanel = new IntermediateCodePanel(this);

    connect(m_wordsPanel, &WordsEditorPanel::parseRequested, this,
            &MainWindow::onParseWordsRequested);
    connect(m_wordsPanel, &WordsEditorPanel::fullPipelineRequested, this,
            &MainWindow::onFullLexPipelineRequested);
    connect(m_nfaPanel, &NfaPanel::buildRequested, this, &MainWindow::onBuildNfaRequested);
    connect(m_dfaPanel, &DfaPanel::actionRequested, this, &MainWindow::onBuildDfaRequested);
    connect(m_minDfaPanel, &DfaPanel::actionRequested, this,
            &MainWindow::onMinimizeDfaRequested);
    connect(m_lexCodePanel, &LexCodePanel::regenerateRequested, this,
            &MainWindow::onLexRegenerateRequested);
    connect(m_lexTestPanel, &LexTestPanel::runRequested, this,
            &MainWindow::onLexTestRequested);
    connect(m_lexTestPanel, &LexTestPanel::browseRequested, this,
            &MainWindow::onBrowseSourceRequested);
    connect(m_grammarPanel, &GrammarEditorPanel::loadAndSimplifyRequested, this,
            &MainWindow::onLoadAndSimplifyRequested);
    connect(m_grammarPanel, &GrammarEditorPanel::fullPipelineRequested, this,
            &MainWindow::onFullSyntaxPipelineRequested);
    connect(m_grammarPanel, &GrammarEditorPanel::languageChanged, this,
            [this](LanguageId id) {
                if (m_language != id)
                    m_languageCombo->setCurrentIndex(static_cast<int>(id));
            });
    // 页签⑬：用户点「开始语法分析」→ 主窗口统一跑"读 .lex → 分析 → 建树"
    connect(m_syntaxProcessPanel, &SyntaxProcessPanel::analyzeRequested, this,
            [this](const QString &lexPath) { runSyntaxAnalysis(lexPath); });
    // 页签⑭：导出语法树（三种文本形式一起写盘）
    connect(m_syntaxTreePanel, &SyntaxTreePanel::exportRequested, this,
            &MainWindow::onExportSyntaxTreeRequested);
    // 页签⑮：导出四元式
    connect(m_icPanel, &IntermediateCodePanel::exportRequested, this,
            &MainWindow::onExportIntermediateCodeRequested);

    m_tabWidget = new QTabWidget(this);
    m_tabWidget->setDocumentMode(true);
    m_tabWidget->addTab(m_wordsPanel, QStringLiteral("① 单词规格编辑器"));

    const QVector<TabSpec> specs = buildTabSpecs();
    for (int i = 0; i < specs.size(); ++i) {
        const TabSpec &spec = specs.at(i);
        // 15 个页签全部对应真实面板，一一映射见下方 if 链
        QWidget *panel = nullptr;
        if (spec.title.contains(QStringLiteral("② NFA")))
            panel = m_nfaPanel;
        else if (spec.title.contains(QStringLiteral("③ DFA")))
            panel = m_dfaPanel;
        else if (spec.title.contains(QStringLiteral("④ 最小化DFA")))
            panel = m_minDfaPanel;
        else if (spec.title.contains(QStringLiteral("⑤ 词法分析源程序")))
            panel = m_lexCodePanel;
        else if (spec.title.contains(QStringLiteral("⑥ 编译与词法测试")))
            panel = m_lexTestPanel;
        else if (spec.title.contains(QStringLiteral("⑦ 文法编辑器")))
            panel = m_grammarPanel;
        else if (spec.title.contains(QStringLiteral("⑧ 文法化简结果")))
            panel = m_simplifyPanel;
        else if (spec.title.contains(QStringLiteral("⑨ First/Follow")))
            panel = m_firstFollowPanel;
        else if (spec.title.contains(QStringLiteral("⑩ LR(0) DFA")))
            panel = m_lrItemsPanel;
        else if (spec.title.contains(QStringLiteral("⑪ LALR(1) 项目集")))
            panel = m_lrItemsPanel2;   // 独立实例；共用一个控件会让⑩的页签被"搬走"
        else if (spec.title.contains(QStringLiteral("⑫ LALR(1) 分析表")))
            panel = m_parseTablePanel;
        else if (spec.title.contains(QStringLiteral("⑬ 语法分析过程")))
            panel = m_syntaxProcessPanel;
        else if (spec.title.contains(QStringLiteral("⑭ 语法树")))
            panel = m_syntaxTreePanel;
        else if (spec.title.contains(QStringLiteral("⑮ 中间代码")))
            panel = m_icPanel;
        else
            continue;   // 理论上不可达（15 个页签都有真实面板）；跳过而不是造占位面板
        panel->setParent(this);
        m_tabWidget->addTab(panel, spec.title);
    }
    // ⑦ 文法编辑器已经作为 specs 的一项在正确位置加入，不要再单独 addTab ——
    // 那样会让它掉到页签栏最末尾（本项目实际踩过这个坑）。

    // 左侧功能树：与页签一一对应，点击即跳转
    m_functionTree = new QTreeWidget(this);
    m_functionTree->setHeaderLabel(QStringLiteral("功能导航"));
    m_functionTree->setMinimumWidth(180);

    auto *task1 = new QTreeWidgetItem(m_functionTree, QStringList{QStringLiteral("第一项任务：词法分析生成器")});
    auto *task2 = new QTreeWidgetItem(m_functionTree, QStringList{QStringLiteral("第二项任务：LALR(1) 语法分析生成器")});
    task1->setExpanded(true);
    task2->setExpanded(true);

    // 页签归属：靠标题开头的**带圈数字**判断，①~⑥ 属第一项任务（词法），
    // ⑦~⑮ 属第二项任务（语法）。
    //
    // 为什么不能用"标题里包含某个词"这种子串判断：⑩ 的标题是
    // 「⑩ LR(0) DFA」，它也含 "DFA"，会被 ③ 的规则一起命中，
    // 于是⑩被错分到第一项任务里（本项目实际踩过）。
    // 带圈数字与页签顺序一一对应，是最稳的判据。
    // 注意 lambda **不捕获** kCircled：它有静态存储期，无需捕获即可访问；
    // 若写成 `[&kCircled]`，GCC 会报 "capture of variable with non-automatic
    // storage duration"，而本项目要求 0 warning。
    const auto taskOf = [](const QString &tabText) {
        static const QString kCircled = QStringLiteral("①②③④⑤⑥⑦⑧⑨⑩⑪⑫⑬⑭⑮");
        for (int k = 0; k < kCircled.size(); ++k) {
            if (tabText.startsWith(kCircled.at(k)))
                return k + 1;   // ①②…⑮ 在 UTF-16 里都是单个码元，可按下标计数
        }
        return 0;   // 认不出圈号时按"第二项任务"处理，宁可多归语法侧
    };

    for (int i = 0; i < m_tabWidget->count(); ++i) {
        const QString tabText = m_tabWidget->tabText(i);
        const int num = taskOf(tabText);
        QTreeWidgetItem *parent = (num >= 1 && num <= 6) ? task1 : task2;
        auto *item = new QTreeWidgetItem(parent, QStringList{tabText});
        item->setData(0, Qt::UserRole, i);
    }
    // ⑦ 文法编辑器属于任务二
    m_functionTree->addTopLevelItem(task1);
    m_functionTree->addTopLevelItem(task2);

    connect(m_functionTree, &QTreeWidget::itemClicked, this,
            [this](QTreeWidgetItem *item, int) {
                const QVariant index = item->data(0, Qt::UserRole);
                if (index.isValid())
                    m_tabWidget->setCurrentIndex(index.toInt());
            });

    auto *splitter = new QSplitter(Qt::Horizontal, this);
    splitter->addWidget(m_functionTree);
    splitter->addWidget(m_tabWidget);
    splitter->setStretchFactor(0, 0);
    splitter->setStretchFactor(1, 1);
    splitter->setSizes({210, 1000});
    setCentralWidget(splitter);
}

/**
 * 功能：建立日志停靠窗口。
 * 输入：无。
 * 输出：无。
 * 注意：日志视图用等宽字体并限制最大块数，避免长时间运行占用过多内存。
 */
void MainWindow::buildLogDock()
{
    m_logDock = new QDockWidget(QStringLiteral("⑯ 运行日志"), this);
    m_logDock->setObjectName(QStringLiteral("logDock"));

    m_logView = new QPlainTextEdit(m_logDock);
    m_logView->setReadOnly(true);
    m_logView->setFont(QFontDatabase::systemFont(QFontDatabase::FixedFont));
    m_logView->setMaximumBlockCount(Logger::kMaxHistory);
    m_logView->setPlaceholderText(QStringLiteral("系统运行日志将在此显示。"));

    m_logDock->setWidget(m_logView);
    addDockWidget(Qt::BottomDockWidgetArea, m_logDock);

    // 视图菜单里加入日志窗口的显示/隐藏开关
    for (QMenu *menu : menuBar()->findChildren<QMenu *>()) {
        if (menu->title().startsWith(QStringLiteral("视图"))) {
            menu->addAction(m_logDock->toggleViewAction());
            break;
        }
    }
}

// ---------------------------------------------------------------------------
// 槽函数
// ---------------------------------------------------------------------------

/**
 * 功能：语言切换。
 * 输入：index —— 下拉框下标（等于 LanguageId 的枚举值）。
 * 输出：无。
 * 注意：会重新载入①单词规格与⑦文法，并把结果写入日志。
 */
void MainWindow::onLanguageChanged(int index)
{
    const QVector<LanguageConfig> &configs = LanguageConfigRegistry::all();
    if (index < 0 || index >= configs.size())
        return;

    m_language = configs.at(index).id;
    LOG_INFO(QStringLiteral("切换测试语言为：%1").arg(configs.at(index).displayName));

    m_wordsPanel->loadLanguageWords(m_language);
    m_grammarPanel->loadLanguageGrammar(m_language, m_grammarPanel->useFixedGrammar());
    updateLanguageIndicators();
}

/**
 * 功能：解析"① 单词规格编辑器"中的文本，并把结果填入页签①的规格表。
 * 输入：无（读取 m_wordsPanel->text()）。
 * 输出：解析无错误为 true；有错误时仍显示已解析出的部分结果。
 * 注意：
 *  1. 结果缓存到 m_tokenSpec 与 m_tokenSpecValid，供页签②构造 NFA 复用，
 *     避免重复解析；
 *  2. 解析错误逐条写入日志，且把错误行号一并显示，便于用户定位。
 */
bool MainWindow::parseAndShowTokenSpec()
{
    const QString text = m_wordsPanel->text();
    if (text.trimmed().isEmpty()) {
        LOG_ERROR(QStringLiteral("单词规格编辑器内容为空，无契约可解析。"));
        m_tokenSpecValid = false;
        return false;
    }

    m_tokenSpec = TokenSpec();
    const bool ok = m_tokenSpec.parse(text);
    m_tokenSpecValid = ok;

    // 填充页签①的规格表
    QStringList headers;
    QList<QStringList> rows;
    m_tokenSpec.buildSummaryTable(headers, rows);

    QString summary;
    if (ok) {
        summary = QStringLiteral("解析成功：共 %1 个定义，其中宏定义 %2 个、"
                                 "关键字定义 %3 个、需要构造自动机 %4 个。")
                      .arg(m_tokenSpec.defs().size())
                      .arg(m_tokenSpec.macros().size())
                      .arg(m_tokenSpec.keywordDefs().size())
                      .arg(m_tokenSpec.automatonDefs().size());
    } else {
        summary = QStringLiteral("解析存在 %1 处错误（已显示成功解析的部分）：%2")
                      .arg(m_tokenSpec.errors().size())
                      .arg(m_tokenSpec.errors().join(QStringLiteral("；")));
        for (const QString &err : m_tokenSpec.errors())
            LOG_ERROR(QStringLiteral("单词规格解析错误：%1").arg(err));
    }
    m_wordsPanel->showParseResult(headers, rows, summary);

    if (ok)
        LOG_SUCCESS(summary);
    return ok;
}

/** 功能：处理"解析单词规格"请求。输入：无。输出：无。 */
void MainWindow::onParseWordsRequested()
{
    parseAndShowTokenSpec();
}

/**
 * 功能：由单词规格构造 NFA（正则式解析 + 宏展开 + Thompson 构造）。
 * 输入：rules —— 输出参与构造的规则；postfix —— 输出逆波兰式对照文本。
 * 输出：成功为 true。
 * 注意：要求 m_tokenSpecValid 已为真；调用者负责先解析规格。
 */
bool MainWindow::buildNfaFromSpec(QVector<NfaRule> &rules, QString &postfix)
{
    rules.clear();
    postfix.clear();

    const QVector<RegexDef> autoDefs = m_tokenSpec.automatonDefs();
    if (autoDefs.isEmpty()) {
        LOG_ERROR(QStringLiteral("规格中没有需要构造自动机的定义"
                                 "（缺少带编码的普通单词或注释定义）。"));
        return false;
    }

    QStringList postfixLines;
    for (const RegexDef &def : autoDefs) {
        RegexParser parser;
        parser.setMacros(m_tokenSpec.macros());

        NfaRule rule;
        rule.name = def.name;
        rule.code = def.code;
        rule.isComment = (def.kind == RegexKind::Comment);

        if (!parser.parse(def.rawRegex, rule.ast)) {
            LOG_ERROR(QStringLiteral("定义 %1 的正则式解析失败：%2").arg(def.name, parser.error()));
            continue;
        }

        postfixLines.append(
            QStringLiteral("[%1]  编码 %2  类别 %3\n    中缀式  ： %4\n    逆波兰式： %5\n")
                .arg(def.name)
                .arg(def.code)
                .arg(def.kind == RegexKind::Comment ? QStringLiteral("注释（不输出编码）")
                                                    : QStringLiteral("普通单词"))
                .arg(def.rawRegex, rule.ast.toPostfix()));
        rules.append(rule);
    }

    if (rules.isEmpty()) {
        LOG_ERROR(QStringLiteral("没有可用的规则，NFA 构造中止。"));
        return false;
    }

    postfix = QStringLiteral("共 %1 个定义参与自动机构造：\n\n").arg(rules.size())
              + postfixLines.join(QStringLiteral("\n"));
    return true;
}

/**
 * 功能：切换到标题含指定片段的页签。
 * 输入：titleFragment —— 页签标题中的关键片段。
 * 输出：无。
 * 注意：找不到匹配页签时静默返回（不报错），以免影响主流程。
 */
void MainWindow::activateTab(const QString &titleFragment)
{
    for (int i = 0; i < m_tabWidget->count(); ++i) {
        if (m_tabWidget->tabText(i).contains(titleFragment)) {
            m_tabWidget->setCurrentIndex(i);
            return;
        }
    }
}

/**
 * 功能：构造 NFA 并级联刷新页签②③④（NFA → DFA → 最小化 DFA）。
 * 输入：无。
 * 输出：成功为 true。
 * 注意：
 *  1. 若规格尚未解析则先解析一次，保证与编辑框内容同步；
 *  2. 三个自动机结果都会缓存（m_nfa / m_dfa / m_minDfa），
 *     页签③④的按钮可直接复用而不必重新构造；
 *  3. DFA 或最小化失败不会中断 NFA 的显示——各自独立记录日志。
 */
bool MainWindow::buildAndShowAutomata()
{
    if (!m_tokenSpecValid && !parseAndShowTokenSpec()) {
        LOG_ERROR(QStringLiteral("单词规格解析未通过，无法构造自动机。"));
        return false;
    }

    QVector<NfaRule> rules;
    QString postfix;
    if (!buildNfaFromSpec(rules, postfix))
        return false;

    RegexToNfa nfaBuilder;
    if (!nfaBuilder.build(rules, m_nfa)) {
        LOG_ERROR(QStringLiteral("NFA 构造失败：%1").arg(nfaBuilder.error()));
        m_nfaValid = false;
        return false;
    }
    m_nfaValid = true;

    // 页签②：NFA 状态转换表 + 逆波兰式对照
    m_nfaPanel->setNfa(m_nfa, QStringLiteral("由 %1 个定义合并构造（规则之间为“或”关系），"
                                             "统一初态为状态 0；字母表为 %2 个字符类"
                                             "（一个字符集算一个输入符号）")
                                  .arg(rules.size())
                                  .arg(m_nfa.alphabetInfo.classes.size()));
    m_nfaPanel->setPostfixText(postfix);
    LOG_SUCCESS(QStringLiteral("已构造 NFA（%1 个状态）并显示于页签②。").arg(m_nfa.stateCount));

    // 级联：NFA → DFA
    NfaToDfa subset;
    // 子集构造在 TINY 规模下约 9 秒，是整条词法流水线最慢的一步。
    // 这里挂进度回调：借机排空消息队列并刷新状态栏，否则 Windows 在 5 秒后
    // 就会把窗口标题加上「（未响应）」—— 这正是用户反馈的现象。
    // 状态栏的文字与"排空消息队列"都由 setActivity() 一处负责。
    const auto onSubsetProgress = [this](int states) {
        setActivity(QStringLiteral("正在构造 DFA…已生成 %1 个状态").arg(states));
    };
    const bool subsetOk = subset.construct(m_nfa, m_dfa, onSubsetProgress);
    // 收尾：进度提示只代表"此刻正在做的事"，构造结束（无论成败）必须清掉，
    // 否则跑完流水线后状态栏会一直挂着"正在构造 DFA…已生成 N 个状态"。
    setActivity(QString());
    if (subsetOk) {
        m_dfaValid = true;
        m_dfaPanel->setDfa(m_dfa,
                           QStringLiteral("由 NFA（%1 个状态）经子集构造法得到，"
                                          "DFA 状态编号按发现顺序（广度优先）分配；"
                                          "字母表 %2 个字符类")
                               .arg(m_nfa.stateCount)
                               .arg(m_dfa.alphabetInfo.classes.size()));
        LOG_SUCCESS(QStringLiteral("已构造 DFA（%1 个状态）并显示于页签③。")
                        .arg(m_dfa.stateCount));

        // 级联：DFA → 最小化 DFA
        DfaMinimize minimizer;
        if (minimizer.minimize(m_dfa, m_minDfa)) {
            m_minDfaValid = true;

            // 合并关系文本：只列出"合并了 2 个及以上原状态"的组，避免输出过长
            QStringList mergeLines;
            const QVector<QVector<int>> &groups = minimizer.mergedGroups();
            int mergedGroupCount = 0;
            for (int g = 0; g < groups.size(); ++g) {
                if (groups.at(g).size() < 2)
                    continue;
                ++mergedGroupCount;
                QStringList members;
                for (int oldState : groups.at(g))
                    members << QString::number(oldState);
                mergeLines << QStringLiteral("最小化状态 %1  ←  原 DFA 状态 {%2}（%3 个合并为 1）")
                                  .arg(g)
                                  .arg(members.join(QLatin1Char(',')))
                                  .arg(groups.at(g).size());
            }

            QString detail;
            if (mergeLines.isEmpty()) {
                detail = QStringLiteral("没有可合并的状态：原 DFA 已是最小 DFA"
                                        "（%1 个状态全部两两可区分）。\n")
                             .arg(m_dfa.stateCount);
            } else {
                detail = QStringLiteral("共有 %1 组状态被合并：\n%2\n")
                             .arg(mergedGroupCount)
                             .arg(mergeLines.join(QLatin1Char('\n')));
            }
            detail += QStringLiteral("\n—— 划分法细化过程 ——\n")
                      + minimizer.partitionTrace().join(QLatin1Char('\n'));

            m_minDfaPanel->setDfa(
                m_minDfa,
                QStringLiteral("由 %1 个状态的 DFA 经划分法最小化为 %2 个状态"
                               "（不同编码的接受态不会被合并）；字母表 %3 个字符类")
                    .arg(m_dfa.stateCount)
                    .arg(m_minDfa.stateCount)
                    .arg(m_minDfa.alphabetInfo.classes.size()));
            m_minDfaPanel->setDetailText(detail);
            LOG_SUCCESS(QStringLiteral("已最小化 DFA：%1 → %2 个状态，显示于页签④。")
                            .arg(m_dfa.stateCount)
                            .arg(m_minDfa.stateCount));
        } else {
            m_minDfaValid = false;
            LOG_ERROR(QStringLiteral("DFA 最小化失败：%1").arg(minimizer.error()));
        }
    } else {
        m_dfaValid = false;
        LOG_ERROR(QStringLiteral("子集构造失败：%1").arg(subset.error()));
    }

    return true;
}

/**
 * 功能：处理"构造 NFA"请求。
 * 输入：无。
 * 输出：无。
 * 注意：一次点击即完成 NFA → DFA → 最小化 DFA 的完整级联，
 *       并把三个结果分别填入页签②③④，最后切到页签②。
 */
void MainWindow::onBuildNfaRequested()
{
    if (buildAndShowAutomata())
        activateTab(QStringLiteral("② NFA"));
}

/**
 * 功能：处理页签③的"由 NFA 构造 DFA"请求。
 * 输入：无。
 * 输出：无。
 * 注意：若已有可用的 DFA 则直接复用，否则重新跑一遍级联。
 */
void MainWindow::onBuildDfaRequested()
{
    if (!m_dfaValid) {
        LOG_INFO(QStringLiteral("DFA 尚未构造，先执行完整级联（规格 → NFA → DFA）。"));
        buildAndShowAutomata();
    }
    activateTab(QStringLiteral("③ DFA"));
}

/**
 * 功能：处理页签④的"最小化 DFA"请求。
 * 输入：无。
 * 输出：无。
 * 注意：若已有可用的最小化结果则直接复用，否则重新跑一遍级联。
 */
void MainWindow::onMinimizeDfaRequested()
{
    if (!m_minDfaValid) {
        LOG_INFO(QStringLiteral("最小化 DFA 尚未构造，先执行完整级联"));
        buildAndShowAutomata();
    }
    activateTab(QStringLiteral("④ 最小化DFA"));
}

/** 功能：处理"完整词法流水线"请求（阶段 3 实现）。输入：无。输出：无。 */
void MainWindow::onFullLexPipelineRequested()
{
    runLexPipeline(true);
    activateTab(QStringLiteral("⑥ 编译与词法测试"));
}

/**
 * 功能：执行一次完整词法流水线，并把结果分发到页签③~⑥。
 * 输入：runBuild —— 是否执行"g++ 编译 + 运行"。
 * 输出：生成源码成功为 true。
 * 注意：
 *  1. 规格文本取自页签①的编辑器，因此用户在界面上改完正则式后点按钮即生效；
 *  2. 页签⑤要求"能切换两种转换方法"，所以方法与排版从页签⑤的下拉框读取；
 *  3. 结果同时缓存到 m_lastLexCode / m_lastLexBuild，
 *     页签⑥在只重跑编译时可以直接复用上一次生成的源码。
 */
bool MainWindow::runLexPipeline(bool runBuild)
{
    const QString wordsText = m_wordsPanel->text();
    if (wordsText.trimmed().isEmpty()) {
        LOG_WARN(QStringLiteral("单词规格为空，无法执行词法流水线。"));
        appendLogLine(QStringLiteral("单词规格为空，请先在页签①载入或填写规格文件。"));
        return false;
    }

    const LanguageConfig &config = LanguageConfigRegistry::get(m_language);
    const QString programName = config.displayName;
    const QString identifier = config.key;
    const QString outputDir = FileUtil::outputDir() + QStringLiteral("/lexbuild");

    // 待测源程序：页签⑥里填写的优先，为空则回落到该语言的随附样例
    QString sourceFile = m_lexTestPanel->sourceFile();
    if (sourceFile.isEmpty())
        sourceFile = defaultSourceFile();
    if (sourceFile.isEmpty()) {
        LOG_WARN(QStringLiteral("找不到 %1 的随附源程序，只生成源程序、不执行编译。")
                     .arg(programName));
        runBuild = false;
    }

    LexPipeline pipeline;
    pipeline.setMethod(m_lexCodePanel->method());
    pipeline.setLayout(m_lexCodePanel->layout());
    // 把"事件泵"交给编译/运行模块：等待 g++ 与生成的词法程序期间，
    // 每 100ms 让消息队列走一次，窗口就不会被 Windows 判成「未响应」。
    pipeline.setEventPump([] {
        QCoreApplication::processEvents(QEventLoop::ExcludeUserInputEvents);
    });

    LOG_INFO(QStringLiteral("开始执行词法流水线（%1，%2，%3）...")
                 .arg(programName, m_lexCodePanel->method() == LexCodeGen::Method::JudgeChar
                                       ? QStringLiteral("方法一")
                                       : QStringLiteral("方法二"),
                      runBuild ? QStringLiteral("含编译运行") : QStringLiteral("仅生成源码")));

    const LexPipeline::Result result =
        pipeline.run(wordsText, programName, runBuild ? sourceFile : QString(), outputDir,
                     identifier, runBuild, m_lexTestPanel->compilerPath());

    // ---- 分发到页签③④（自动机类结果）----
    if (result.dfaOk) {
        m_dfa = result.dfa;
        m_dfaValid = true;
        m_dfaPanel->setDfa(result.dfa,
                           QStringLiteral("子集构造法得到的 DFA（由单词规格直接生成）；"
                                          "字母表 %1 个字符类")
                               .arg(result.dfa.alphabetInfo.classes.size()));
    }
    if (result.minOk) {
        m_minDfa = result.minDfa;
        m_minDfaValid = true;
        m_minDfaPanel->setDfa(result.minDfa,
                              QStringLiteral("划分法最小化后的 DFA；字母表 %1 个字符类")
                                  .arg(result.minDfa.alphabetInfo.classes.size()));

        // 合并关系与划分过程：把结构化的合并表拼成可读文本
        QString detail = QStringLiteral("%1 个新状态 ← 原状态集合：\n")
                             .arg(result.minimizeDetail.mergedGroups().size());
        for (int i = 0; i < result.minimizeDetail.mergedGroups().size(); ++i) {
            QStringList members;
            for (int s : result.minimizeDetail.mergedGroups().at(i))
                members << QStringLiteral("S%1").arg(s);
            detail += QStringLiteral("  S%1 ← {%2}\n").arg(i).arg(members.join(QStringLiteral(",")));
        }
        detail += QStringLiteral("\n---- 划分轨迹 ----\n\n")
                  + result.minimizeDetail.partitionTrace().join(QLatin1Char('\n'));
        m_minDfaPanel->setDetailText(detail);
    }
    if (result.nfaOk) {
        m_nfa = result.nfa;
        m_nfaValid = true;
        // 页签② 也必须一起刷新。
        // 历史缺陷：这里只更新了 m_nfa 与 ③④⑤⑥，唯独没刷 ②，于是"跑完整词法
        // 流水线"之后 ② 里还是上一次（甚至另一种语言）构造的 NFA —— 切到 mini-C
        // 再回头看 ②，看到的是 TINY 的自动机，对比时会得出完全错误的结论。
        // 由自测 G29.4（双圈节点数 == NFA 接受态数）暴露出来：数据是 mini-C 的
        // 29 个接受态，面板上却是 TINY 的 21 个。
        m_nfaPanel->setNfa(m_nfa,
                           QStringLiteral("由 %1 条规则合并构造（规则之间为“或”关系），"
                                          "统一初态为状态 0；字母表为 %2 个字符类"
                                          "（一个字符集算一个输入符号）")
                               .arg(result.rules.size())
                               .arg(m_nfa.alphabetInfo.classes.size()));
        m_nfaPanel->setPostfixText(result.postfixText);
    }
    m_tokenSpec = result.spec;
    m_tokenSpecValid = result.specOk;

    // ---- 分发到页签⑤（生成的源程序）----
    if (result.codeOk) {
        m_lastLexCode = result.generatedCode;
        m_lexCodePanel->setSource(
            result.generatedCode,
            QStringLiteral("%1；共 %2 行，%3 字节。")
                .arg(result.summary())
                .arg(result.generatedCode.count(QLatin1Char('\n')))
                .arg(result.generatedCode.size()));
    } else {
        m_lexCodePanel->clearAll();
        m_lexCodePanel->setSource(QString(), QStringLiteral("生成失败：%1").arg(result.error));
    }

    // ---- 分发到页签⑥（编译与词法测试）----
    if (runBuild) {
        m_lastLexBuild = result.build;
        m_lexTestPanel->setResult(result.build, result.build.summary());
    }

    // ---- 关键字可产生性等非致命告警：界面日志与调试日志各报一次 ----
    for (const QString &warning : result.warnings) {
        LOG_WARN(warning);
        appendLogLine(QStringLiteral("⚠ %1").arg(warning));
    }

    // ---- 日志 ----
    if (result.codeOk) {
        LOG_SUCCESS(QStringLiteral("词法流水线完成：%1").arg(result.summary()));
    } else {
        LOG_ERROR(QStringLiteral("词法流水线失败于【%1】：%2")
                      .arg(result.failedStage.isEmpty() ? QStringLiteral("未知阶段")
                                                        : result.failedStage,
                           result.error));
    }
    appendLogLine(result.codeOk ? QStringLiteral("✔ %1").arg(result.summary())
                                : QStringLiteral("✘ %1 阶段失败：%2")
                                      .arg(result.failedStage, result.error));
    return result.codeOk;
}

/**
 * 功能：处理页签⑤的"重新生成"请求。
 * 输入：无。
 * 输出：无。
 * 注意：只重新生成源码（不编译），保持页签⑤操作轻快；
 *       若要连编译一起跑，用户到页签⑥点"编译并测试"。
 */
void MainWindow::onLexRegenerateRequested()
{
    runLexPipeline(false);
}

/**
 * 功能：处理页签⑥的"编译并测试"请求。
 * 输入：无。
 * 输出：无。
 * 注意：若页签⑤还没生成过源码，这里会先走一次完整流水线（含生成）。
 */
void MainWindow::onLexTestRequested()
{
    runLexPipeline(true);
    activateTab(QStringLiteral("⑥ 编译与词法测试"));
}

/** 功能：处理页签⑥的"浏览源程序"请求。输入：无。输出：无。 */
void MainWindow::onBrowseSourceRequested()
{
    const QString current = m_lexTestPanel->sourceFile().isEmpty()
                                ? defaultSourceFile()
                                : m_lexTestPanel->sourceFile();
    const QString path = QFileDialog::getOpenFileName(
        this, QStringLiteral("选择待词法分析的源程序"), current,
        QStringLiteral("源程序 (*.tny *.mc *.c *.txt);;所有文件 (*)"));
    if (!path.isEmpty())
        m_lexTestPanel->setSourceFile(path);
}

/**
 * 功能：取当前语言的随附源程序路径。
 * 输入：无。
 * 输出：文件路径；不存在时返回空串。
 * 注意：按语言取 LanguageConfig 里登记的样例文件名（TINY → sample.tny），
 *       定位不到时回落到 dataDir() 下的同名文件。
 */
QString MainWindow::defaultSourceFile() const
{
    const LanguageConfig &config = LanguageConfigRegistry::get(m_language);
    const QString name = config.sampleFile;
    if (name.isEmpty())
        return QString();
    const QString located = FileUtil::locateDataFile(name);
    if (!located.isEmpty())
        return located;
    const QString fallback = FileUtil::dataDir() + QLatin1Char('/') + name;
    return QFileInfo::exists(fallback) ? fallback : QString();
}

/**
 * 功能：【自测用】程序化执行一次完整词法流水线。
 * 输入：runBuild —— 是否执行 g++ 编译与运行；outSummary —— 输出摘要。
 * 输出：源程序生成成功为 true。
 * 注意：
 *  1. 先确保页签①里已载入规格（若为空则主动载入当前语言的随附数据），
 *     否则自测会因为"编辑器为空"而失败，与真实按钮行为不符；
 *  2. 待测源程序直接填进页签⑥，与用户手填路径等价。
 */
bool MainWindow::runStage3UiCheck(bool runBuild, QString &outSummary)
{
    if (m_wordsPanel->text().trimmed().isEmpty()) {
        const LanguageConfig &config = LanguageConfigRegistry::get(m_language);
        const QString path = FileUtil::locateDataFile(config.wordsFile);
        bool ok = false;
        const QString text = FileUtil::readText(path, &ok);
        if (ok)
            m_wordsPanel->setText(text);
    }
    if (m_lexTestPanel->sourceFile().isEmpty())
        m_lexTestPanel->setSourceFile(defaultSourceFile());

    const bool codeOk = runLexPipeline(runBuild);

    outSummary = QStringLiteral("页签⑤源码 %1 行；页签⑥ %2；页签⑤提示：%3")
                     .arg(lexCodeText().count(QLatin1Char('\n')))
                     .arg(m_lastLexBuild.summary())
                     .arg(m_lexCodePanel == nullptr ? QString() : m_lexCodePanel->summaryText());
    if (codeOk && runBuild)
        outSummary += QStringLiteral("；.lex %1 行").arg(lexTokenText().count(QLatin1Char('\n')));
    return codeOk;
}

/** 功能：【自测用】取页签⑤当前显示的源程序文本。输入：无。输出：文本。 */
QString MainWindow::lexCodeText() const
{
    return m_lexCodePanel->source();
}

/** 功能：【自测用】取页签⑥当前显示的单词编码文件内容。输入：无。输出：文本。 */
QString MainWindow::lexTokenText() const
{
    return m_lastLexBuild.lexContent;
}

/** 功能：【自测用】取页签⑥当前的 .lex 文件路径。输入：无。输出：路径。 */
QString MainWindow::lexTokenPath() const
{
    return m_lastLexBuild.lexPath;
}

/** 功能：【自测用】取全部功能页签标题。输入：无。输出：标题列表（按界面顺序）。 */
QStringList MainWindow::tabTitles() const
{
    QStringList titles;
    if (m_tabWidget == nullptr)
        return titles;
    for (int i = 0; i < m_tabWidget->count(); ++i)
        titles << m_tabWidget->tabText(i);
    return titles;
}

/** 功能：【自测用】取自动机 DOT 文本。输入：minimal。输出：DOT 文本。 */
QString MainWindow::automatonDotText(bool minimal) const
{
    const DfaPanel *panel = minimal ? m_minDfaPanel : m_dfaPanel;
    return panel == nullptr ? QString() : panel->dotText();
}

/**
 * 功能：【自测用】取页签②的 NFA 图 DOT 文本。
 * 输入：无。
 * 输出：DOT 文本；NFA 面板为空或尚未构造时为空串。
 */
QString MainWindow::nfaDotText() const
{
    return m_nfaPanel == nullptr ? QString() : m_nfaPanel->dotText();
}

/** 功能：【自测用】当前 NFA 的接受态个数。输入：无。输出：接受态数。 */
int MainWindow::nfaAcceptingStateCount() const
{
    return m_nfaValid ? m_nfa.accepts.size() : 0;
}

/** 功能：【自测用】当前 NFA 的初态编号。输入：无。输出：初态编号。 */
int MainWindow::nfaStartState() const
{
    return m_nfaValid ? m_nfa.start : 0;
}

/**
 * 功能：【自测用】取页签③/④状态转换表的列数。
 * 输入：minimal —— true 取页签④（最小化 DFA），false 取页签③。
 * 输出：列数（含 状态/说明 两列）；面板为空时为 0。
 */
int MainWindow::automatonColumnCount(bool minimal) const
{
    const DfaPanel *panel = minimal ? m_minDfaPanel : m_dfaPanel;
    return panel == nullptr ? 0 : panel->columnCount();
}

/** 功能：【自测用】取页签③的字符类对照图例。输入：无。输出：图例文本。 */
QString MainWindow::charClassLegendText() const
{
    return m_dfaPanel == nullptr ? QString() : m_dfaPanel->charClassLegend();
}

/** 功能：【自测用】取当前 DFA 的字母表大小。输入：无。输出：符号数。 */
int MainWindow::automatonAlphabetSize() const
{
    return m_dfaValid ? m_dfa.alphabetInfo.classes.size() : 0;
}

/** 功能：【自测用】取页签⑭语法树的 DOT 文本。输入：无。输出：DOT 文本。 */
QString MainWindow::syntaxTreeDotText() const
{
    if (m_parseResult.tree.root < 0)
        return QString();
    return SyntaxTree::toDot(m_parseResult.tree, QStringLiteral("syntax tree"));
}

/** 功能：处理"载入并化简文法"请求（阶段 4 实现）。
 *  输入：无。输出：无。
 *  注意：本槽是页签⑧⑨⑩⑪⑫的统一驱动入口 —— 一次点击把"化简 → First/Follow
 *        → LR(0) 项目集 → LALR(1) 项目集 → 分析表"整条链算完并分发到各页签，
 *        避免每个页签各自重复计算（文法规模大时重复构造代价明显）。
 */
void MainWindow::onLoadAndSimplifyRequested()
{
    runSyntaxFrontend(true);
}

/** 功能：处理"完整语法流水线"请求（阶段 6 实现）。
 *  输入：无。输出：无。
 *  注意：这里是"一键跑完整条链"的入口 —— 文法前端（化简 → First/Follow →
 *        LR(0) → LALR(1) → 分析表）之后再接语法分析（读 `.lex` → 分析 → 建树）。
 *        两条子链各自都有幂等的"已跑过就复用"判断，因此重复点击不会重复计算。
 */
void MainWindow::onFullSyntaxPipelineRequested()
{
    if (!runSyntaxFrontend(true)) {
        appendLogLine(QStringLiteral("[语法流水线] 文法前端失败，已中止。"));
        return;
    }
    runSyntaxAnalysis(QString());
}

/**
 * 功能：执行一次"文法前端"流水线（阶段 4+5），并把结果分发到页签⑦~⑫。
 * 输入：runParseTable —— 是否连分析表一起构造（阶段 5 尚未接界面时可传 false）。
 * 输出：整条链成功为 true。
 * 注意：
 *  1. **算法全部在本函数内完成**，面板只接收算好的结果 —— 这是 AGENT.md §7
 *     的界面分层约定：`syntax/` 不依赖 QtWidgets，面板不写算法；
 *  2. 化简必须在增广**之前**做（`ParseTableBuilder` 内部也是这个顺序）：
 *     增广产生的 S' 从开始符号不可达，若先增广会被化简误删；
 *  3. 每一步都写入日志，便于在实验报告与答疑时说明中间结果。
 */
bool MainWindow::runSyntaxFrontend(bool runParseTable)
{
    const LanguageId id = m_language;
    const QString grammarPath =
        FileUtil::locateDataFile(LanguageConfigRegistry::get(id).grammarFile);

    if (grammarPath.isEmpty()) {
        appendLogLine(QStringLiteral("[文法] 找不到 %1 对应的文法数据文件，已中止。")
                          .arg(LanguageConfigRegistry::get(id).displayName));
        return false;
    }

    Grammar grammar;
    QString error;
    if (!grammar.loadFromFile(grammarPath, error)) {
        appendLogLine(QStringLiteral("[文法] 载入失败：%1").arg(error));
        return false;
    }
    grammar.setTerminalNames(LanguageConfigRegistry::terminalNames(id));

    appendLogLine(QStringLiteral("[文法] 已载入 %1：%2").arg(id == LanguageId::Tiny
                                                                 ? QStringLiteral("TINY")
                                                                 : QStringLiteral("mini-C"),
                                                             grammar.summary()));

    // ---- 第 1 步：化简（务必在增广之前）----
    GrammarSimplify simplifier;
    const SimplifyResult simplified = simplifier.simplify(grammar);
    appendLogLine(QStringLiteral("[文法] 化简：%1").arg(simplified.summary()));

    // ---- 第 2 步：增广 + First/Follow ----
    Grammar augmented = simplified.kept;
    augmented.setTerminalNames(grammar.terminalNames());
    augmented.augment();

    FirstFollow firstFollow;
    const FirstFollowResult followResult = firstFollow.compute(augmented);
    appendLogLine(QStringLiteral("[文法] %1").arg(followResult.summary()));

    // ---- 缓存结果供自测与后续阶段使用 ----
    m_syntaxGrammar = grammar;
    m_simplifyResult = simplified;
    m_augmentedGrammar = augmented;
    m_syntaxValid = true;

    // ---- 分发到页签⑧⑨⑩⑪ ----
    if (m_simplifyPanel != nullptr)
        m_simplifyPanel->showSimplifyResult(grammar, simplified);
    if (m_firstFollowPanel != nullptr)
        m_firstFollowPanel->setGrammar(simplified.kept);
    if (m_lrItemsPanel != nullptr) {
        // ⑩ 固定看 LR(0) 项目集规范族
        m_lrItemsPanel->setModeIndex(0);
        m_lrItemsPanel->setGrammar(simplified.kept);
    }
    if (m_lrItemsPanel2 != nullptr) {
        // ⑪ 固定看 LALR(1) 项目集（LR(1) 同心集合并后的结果）
        m_lrItemsPanel2->setModeIndex(1);
        m_lrItemsPanel2->setGrammar(simplified.kept);
    }

    // ---- 第 3 步：分析表（阶段 5）----
    if (runParseTable && m_parseTablePanel != nullptr) {
        m_parseTablePanel->setGrammar(simplified.kept);
        m_parseTable = m_parseTablePanel->table();
        appendLogLine(QStringLiteral("[分析表] %1").arg(m_parseTable.summary()));
        if (!m_parseTable.isClean()) {
            appendLogLine(QStringLiteral("[分析表] 检测到 %1 处冲突，已按默认规则消解"
                                         "（移进优先 / 序号小的产生式优先），详见页签⑫的冲突报告。")
                              .arg(m_parseTable.conflicts.size()));
        }
    }

    appendLogLine(QStringLiteral("[语法前端] 流水线完成：化简后 %1 条产生式，"
                                 "First/Follow 覆盖原始文法的 %2 个非终结符"
                                 "（增广符号 %3 不计入）")
                      .arg(simplified.kept.productionCount())
                      .arg(simplified.kept.nonterminals().size())
                      .arg(augmented.augmentedStart()));

    updateLanguageIndicators();
    return true;
}

/**
 * 功能：切换当前测试语言。
 * 输入：language —— 目标语言。
 * 输出：切换成功为 true。
 * 注意：与用户操作下拉框走完全相同的路径（`onLanguageChanged` 会重新载入
 *       单词规格与文法），并且会把阶段 4~6 的缓存全部作废 —— 否则切到 mini-C
 *       后仍拿着 TINY 的分析表与语法树，后续分析必然错乱。
 */
bool MainWindow::setLanguage(LanguageId language)
{
    const int index = static_cast<int>(language);
    if (m_languageCombo == nullptr || index < 0 || index >= m_languageCombo->count())
        return false;

    if (m_language == language)
        return true;

    m_languageCombo->setCurrentIndex(index);   // 触发 onLanguageChanged
    if (m_language != language) {
        // 下拉框未发信号（例如下标恰好相同）时兜底
        onLanguageChanged(index);
    }

    // 作废跨阶段缓存：这些东西都是"针对某一种语言"的，换了语言就都不成立了
    m_tokenSpecValid = false;
    m_nfaValid = false;
    m_dfaValid = false;
    m_minDfaValid = false;
    m_syntaxValid = false;
    m_parseValid = false;
    m_parseResult = ParseResult();
    m_lastLexFile.clear();
    m_lastLexCode.clear();
    m_lastLexBuild = LexBuild::Result();
    m_parseTable = ParseTable();
    m_semantics.clear();
    m_simplifyResult = SimplifyResult();
    m_augmentedGrammar = Grammar();

    // 关键：页签⑥里还留着**上一种语言**的待测源程序路径。
    // 若不一并换掉，切到 mini-C 后仍会拿 TINY 的 sample.tny 去跑，
    // 结果是"mini-C 的自动机 + TINY 的源程序"，必然大范围词法错误 ——
    // 这是阶段 7 联调时踩到的真实问题。
    if (m_lexTestPanel != nullptr)
        m_lexTestPanel->setSourceFile(defaultSourceFile());
    if (m_lexCodePanel != nullptr)
        m_lexCodePanel->clearAll();
    if (m_syntaxProcessPanel != nullptr)
        m_syntaxProcessPanel->clearAll();
    if (m_syntaxTreePanel != nullptr)
        m_syntaxTreePanel->clearAll();
    if (m_lrItemsPanel2 != nullptr)
        m_lrItemsPanel2->clearAll();

    return m_language == language;
}

/**
 * 功能：执行一次"语法分析 + 建树"流水线（阶段 6），并分发到页签⑬⑭。
 * 输入：lexPath —— 单词编码文件路径；为空时取词法阶段的默认输出位置。
 * 输出：分析成功且树非空为 true。
 *
 * 注意：
 *  1. 这是任务二后半段的总入口，页签⑬的按钮与自测都走这一条路径，
 *     保证"界面上能看到的"与"自测断言的"是同一套逻辑；
 *  2. 若文法前端还没跑过（`m_syntaxValid` 为假），先跑一次
 *     `runSyntaxFrontend(true)` —— 因为分析表和语义动作都依赖它；
 *  3. 算法全部在此完成（读 `.lex`、分析、建树），两个面板只接收结果。
 *     尤其**不能**让页签⑭自己去分析一遍：那样两处一旦不一致，
 *     就会出现"过程显示通过、树却是上一次的"，而且极难排查；
 *  4. 语义动作文件缺失时按"每行默认动作"处理仍可出树，因此这里只把
 *     解析错误写进日志、不视为致命错误（需求允许用户只定义关心的那几条）。
 */
bool MainWindow::runSyntaxAnalysis(const QString &lexPath)
{
    if (m_syntaxProcessPanel == nullptr || m_syntaxTreePanel == nullptr)
        return false;

    // ---- 第 1 步：确保文法前端已跑完 ----
    if (!m_syntaxValid) {
        appendLogLine(QStringLiteral("[语法分析] 文法前端尚未跑过，先自动执行一次。"));
        if (!runSyntaxFrontend(true)) {
            appendLogLine(QStringLiteral("[语法分析] 文法前端失败，已中止。"));
            return false;
        }
    }

    const LanguageId id = m_language;

    // ---- 第 2 步：载入语义动作 ----
    const QString semPath =
        FileUtil::locateDataFile(LanguageConfigRegistry::get(id).semanticsFile);
    QString semErr;
    if (semPath.isEmpty() || !m_semantics.loadFromFile(semPath, semErr)) {
        appendLogLine(QStringLiteral("[语法分析] 语义动作文件载入失败：%1").arg(semErr));
        m_semantics.clear();
    } else if (!m_semantics.errors().isEmpty()) {
        // 非致命：出错的行会被跳过，其余行照常生效
        appendLogLine(QStringLiteral("[语法分析] 语义动作有 %1 处格式问题：%2")
                          .arg(m_semantics.errors().size())
                          .arg(m_semantics.errors().join(QStringLiteral(" / "))));
    }
    m_syntaxTreePanel->showSemantics(m_simplifyResult.kept, m_semantics);

    // ---- 第 3 步：定位 .lex ----
    QString file = lexPath;
    if (file.isEmpty())
        file = m_lastLexFile;
    if (file.isEmpty()) {
        // 词法阶段的默认输出位置：output/lexbuild/<语言>.lex
        const QString name = (id == LanguageId::Tiny) ? QStringLiteral("tiny.lex")
                                                     : QStringLiteral("minic.lex");
        const QString candidate = FileUtil::outputDir() + QStringLiteral("/lexbuild/") + name;
        if (QFileInfo::exists(candidate))
            file = candidate;
    }
    if (file.isEmpty() || !QFileInfo::exists(file)) {
        appendLogLine(QStringLiteral("[语法分析] 找不到单词编码文件，"
                                     "请先在页签⑥完成一次词法分析，或在页签⑬选择文件。"));
        return false;
    }
    m_lastLexFile = file;

    // ---- 第 4 步：读 .lex ----
    LexReader reader;
    Grammar readGrammar = m_simplifyResult.kept;
    readGrammar.setTerminalNames(LanguageConfigRegistry::terminalNames(id));
    readGrammar.augment();
    reader.setGrammar(readGrammar);

    QString lexErr;
    if (!reader.loadFromFile(file, lexErr)) {
        appendLogLine(QStringLiteral("[语法分析] 读取 %1 失败：%2").arg(file, lexErr));
        return false;
    }
    appendLogLine(QStringLiteral("[语法分析] 已读入 %1：%2").arg(file, reader.summary()));

    // ---- 第 5 步：表驱动分析 + 建树 ----
    Parser parser;
    parser.setTable(m_parseTable);
    parser.setSemantics(m_semantics);
    // 语句边界用文法里的符号名（TINY/mini-C 都是 SEMI），不是字面量 `;`
    parser.setStatementBoundary(QStringLiteral("SEMI"));

    m_parseResult = parser.parse(reader.symbols(), reader.symbolLines(), reader.symbolValues());
    m_parseValid = m_parseResult.success && m_parseResult.tree.root >= 0;

    appendLogLine(QStringLiteral("[语法分析] %1").arg(m_parseResult.summary()));
    if (!m_parseResult.success) {
        appendLogLine(QStringLiteral("[语法分析] 共 %1 处语法错误：\n%2")
                          .arg(m_parseResult.errors.size())
                          .arg(m_parseResult.errorsText()));
    }

    // ---- 第 6 步：分发到页签⑬⑭（同一次结果）----
    m_syntaxProcessPanel->setTable(m_parseTable);
    m_syntaxProcessPanel->setSemantics(m_semantics);
    m_syntaxProcessPanel->showParseResult(m_parseResult, file);
    m_syntaxProcessPanel->setLexPath(file);

    m_syntaxTreePanel->showTree(m_parseResult.tree, QFileInfo(file).fileName());

    // ---- 第 7 步：选做 —— 由语法树生成四元式中间代码（阶段 8）----
    m_intermediateCode = IntermediateCode::generate(m_parseResult.tree);
    m_icPanel->showCode(m_intermediateCode, QFileInfo(file).fileName());
    appendLogLine(QStringLiteral("[中间代码] %1").arg(m_intermediateCode.summary()));
    if (!m_intermediateCode.warnings.isEmpty()) {
        appendLogLine(QStringLiteral("[中间代码] 告警：%1")
                          .arg(m_intermediateCode.warnings.join(QStringLiteral(" / "))));
    }

    updateLanguageIndicators();
    return m_parseValid;
}

/**
 * 功能：导出四元式中间代码。
 * 输入：无。输出：无。
 * 注意：写成制表符分隔的文本，Excel / WPS 可直接打开成表格，
 *       比纯文本更适合当作实验报告的附录；同时保留一条"带注释"的可读形式。
 */
void MainWindow::onExportIntermediateCodeRequested()
{
    if (m_intermediateCode.isEmpty()) {
        appendLogLine(QStringLiteral("[中间代码] 还没有可导出的四元式。"));
        return;
    }

    const QString path = QFileDialog::getSaveFileName(
        this, QStringLiteral("导出四元式中间代码"),
        FileUtil::outputDir() + QStringLiteral("/intermediate_code.txt"),
        QStringLiteral("文本文件 (*.txt)"));
    if (path.isEmpty())
        return;

    QStringList lines;
    lines << QStringLiteral("四元式中间代码：%1").arg(m_lastLexFile);
    lines << m_intermediateCode.summary();
    lines << QString();
    lines << QStringLiteral("序号\t运算符\t运算对象1\t运算对象2\t结果\t源程序行\t说明");
    QStringList headers;
    QList<QStringList> rows;
    m_intermediateCode.buildTable(headers, rows);
    for (const QStringList &row : rows)
        lines << row.join(QLatin1Char('\t'));
    lines << QString();
    lines << QStringLiteral("---- 可读形式 ----");
    lines << m_intermediateCode.toText(false);
    if (!m_intermediateCode.warnings.isEmpty()) {
        lines << QString();
        lines << QStringLiteral("---- 生成告警 ----");
        lines << m_intermediateCode.warnings;
    }

    const bool ok = FileUtil::writeText(path, lines.join(QLatin1Char('\n')), false);
    appendLogLine(QStringLiteral("[中间代码] 导出%1：%2")
                      .arg(ok ? QStringLiteral("成功") : QStringLiteral("失败"), path));
}

/**
 * 功能：导出语法树（缩进文本 / 括号表示 / DOT 三种形式一次写盘）。
 * 输入：无。输出：无。
 * 注意：文件名由用户选择，三种形式会分别写成 `<名>.txt`、`<名>.bracket.txt`、
 *       `<名>.dot`，避免互相覆盖 —— 报告里三种形式各有用途
 *       （缩进文本便于阅读、括号表示便于与参考实现比对、DOT 可转成插图）。
 */
void MainWindow::onExportSyntaxTreeRequested()
{
    if (m_parseResult.tree.root < 0) {
        appendLogLine(QStringLiteral("[语法树] 还没有可导出的语法树。"));
        return;
    }

    const QString base = QFileDialog::getSaveFileName(
        this, QStringLiteral("导出语法树"),
        FileUtil::outputDir() + QStringLiteral("/syntax_tree.txt"),
        QStringLiteral("文本文件 (*.txt)"));
    if (base.isEmpty())
        return;

    const QString stem = base.endsWith(QStringLiteral(".txt"))
                             ? base.left(base.size() - 4)
                             : base;
    const SyntaxTreeData &tree = m_parseResult.tree;
    const bool a = FileUtil::writeText(stem + QStringLiteral(".txt"),
                                       SyntaxTree::toText(tree), false);
    const bool b = FileUtil::writeText(stem + QStringLiteral(".bracket.txt"),
                                       SyntaxTree::toBracket(tree), false);
    const bool c = FileUtil::writeText(stem + QStringLiteral(".dot"),
                                       SyntaxTree::toDot(tree, QStringLiteral("语法树")), false);

    appendLogLine(QStringLiteral("[语法树] 导出%1：%2{.txt, .bracket.txt, .dot}")
                      .arg((a && b && c) ? QStringLiteral("成功") : QStringLiteral("部分失败"),
                           stem));
}

/**
 * 功能：【自测用】程序化执行一次文法前端流水线。
 * 输入：outSummary —— 输出摘要。
 * 输出：成功为 true。
 * 注意：走的是与页签⑦按钮完全相同的代码路径（runSyntaxFrontend），
 *       因此能真实反映阶段 4+5 的界面功能是否可用。
 */
bool MainWindow::runStage45UiCheck(QString &outSummary)
{
    const bool ok = runSyntaxFrontend(true);

    outSummary = QStringLiteral("化简 %1 条产生式；First/Follow %2 组；"
                                "项目集 %3 个状态；分析表 %4 个状态、冲突 %5 处")
                     .arg(m_simplifyResult.kept.productionCount())
                     .arg(m_firstFollowPanel == nullptr ? 0 : m_firstFollowPanel->tableRowCount())
                     .arg(m_lrItemsPanel == nullptr ? 0 : m_lrItemsPanel->stateRowCount())
                     .arg(m_parseTable.stateCount())
                     .arg(m_parseTable.conflicts.size());

    return ok;
}

/** 功能：【自测用】页签⑫分析表摘要。输入：无。输出：文本。 */
QString MainWindow::parseTableSummary() const
{
    return m_parseTablePanel == nullptr ? QString() : m_parseTablePanel->summaryText();
}

/** 功能：【自测用】页签⑫冲突报告。输入：无。输出：文本。 */
QString MainWindow::parseTableConflicts() const
{
    return m_parseTablePanel == nullptr ? QString() : m_parseTablePanel->conflictsText();
}

/** 功能：【自测用】页签⑩⑪项目集摘要。输入：无。输出：文本。 */
QString MainWindow::lrItemsSummary() const
{
    return m_lrItemsPanel == nullptr ? QString() : m_lrItemsPanel->summaryText();
}

/** 功能：【自测用】页签⑪状态表行数。输入：无。输出：行数。 */
int MainWindow::lrItemsStateCount() const
{
    return m_lrItemsPanel == nullptr ? 0 : m_lrItemsPanel->stateRowCount();
}

/**
 * 功能：【自测用】程序化执行一次"语法分析 + 建树"流水线（阶段 6）。
 * 输入：outSummary —— 输出摘要。
 * 输出：分析成功且树非空为 true。
 * 注意：走的是与页签⑬「开始语法分析」按钮完全相同的代码路径（runSyntaxAnalysis），
 *       因此能真实反映阶段 6 的界面功能是否可用。
 */
bool MainWindow::runStage6UiCheck(QString &outSummary)
{
    const bool ok = runSyntaxAnalysis(QString());

    outSummary = QStringLiteral("过程 %1 行；%2；树 %3 个结点；语义动作 %4 条")
                     .arg(m_syntaxProcessPanel == nullptr
                              ? 0
                              : m_syntaxProcessPanel->stepRowCount())
                     .arg(m_parseResult.summary())
                     .arg(m_syntaxTreePanel == nullptr ? -1 : m_syntaxTreePanel->treeNodeCount())
                     .arg(m_syntaxTreePanel == nullptr
                              ? 0
                              : m_syntaxTreePanel->semanticsRowCount());
    // 分析失败时把错误明细带出来 —— 否则自测只看到"失败"两个字，
    // 还得回到界面里翻页签⑬才能知道错在哪一行。
    if (!m_parseResult.success && m_syntaxProcessPanel != nullptr)
        outSummary += QStringLiteral("；错误：")
                      + m_syntaxProcessPanel->errorsText().left(400);

    return ok;
}

/** 功能：【自测用】页签⑬分析过程表行数。输入：无。输出：行数。 */
int MainWindow::syntaxStepRowCount() const
{
    return m_syntaxProcessPanel == nullptr ? 0 : m_syntaxProcessPanel->stepRowCount();
}

/** 功能：【自测用】页签⑬摘要文本。输入：无。输出：文本。 */
QString MainWindow::syntaxProcessSummary() const
{
    return m_syntaxProcessPanel == nullptr ? QString() : m_syntaxProcessPanel->summaryText();
}

/** 功能：【自测用】页签⑭结点数。输入：无。输出：结点数（-1 表示无树）。 */
int MainWindow::syntaxTreeNodeCount() const
{
    return m_syntaxTreePanel == nullptr ? -1 : m_syntaxTreePanel->treeNodeCount();
}

/** 功能：【自测用】页签⑭语法树缩进文本。输入：无。输出：文本。 */
QString MainWindow::syntaxTreeText() const
{
    return m_syntaxTreePanel == nullptr ? QString() : m_syntaxTreePanel->treeTextContent();
}

/** 功能：【自测用】页签⑭括号表示。输入：无。输出：文本。 */
QString MainWindow::syntaxTreeBracket() const
{
    return m_syntaxTreePanel == nullptr ? QString() : m_syntaxTreePanel->bracketTextContent();
}

/** 功能：【自测用】页签⑭语义动作表行数。输入：无。输出：行数。 */
int MainWindow::syntaxSemanticsRowCount() const
{
    return m_syntaxTreePanel == nullptr ? 0 : m_syntaxTreePanel->semanticsRowCount();
}

// ---------------------------------------------------------------------------
// 【自测用】页签⑭图形视图的浏览状态（"适应窗口 / 可拖动放大"的守门员）
// ---------------------------------------------------------------------------

/** 功能：【自测】语法树图形视图当前缩放比例。输入：无。输出：比例（1.0=100%）。 */
double MainWindow::syntaxTreeGraphZoom() const
{
    return m_syntaxTreePanel == nullptr ? 1.0 : m_syntaxTreePanel->graphZoom();
}

/** 功能：【自测】当前比例是否正好是"适应窗口"比例。输入：无。输出：布尔。 */
bool MainWindow::syntaxTreeGraphAtFitZoom() const
{
    return m_syntaxTreePanel != nullptr && m_syntaxTreePanel->graphAtFitZoom();
}

/** 功能：【自测】整棵树是否落在视口内。输入：无。输出：布尔。 */
bool MainWindow::syntaxTreeGraphFits() const
{
    return m_syntaxTreePanel != nullptr && m_syntaxTreePanel->graphFitsViewport();
}

/** 功能：【自测】整棵树是否在视口内居中。输入：无。输出：布尔。 */
bool MainWindow::syntaxTreeGraphCentered() const
{
    return m_syntaxTreePanel != nullptr && m_syntaxTreePanel->graphCentered();
}

/** 功能：【自测】是否可拖动浏览。输入：无。输出：布尔。 */
bool MainWindow::syntaxTreeGraphCanPan() const
{
    return m_syntaxTreePanel != nullptr && m_syntaxTreePanel->graphCanPan();
}

/** 功能：【自测】内容尺寸（100% 时，像素）。输入：无。输出：尺寸。 */
QSize MainWindow::syntaxTreeGraphSceneSize() const
{
    return m_syntaxTreePanel == nullptr ? QSize() : m_syntaxTreePanel->graphSceneSize();
}

/** 功能：【自测】把图形视图缩放到指定比例（模拟用户拖滑块）。输入：zoom。输出：无。 */
void MainWindow::setSyntaxTreeGraphZoom(double zoom)
{
    if (m_syntaxTreePanel != nullptr)
        m_syntaxTreePanel->setGraphZoom(zoom);
}

/** 功能：【自测用】页签⑮四元式表行数。输入：无。输出：行数。 */
int MainWindow::intermediateQuadCount() const
{
    return m_icPanel == nullptr ? 0 : m_icPanel->quadRowCount();
}

/** 功能：【自测用】页签⑮某单元格文本。输入：row、column。输出：文本。 */
QString MainWindow::intermediateCellText(int row, int column) const
{
    return m_icPanel == nullptr ? QString() : m_icPanel->quadCellText(row, column);
}

/** 功能：【自测用】页签⑮摘要文本。输入：无。输出：文本。 */
QString MainWindow::intermediateSummary() const
{
    return m_icPanel == nullptr ? QString() : m_icPanel->summaryText();
}

/** 功能：清空日志面板与内存日志。输入：无。输出：无。 */
void MainWindow::onClearLog()
{
    m_logView->clear();
    Logger::instance()->clear();
}

/** 功能：用系统默认程序打开数据目录。输入：无。输出：无。 */
void MainWindow::onOpenDataDir()
{
    FileUtil::ensureDir(FileUtil::dataDir());
    QDesktopServices::openUrl(QUrl::fromLocalFile(QDir::toNativeSeparators(FileUtil::dataDir())));
}

/** 功能：显示"关于本系统"对话框。输入：无。输出：无。 */
void MainWindow::onAbout()
{
    QMessageBox::about(
        this, QStringLiteral("关于本系统"),
        QStringLiteral(
            "<h3>编译原理课程设计系统</h3>"
            "<p>华南师范大学计算机学院 · 编译原理课程设计</p>"
            "<p><b>第一项任务</b>：正则表达式 → NFA → DFA → 最小化 DFA → 词法分析源程序<br>"
            "<b>第二项任务</b>：BNF 文法 → 化简 → First/Follow → LR(0) → LALR(1) 分析表 "
            "→ 语法分析过程 → 语法树</p>"
            "<p><b>测试语言</b>：TINY 语言、mini-C 语言</p>"
            "<p>开发语言：C++17 · 图形界面：Qt %1</p>")
            .arg(QString::fromLatin1(qVersion())));
}

/** 功能：显示"关于 Qt"对话框。输入：无。输出：无。 */
void MainWindow::onAboutQt()
{
    QApplication::aboutQt();
}

// ---------------------------------------------------------------------------
// 内部工具
// ---------------------------------------------------------------------------

/**
 * 功能：更新窗口标题与状态栏的语言相关提示。
 * 输入：无。
 * 输出：无。
 * 注意：状态栏同时显示当前语言的单词规格文件路径，便于确认数据来源；
 *       **不**碰"当前操作"提示（那段由 setActivity() 管，见其说明）。
 */
void MainWindow::updateLanguageIndicators()
{
    const LanguageConfig &cfg = LanguageConfigRegistry::get(m_language);
    setWindowTitle(QStringLiteral("编译原理课程设计系统 — %1").arg(cfg.displayName));
    m_statusLanguage->setText(QStringLiteral("语言：%1").arg(cfg.displayName));
    m_statusDataFile->setText(
        QStringLiteral("数据：%1").arg(FileUtil::toNativePath(FileUtil::locateDataFile(cfg.wordsFile))));
}

/**
 * 功能：更新状态栏的"当前操作"提示，并（非空时）让消息队列走一次。
 * 输入：text —— 提示文本；空串表示回到空闲状态。
 * 输出：无。
 * 注意：
 *  1. 该控件只描述**此刻正在做什么**（目前唯一用户是子集构造的进度回调）。
 *     早期这里显示"当前进度：阶段 0（工程骨架）"，功能全部实现后
 *     那句话就永久错误 —— 界面文案不得用来表达"开发进度"；
 *  2. 非空时排空消息队列，否则 Windows 在 5 秒后会把标题加上「（未响应）」；
 *  3. 一律用 ExcludeUserInputEvents：只处理重绘与定时器，**不处理鼠标键盘事件**。
 *     这一点很关键 —— 若把点击也处理掉，用户可能在上一次构造还没结束时
 *     又点一次按钮，形成重入，同一份 m_dfa 与面板会被两个流程交错改写；
 *  4. 清空时不排空队列（没必要），所以"设进度 / 收尾清空"必须成对出现。
 */
void MainWindow::setActivity(const QString &text)
{
    if (m_statusActivity == nullptr)
        return;
    m_statusActivity->setText(text);
    if (text.isEmpty())
        return;
    QCoreApplication::processEvents(QEventLoop::ExcludeUserInputEvents);
}

/** 功能：【自测用】取状态栏"当前操作"提示文本。输入：无。输出：提示文本（空闲为空串）。 */
QString MainWindow::statusActivityText() const
{
    return m_statusActivity != nullptr ? m_statusActivity->text() : QString();
}

/**
 * 功能：【自测用】取状态栏三段的合并文本。
 * 输入：无。
 * 输出：形如 "语言：TINY | 数据：… | <当前操作>"；当前操作为空时不追加尾部。
 * 注意：断言失败时直接把这段文本打出来，比逐个字段断言更好定位。
 */
QString MainWindow::statusBarSummary() const
{
    QStringList parts;
    if (m_statusLanguage != nullptr)
        parts << m_statusLanguage->text();
    if (m_statusDataFile != nullptr)
        parts << m_statusDataFile->text();
    const QString activity = statusActivityText();
    if (!activity.isEmpty())
        parts << activity;
    return parts.join(QStringLiteral(" | "));
}

/**
 * 功能：把一行日志追加到日志面板末尾。
 * 输入：line —— 已格式化的日志整行。
 * 输出：无。
 * 注意：使用 appendPlainText 而非 setPlainText，以保留历史内容。
 */
void MainWindow::appendLogLine(const QString &line)
{
    if (m_logView)
        m_logView->appendPlainText(line);
}

}  // namespace ccd
