/******************************************************************************
 * (1) 版权信息 : 编译原理课程设计 - 华南师范大学计算机学院
 * (2) 文件名称 : MainWindow.h
 *     标识符   : MainWindow
 *     摘要     : 应用程序主窗口。左侧为功能树（QTreeWidget）用于快速定位功能，
 *                中央为 QTabWidget，每个页签对应任务书的一项功能要求：
 *                  ① 单词规格编辑器   ② NFA   ③ DFA   ④ 最小化DFA
 *                  ⑤ 词法分析源程序   ⑥ 编译与词法测试
 *                  ⑦ 文法编辑器       ⑧ 文法化简结果   ⑨ First/Follow
 *                  ⑩ LR(0) DFA       ⑪ LALR(1) 项目集  ⑫ LALR(1) 分析表
 *                  ⑬ 语法分析过程     ⑭ 语法树          ⑮ 中间代码(选做)
 *                  ⑯ 日志
 *                本文件依赖 QtWidgets；算法逻辑一律不写在此处，只做界面编排与
 *                算法模块的调用粘合。
 * (3) 版本号   : 1.0.0       作者/修改者 : <学号 姓名>   完成日期 : 2026-XX-XX
 * (4) 版本历史 : 1.0.0  2026-XX-XX  建立界面骨架与语言切换、日志面板
 *****************************************************************************/

#pragma once

#include <QMainWindow>

#include "common/LanguageConfig.h"
#include "lex/LexCodeGen.h"
#include "lex/LexPipeline.h"
#include "lex/RegexToNfa.h"
#include "lex/TokenSpec.h"
#include "syntax/Grammar.h"
#include "syntax/GrammarSimplify.h"
#include "syntax/IntermediateCode.h"
#include "syntax/ParseTable.h"
#include "syntax/Parser.h"
#include "syntax/SemanticsAction.h"

class QAction;
class QComboBox;
class QDockWidget;
class QLabel;
class QPlainTextEdit;
class QTabWidget;
class QTreeWidget;
class QTreeWidgetItem;

namespace ccd {

class DfaPanel;
class FirstFollowPanel;
class GrammarEditorPanel;
class GrammarSimplifyPanel;
class LexCodePanel;
class LexTestPanel;
class LrItemsPanel;
class NfaPanel;
class ParseTablePanel;
class SyntaxProcessPanel;
class SyntaxTreePanel;
class IntermediateCodePanel;
class WordsEditorPanel;

/**
 * @brief 主窗口。
 *
 * 调用注意：
 *  1. 构造后需调用 initialize() 完成语言数据载入与日志连接；
 *  2. 语言切换统一走 onLanguageChanged()，它会刷新①单词规格与⑦文法两个编辑器，
 *     并按需求把状态栏与窗口标题一并更新；
 *  3. 15 个功能页签全部是真实面板（①单词规格 … ⑮中间代码），没有占位页；
 *     页签顺序由 `buildTabSpecs()` 决定，断言 `G1b` 守住 ①→⑮ 的次序。
 */
class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit MainWindow(QWidget *parent = nullptr);

    /**
     * @brief 初始化：建立界面、连接日志、载入默认语言数据。
     * @param language 默认语言（默认 TINY，符合任务书"先以 TINY 语言测试"）。
     */
    void initialize(LanguageId language = LanguageId::Tiny);

    /**
     * @brief 切换到指定序号的中央功能页签。
     * @param index 页签序号（从 0 开始）；越界时忽略。
     * @return 实际切换成功为 true。
     * @note 供 --shot 截图与"功能树导航"使用。
     */
    bool selectTab(int index);

    /**
     * @brief 【自测用】程序化执行一次"解析规格 → 构造 NFA"的完整界面流程。
     * @param outSummary 输出：流程结果摘要（供自测报告记录）。
     * @return 成功为 true。
     * @note 本方法走的是与按钮点击完全相同的代码路径（onParseWordsRequested
     *       与 onBuildNfaRequested），因此可以真实反映界面功能是否可用。
     */
    bool runLexicalUiCheck(QString &outSummary);

    /**
     * @brief 【自测用】程序化执行一次"完整词法流水线"（页签①→⑤→⑥）。
     * @param outSummary 输出：流程结果摘要（供自测报告记录）。
     * @return 源程序生成成功为 true。
     * @note 走的是与页签①"执行完整词法流水线"按钮完全相同的代码路径
     *       （runLexPipeline），因此可真实反映阶段 3 的界面功能是否可用。
     *       参数 runBuild 控制是否连"g++ 编译 + 运行"一起执行。
     */
    bool runStage3UiCheck(bool runBuild, QString &outSummary);

    /** @brief 【自测用】取页签⑤当前显示的源程序文本 */
    QString lexCodeText() const;
    /** @brief 【自测用】取页签⑥当前显示的单词编码文件内容 */
    QString lexTokenText() const;
    /** @brief 【自测用】取页签⑥当前的 .lex 文件路径 */
    QString lexTokenPath() const;

    /**
     * @brief 【自测用】程序化执行一次文法前端流水线（阶段 4+5）。
     * @param outSummary 输出：流程结果摘要（供自测报告记录）。
     * @return 成功为 true。
     * @note 走的是与页签⑦"载入并化简文法"按钮完全相同的代码路径
     *       （runSyntaxFrontend），因此可真实反映阶段 4+5 的界面功能是否可用。
     */
    bool runStage45UiCheck(QString &outSummary);

    /** @brief 【自测用】取页签⑫当前显示的分析表摘要 */
    QString parseTableSummary() const;
    /** @brief 【自测用】取页签⑫当前的冲突报告文本 */
    QString parseTableConflicts() const;
    /** @brief 【自测用】取页签⑩⑪当前项目集面板的摘要 */
    QString lrItemsSummary() const;
    /** @brief 【自测用】取页签⑩⑪状态表的行数 */
    int lrItemsStateCount() const;

    /**
     * @brief 【自测用】程序化执行一次"语法分析 + 建树"流水线（阶段 6）。
     * @param outSummary 输出：流程结果摘要（供自测报告记录）。
     * @return 分析成功且树非空为 true。
     * @note 走的是与页签⑬「开始语法分析」按钮完全相同的代码路径
     *       （runSyntaxAnalysis），因此可真实反映阶段 6 的界面功能是否可用。
     */
    bool runStage6UiCheck(QString &outSummary);

    /** @brief 【自测用】取页签⑬分析过程表的行数 */
    int syntaxStepRowCount() const;
    /** @brief 【自测用】取页签⑬的摘要文本 */
    QString syntaxProcessSummary() const;
    /** @brief 【自测用】取页签⑭当前的结点数（-1 表示无树） */
    int syntaxTreeNodeCount() const;
    /** @brief 【自测用】取页签⑭的语法树缩进文本 */
    QString syntaxTreeText() const;
    /** @brief 【自测用】取页签⑭的括号表示 */
    QString syntaxTreeBracket() const;
    /** @brief 【自测用】取页签⑭语义动作表的行数 */
    int syntaxSemanticsRowCount() const;

    /** @brief 【自测用】取页签⑮四元式表的行数 */
    int intermediateQuadCount() const;
    /** @brief 【自测用】取页签⑮某单元格文本 */
    QString intermediateCellText(int row, int column) const;
    /** @brief 【自测用】取页签⑮的摘要文本 */
    QString intermediateSummary() const;
    /** @brief 【自测用】取页签⑮的纯文本形式 */
    QString intermediateText() const { return m_intermediateCode.toText(false); }

    /**
     * @brief 切换当前测试语言（等价于用户在下拉框里选择）。
     * @param language 目标语言。
     * @return 切换成功为 true。
     * @note 供 `--selftest` 在同一次运行里把 TINY 与 mini-C 两条链路都真实跑一遍 ——
     *       mini-C 的函数定义/数组/括号是 TINY 完全覆盖不到的语法，
     *       不实地跑一遍就无法发现"括号编码名写反"这类缺陷。
     */
    bool setLanguage(LanguageId language);

    /** @brief 【自测用】当前语言 */
    LanguageId currentLanguage() const { return m_language; }

    /** @brief 【自测用】页签⑥当前落盘的 .lex 文件路径 */
    QString currentLexPath() const { return m_lastLexFile; }

    /**
     * @brief 【自测用】状态栏的"当前操作"提示文本（空闲时应为空串）。
     * @note 守的是这样一类缺陷：进度提示只设不清，跑完流水线后状态栏一直挂着
     *       "正在构造 DFA…已生成 N 个状态"（子集构造约 9 秒，界面靠它证明没卡死，
     *       但收尾必须清掉）。
     */
    QString statusActivityText() const;

    /**
     * @brief 【自测用】状态栏三段的合并文本："语言：… | 数据：… | <当前操作>"。
     * @note 断言失败时能直接看到实际内容；也用于断言界面里不再出现
     *       "阶段 N（工程骨架）"这类**开发进度**文案。
     */
    QString statusBarSummary() const;

    /**
     * @brief 【自测用】全部功能页签的标题（顺序即界面顺序）。
     * @note 供自测断言"第 i 个页签的标题以带圈数字 i+1 开头"。
     *       这条断言守的是"页签被摆错位置"这类缺陷 —— 本项目就曾把
     *       ⑦文法编辑器单独 addTab 到循环之后，结果它掉到页签栏最末尾，
     *       第 7 个位置显示的是⑧ 文法化简结果，使用者以为⑦没做出来。
     */
    QStringList tabTitles() const;

    /**
     * @brief 【自测用】取页签③/④表格里生成的自动机 DOT 文本。
     * @param minimal true 取最小化 DFA 的（页签④），false 取页签③的。
     * @return DOT 文本；尚未构造自动机时为空串。
     * @note 供自测验证「导出的 .dot 是纯可打印 ASCII、能被 Graphviz 解析」。
     *       这个验证必须自动做：字母表覆盖全部 256 个字节值时，边标签里会出现
     *       控制字节与高位字节；一旦原样写进 .dot，Graphviz 会直接报
     *       `syntax error ... scanning a quoted string` 并拒绝出图。
     */
    QString automatonDotText(bool minimal) const;

    /**
     * @brief 【自测用】取页签②（NFA）的 DOT 文本。
     * @return DOT 文本；尚未构造 NFA 时为空串。
     * @note 这是全项目唯一会含非 ASCII 字符（ε）的 DOT，因此**不能**套用
     *       「纯 ASCII」的断言，只能断言"除 ε 外均为可打印 ASCII，且无控制字符"。
     *       断言要验证的重点是：ε 边真的画出来了、且没有孤立状态。
     */
    QString nfaDotText() const;

    /** @brief 【自测用】当前 NFA 的接受态个数（DOT 里双圈节点的数量应与之相等） */
    int nfaAcceptingStateCount() const;

    /** @brief 【自测用】当前 NFA 的初态编号（DOT 里除它以外的节点都应有入边） */
    int nfaStartState() const;

    /**
     * @brief 【自测用】页签③/④状态转换表的列数（含 状态/说明 两列）。
     * @param minimal true 取页签④（最小化 DFA），false 取页签③。
     * @return 列数；面板为空时返回 0。
     * @note 供自测守住"字母表真的是字符类"：按字节时是 258 列（256+2），
     *       按字符类时应降到"类数 + 2"。
     */
    int automatonColumnCount(bool minimal) const;

    /**
     * @brief 【自测用】页签③字符类对照图例文本。
     * @return 形如 "letter = A-Z,a-z" 的逐行文本；无字符类字母表时为空串。
     */
    QString charClassLegendText() const;

    /**
     * @brief 【自测用】当前词法 DFA 的字母表符号数（字符类数）。
     * @return 字母表大小；尚未构造时为 0。
     */
    int automatonAlphabetSize() const;

    /** @brief 【自测用】取页签⑭当前语法树的 DOT 文本 */
    QString syntaxTreeDotText() const;

    // ---- 【自测用】页签⑭图形视图的浏览状态（"适应窗口 / 可拖动放大"守门员）----
    /** @brief 当前缩放比例（1.0 = 100%） */
    double syntaxTreeGraphZoom() const;
    /** @brief 当前比例是否正好是"适应窗口"比例 */
    bool syntaxTreeGraphAtFitZoom() const;
    /** @brief 整棵树是否落在视口内 */
    bool syntaxTreeGraphFits() const;
    /** @brief 整棵树是否在视口内居中 */
    bool syntaxTreeGraphCentered() const;
    /** @brief 是否可拖动浏览（内容 > 视口 → 有滚动条） */
    bool syntaxTreeGraphCanPan() const;
    /** @brief 内容尺寸（100% 时，像素） */
    QSize syntaxTreeGraphSceneSize() const;
    /** @brief 把图形视图缩放到指定比例（模拟用户拖滑块） */
    void setSyntaxTreeGraphZoom(double zoom);

private slots:
    /** @brief 语言下拉框变化 */
    void onLanguageChanged(int index);
    /** @brief 单词规格面板请求解析（正则式 → 规格表） */
    void onParseWordsRequested();
    /** @brief NFA 面板请求构造 NFA（规格 → 逆波兰式 → Thompson 构造） */
    void onBuildNfaRequested();
    /** @brief 单词规格面板请求完整词法流水线（阶段 3 实现） */
    void onFullLexPipelineRequested();
    /** @brief 页签⑤：转换方法或排版变化后重新生成源程序 */
    void onLexRegenerateRequested();
    /** @brief 页签⑥：执行"编译 + 运行 + 输出 .lex" */
    void onLexTestRequested();
    /** @brief 页签⑥：浏览待测源程序 */
    void onBrowseSourceRequested();
    /** @brief 页签⑭：导出语法树（三种文本形式一次写盘） */
    void onExportSyntaxTreeRequested();
    /** @brief 页签⑮：导出四元式中间代码 */
    void onExportIntermediateCodeRequested();
    /** @brief DFA 面板请求子集构造（NFA → DFA） */
    void onBuildDfaRequested();
    /** @brief 最小化 DFA 面板请求划分法最小化 */
    void onMinimizeDfaRequested();
    /** @brief 文法面板请求载入并化简（阶段 4 实现） */
    void onLoadAndSimplifyRequested();
    /** @brief 文法面板请求完整语法流水线（阶段 6 实现） */
    void onFullSyntaxPipelineRequested();
    /** @brief 菜单：关于 */
    void onAbout();
    /** @brief 菜单：关于 Qt */
    void onAboutQt();
    /** @brief 菜单：以默认浏览器打开项目目录 */
    void onOpenDataDir();
    /** @brief 清空日志 */
    void onClearLog();

private:
    /** @brief 建立菜单栏、工具栏、状态栏 */
    void buildActionsAndMenus();
    /** @brief 建立中央页签与左侧功能树 */
    void buildCentralArea();
    /** @brief 建立日志停靠窗口 */
    void buildLogDock();
    /** @brief 更新窗口标题与状态栏语言提示 */
    void updateLanguageIndicators();
    /**
     * @brief 更新状态栏的"当前操作"提示。
     * @param text 提示文本；传空串表示回到空闲状态（提示消失）。
     * @note 只描述**此刻正在做什么**（如子集构造进度），不描述开发进度 ——
     *       早期曾在此显示"当前进度：阶段 0（工程骨架）"，功能全部实现后
     *       就变成永久错误的文案。非空时会顺带排空消息队列，见实现处注释。
     */
    void setActivity(const QString &text);
    /** @brief 在日志面板与日志文件中追加一条界面级日志 */
    void appendLogLine(const QString &line);

    /**
     * @brief 解析"① 单词规格编辑器"中的文本，并把结果填入页签①的规格表。
     * @return 解析无错误为 true（有错误时仍会显示已解析出的部分结果）。
     * @note 解析结果缓存到 m_tokenSpec，供页签②构造 NFA 复用。
     */
    bool parseAndShowTokenSpec();

    /**
     * @brief 构造 NFA 并同时刷新页签②③④（NFA → DFA → 最小化 DFA）。
     * @return 构造并显示成功为 true。
     * @note 结果缓存到 m_nfa / m_dfa / m_minDfa 与三个 valid 标志，
     *       使各页签的按钮可以复用已有结果而不必重复计算。
     */
    bool buildAndShowAutomata();

    /**
     * @brief 由单词规格构造 NFA（含正则式解析与 Thompson 构造）。
     * @param rules    输出：参与构造的规则列表。
     * @param postfix  输出：逆波兰式对照文本。
     * @return 成功为 true。
     */
    bool buildNfaFromSpec(QVector<NfaRule> &rules, QString &postfix);

    /** @brief 切换到指定标题（含关键片段）的页签 */
    void activateTab(const QString &titleFragment);

    /**
     * @brief 执行一次完整词法流水线，并把结果分发到页签③~⑥。
     * @param runBuild 是否执行"g++ 编译 + 运行"（页签⑤只需生成源码时为 false）。
     * @return 生成源码成功为 true。
     * @note 这是任务一的总入口：菜单"执行完整词法流水线"、页签①按钮、
     *       页签⑤的"重新生成"、页签⑥的"编译并测试"都走这一个函数，
     *       保证界面操作与自测走的是同一条代码路径。
     */
    bool runLexPipeline(bool runBuild);

    /**
     * @brief 取当前语言的随附源程序路径（TINY → data/sample.tny）。
     * @return 文件路径；不存在时返回空串。
     */
    QString defaultSourceFile() const;

    /**
     * @brief 执行一次"文法前端"流水线（阶段 4+5），并把结果分发到页签⑦⑨⑩⑪⑫。
     * @param runParseTable 是否连分析表一起构造。
     * @return 整条链成功为 true。
     * @note 这是任务二的总入口：菜单"载入并化简文法"与页签⑦的按钮都走这一个函数。
     *       算法全部在此完成（面板只接收结果），符合 AGENT.md §7 的界面分层约定。
     */
    bool runSyntaxFrontend(bool runParseTable);

    /**
     * @brief 执行一次"语法分析 + 建树"流水线（阶段 6），并分发到页签⑬⑭。
     * @param lexPath 单词编码文件（`.lex`）路径；为空时取词法阶段的默认输出位置。
     * @return 分析成功且树非空为 true。
     * @note 这是任务二后半段的总入口：页签⑬的「开始语法分析」按钮与菜单都走这一个
     *       函数。它内部依次做：确保文法前端已跑完（否则先跑一次）→ 读 `.lex`
     *       （`syntax/LexReader`）→ 表驱动分析并建树（`syntax/Parser`）→
     *       把同一次结果分发给页签⑬（过程）与⑭（树）。
     *       两个页签必须共享**同一次**分析结果，否则会出现"过程显示通过、
     *       树却是上一次的"这种不一致。
     */
    bool runSyntaxAnalysis(const QString &lexPath = QString());

    // ---- 动作 ----
    QAction *m_actReloadData = nullptr;   ///< 重新载入当前语言的随附数据
    QAction *m_actOpenDataDir = nullptr;  ///< 打开数据目录
    QAction *m_actExit = nullptr;         ///< 退出
    QAction *m_actParseWords = nullptr;   ///< 解析单词规格
    QAction *m_actLexPipeline = nullptr;  ///< 执行完整词法流水线
    QAction *m_actSimplifyGrammar = nullptr;   ///< 载入并化简文法
    QAction *m_actSyntaxPipeline = nullptr;    ///< 执行完整语法流水线
    QAction *m_actClearLog = nullptr;     ///< 清空日志
    QAction *m_actAbout = nullptr;        ///< 关于
    QAction *m_actAboutQt = nullptr;      ///< 关于 Qt

    // ---- 控件 ----
    QComboBox *m_languageCombo = nullptr;         ///< 语言选择下拉框
    QTabWidget *m_tabWidget = nullptr;            ///< 中央功能页签
    QTreeWidget *m_functionTree = nullptr;        ///< 左侧功能树
    QDockWidget *m_logDock = nullptr;             ///< 日志停靠窗口
    QPlainTextEdit *m_logView = nullptr;          ///< 日志文本视图
    QLabel *m_statusLanguage = nullptr;           ///< 状态栏：当前语言
    QLabel *m_statusDataFile = nullptr;           ///< 状态栏：当前数据文件
    QLabel *m_statusActivity = nullptr;           ///< 状态栏：当前操作/进度提示（空闲为空串）

    // ---- 功能面板 ----
    WordsEditorPanel *m_wordsPanel = nullptr;         ///< ① 单词规格编辑器
    NfaPanel *m_nfaPanel = nullptr;                   ///< ② NFA
    DfaPanel *m_dfaPanel = nullptr;                   ///< ③ DFA（子集构造）
    DfaPanel *m_minDfaPanel = nullptr;                ///< ④ 最小化 DFA
    LexCodePanel *m_lexCodePanel = nullptr;           ///< ⑤ 词法分析源程序
    LexTestPanel *m_lexTestPanel = nullptr;           ///< ⑥ 编译与词法测试
    GrammarEditorPanel *m_grammarPanel = nullptr;     ///< ⑦ 文法编辑器
    GrammarSimplifyPanel *m_simplifyPanel = nullptr;  ///< ⑧ 文法化简结果
    FirstFollowPanel *m_firstFollowPanel = nullptr;   ///< ⑨ First/Follow
    LrItemsPanel *m_lrItemsPanel = nullptr;           ///< ⑩ LR(0) DFA
    LrItemsPanel *m_lrItemsPanel2 = nullptr;          ///< ⑪ LALR(1) 项目集（独立实例，见 setModeIndex 的说明）
    ParseTablePanel *m_parseTablePanel = nullptr;      ///< ⑫ LALR(1) 分析表
    SyntaxProcessPanel *m_syntaxProcessPanel = nullptr; ///< ⑬ 语法分析过程
    SyntaxTreePanel *m_syntaxTreePanel = nullptr;       ///< ⑭ 语法树
    IntermediateCodePanel *m_icPanel = nullptr;         ///< ⑮ 中间代码（选做）

    // ---- 文法前端结果缓存（阶段 4+5，供各页签与自测共享）----
    Grammar m_syntaxGrammar;                    ///< 最近一次载入的原始文法
    SimplifyResult m_simplifyResult;            ///< 最近一次化简结果
    Grammar m_augmentedGrammar;                 ///< 化简后并增广的文法
    ParseTable m_parseTable;                    ///< 最近一次构造的分析表
    bool m_syntaxValid = false;                 ///< 文法前端是否已成功跑完

    // ---- 语法分析结果缓存（阶段 6，供页签⑬⑭与自测共享）----
    SemanticsActionSet m_semantics;   ///< 最近一次载入的语义动作
    ParseResult m_parseResult;        ///< 最近一次语法分析结果（含语法树）
    IntermediateCode m_intermediateCode;      ///< 最近一次生成的四元式
    QString m_lastLexFile;            ///< 最近一次分析的 .lex 文件路径
    bool m_parseValid = false;        ///< 最近一次语法分析是否成功

    // ---- 解析结果缓存（跨面板共享，避免重复解析）----
    TokenSpec m_tokenSpec;              ///< 最近一次解析出的单词规格
    bool m_tokenSpecValid = false;      ///< 规格是否已成功解析
    Nfa m_nfa;                          ///< 最近一次构造的 NFA
    bool m_nfaValid = false;            ///< NFA 是否可用
    Dfa m_dfa;                          ///< 最近一次子集构造得到的 DFA
    bool m_dfaValid = false;            ///< DFA 是否可用
    Dfa m_minDfa;                       ///< 最近一次最小化得到的 DFA
    bool m_minDfaValid = false;         ///< 最小化 DFA 是否可用
    QString m_lastLexCode;              ///< 最近一次生成的词法分析源程序
    LexBuild::Result m_lastLexBuild;    ///< 最近一次编译/运行结果

    LanguageId m_language = LanguageId::Tiny;  ///< 当前语言
};

}  // namespace ccd
