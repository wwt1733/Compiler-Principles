/******************************************************************************
 * (1) 版权信息 : 编译原理课程设计 - 华南师范大学计算机学院
 * (2) 文件名称 : main.cpp
 *     标识符   : main
 *     摘要     : 程序入口。负责解析命令行参数，并按参数选择三种运行方式：
 *                  （1）默认        —— 创建 QApplication 显示主窗口（GUI 模式）；
 *                  （2）--console-selftest —— 只创建 QCoreApplication，运行不依赖
 *                      任何平台插件的算法/数据自测，**这是权威回归手段**；
 *                  （3）--selftest  —— 创建 QApplication 做 GUI 冒烟测试（需要
 *                      offscreen 平台插件，属可选验证）。
 *                另提供 --shot 把主窗口渲染为 PNG，便于开发期查看界面。
 * (3) 版本号   : 1.0.0       作者/修改者 : <学号 姓名>   完成日期 : 2026-XX-XX
 * (4) 版本历史 : 1.0.0  2026-XX-XX  首次实现
 *****************************************************************************/

#include <QApplication>
#include <QCommandLineParser>
#include <QCoreApplication>
#include <QFileInfo>
#include <QFont>
#include <QFontDatabase>
#include <QPixmap>
#include <QStringList>
#include <QTextStream>
#include <QTimer>

#include "common/FileUtil.h"
#include "common/LanguageConfig.h"
#include "common/Logger.h"
#include "common/SelfTest.h"
#include "gui/MainWindow.h"
#include "gui/widgets/StateTableView.h"  // 只为取 epsilonDotLabel()（断言要认它）

namespace {


/**
 * 功能：设置界面中文字体（优先微软雅黑，其次思源黑体、黑体）。
 * 输入：无。
 * 输出：无。
 * 注意：必须在 QApplication 构造之后调用；完全不依赖平台插件。
 */
void applyChineseFont()
{
    const QStringList preferred = {QStringLiteral("Microsoft YaHei"),
                                   QStringLiteral("Source Han Sans SC"),
                                   QStringLiteral("SimHei")};
    const QStringList families = QFontDatabase::families();
    for (const QString &family : preferred) {
        if (families.contains(family)) {
            QFont font(family);
            font.setPointSize(10);
            QApplication::setFont(font);
            return;
        }
    }
}

/**
 * 功能：把自测报告写到程序目录下的 selftest_report.txt 并打印到标准输出。
 * 输入：report —— 报告文本。
 * 输出：无。
 */
void emitReport(const QString &report)
{
    const QString path = ccd::FileUtil::appDir() + QStringLiteral("/selftest_report.txt");
    ccd::FileUtil::writeText(path, report, true);

    QTextStream out(stdout);
    out << report;
    out.flush();
}

}  // namespace

/**
 * 功能：程序入口。
 * 输入：argc / argv —— 命令行参数。
 * 输出：进程退出码。0 = 正常退出或自测全部通过；1 = 自测存在失败项。
 * 注意：
 *  1. `--console-selftest` 不创建任何 GUI 对象，因此不依赖 Qt 平台插件，
 *     可在无头环境稳定运行，是算法与数据正确性的权威验证方式；
 *  2. `--selftest` 会创建 QMainWindow，需要 offscreen 平台插件
 *     （$QT/plugins/platforms/qoffscreen.dll），属于可选的 GUI 冒烟测试；
 *  3. `--shot <png>` 把主窗口渲染为图片后退出，供开发期检查界面。
 */
int main(int argc, char *argv[])
{

    // ------------------------------------------------------------------
    // 先做一次"轻量参数预扫描"：--console-selftest 必须使用 QCoreApplication，
    // 以免创建 QApplication 时因缺少平台插件而崩溃。
    // ------------------------------------------------------------------
    bool consoleSelfTest = false;
    for (int i = 1; i < argc; ++i) {
        if (QString::fromLocal8Bit(argv[i]) == QLatin1String("--console-selftest"))
            consoleSelfTest = true;
    }

    if (consoleSelfTest) {
        QCoreApplication coreApp(argc, argv);
        QCoreApplication::setApplicationName(QStringLiteral("CompilerCourseDesign"));
        QCoreApplication::setApplicationVersion(QStringLiteral("1.0.0"));

        ccd::FileUtil::ensureDir(ccd::FileUtil::outputDir());
        ccd::Logger::instance()->setLogFile(ccd::FileUtil::outputDir()
                                           + QStringLiteral("/console_selftest.log"));

        ccd::SelfTestContext ctx;
        ccd::runConsoleSelfTest(ctx);
        emitReport(ctx.report());
        return (ctx.failedCount() == 0) ? 0 : 1;
    }

    // ------------------------------------------------------------------
    // GUI 模式（默认 / --selftest / --shot）
    // ------------------------------------------------------------------
    QApplication app(argc, argv);
    QApplication::setApplicationName(QStringLiteral("CompilerCourseDesign"));
    QApplication::setApplicationVersion(QStringLiteral("1.0.0"));
    QApplication::setOrganizationName(QStringLiteral("SCNU-CS"));
    applyChineseFont();

    QCommandLineParser parser;
    parser.setApplicationDescription(
        QStringLiteral("编译原理课程设计系统：正则式→NFA→DFA→最小化DFA→词法分析程序；"
                       "BNF→LALR(1)→语法树"));
    parser.addHelpOption();
    parser.addVersionOption();

    QCommandLineOption consoleSelfTestOption(
        QStringLiteral("console-selftest"),
        QStringLiteral("运行不依赖 GUI 平台的算法/数据自测（权威回归手段），"
                       "结果写入 selftest_report.txt。"));
    parser.addOption(consoleSelfTestOption);

    QCommandLineOption selfTestOption(
        QStringLiteral("selftest"),
        QStringLiteral("运行 GUI 冒烟自测（需要 offscreen 平台插件，"
                       "配合 -platform offscreen 使用）。"));
    parser.addOption(selfTestOption);

    QCommandLineOption languageOption(
        QStringLiteral("lang"),
        QStringLiteral("启动时使用的测试语言：tiny 或 minic（默认 tiny）。"),
        QStringLiteral("tiny|minic"), QStringLiteral("tiny"));
    parser.addOption(languageOption);

    QCommandLineOption shotOption(
        QStringLiteral("shot"),
        QStringLiteral("把主窗口渲染为 PNG 图片后退出（需 offscreen 平台插件）。"),
        QStringLiteral("png-path"));
    parser.addOption(shotOption);

    QCommandLineOption tabOption(QStringLiteral("tab"),
                                 QStringLiteral("--shot 时切换到第 N 个页签（从 0 开始）。"),
                                 QStringLiteral("N"), QStringLiteral("0"));
    parser.addOption(tabOption);

    QCommandLineOption demoOption(
        QStringLiteral("demo-lex"),
        QStringLiteral("在截图前先执行一次“解析单词规格 → 构造 NFA”，"
                       "使页签①与②带有真实数据（供开发期检查界面布局）。"));
    parser.addOption(demoOption);

    QCommandLineOption demoSyntaxOption(
        QStringLiteral("demo-syntax"),
        QStringLiteral("在截图前先执行一次“载入文法 → 化简 → First/Follow → "
                       "LR(0)/LALR(1) 项目集 → 分析表”，使页签⑧~⑫带有真实数据。"));
    parser.addOption(demoSyntaxOption);

    QCommandLineOption demoFullOption(
        QStringLiteral("demo-full"),
        QStringLiteral("在截图前把两条流水线都完整跑一遍（词法流水线含编译运行 → 文法前端 → "
                       "语法分析 → 四元式），使 15 个功能页签都带有真实数据。"
                       "文档（使用说明书/测试报告）的界面截图用它。"));
    parser.addOption(demoFullOption);

    parser.process(app);

    const bool selfTest = parser.isSet(selfTestOption);
    const ccd::LanguageId language =
        ccd::LanguageConfigRegistry::fromKey(parser.value(languageOption));

    ccd::MainWindow window;
    window.initialize(language);

    // ---- --shot：渲染主窗口为 PNG 后退出 ----
    if (parser.isSet(shotOption)) {
        const QString shotPath = parser.value(shotOption);
        const int tabIndex = parser.value(tabOption).toInt();
        // 可选：--demo-full —— 完整跑一遍两条流水线，使 15 个页签都有真实数据。
        // 文档（使用说明书/测试报告）的截图依赖它：仅用 --demo-lex / --demo-syntax
        // 时，页签⑤⑥（生成的源程序、单词编码）与页签⑮（四元式）会是空的。
        if (parser.isSet(demoFullOption)) {
            QString sLex;
            window.runLexicalUiCheck(sLex);
            QString s3;
            window.runStage3UiCheck(true, s3);
            QString s45;
            window.runStage45UiCheck(s45);
            QString s6;
            window.runStage6UiCheck(s6);
        }
        // 可选：先跑一遍词法界面流程，让页签①与②填入真实数据
        if (parser.isSet(demoOption)) {
            QString demoSummary;
            window.runLexicalUiCheck(demoSummary);
        }
        // 可选：先跑一遍文法前端流水线（阶段 4+5）与语法分析流水线（阶段 6），
        // 让页签⑧~⑭填入真实数据。
        // 没有这一步时，--shot 抓到的是**尚未构造任何项目集/语法树的空面板** ——
        // 截图看起来"是空的"，很容易被误判成界面没做出来。
        if (parser.isSet(demoSyntaxOption)) {
            QString demoSyntaxSummary;
            window.runStage45UiCheck(demoSyntaxSummary);

            QString demoParseSummary;
            window.runStage6UiCheck(demoParseSummary);
        }
        // 截图专用窗口尺寸：默认 1036x632 抓到文档里会糊，放大到 1600x1000
        window.resize(1600, 1000);
        window.show();
        window.selectTab(tabIndex);
        // 让布局与样式表完成一次计算，再抓图，避免抓到未布局的空窗口
        QCoreApplication::processEvents();
        const QPixmap shot = window.grab();
        const bool saved = !shot.isNull() && shot.save(shotPath, "PNG");

        QTextStream out(stdout);
        out << QStringLiteral("截图%1：%2  (%3x%4)\n")
                   .arg(saved ? QStringLiteral("成功") : QStringLiteral("失败"), shotPath)
                   .arg(shot.width())
                   .arg(shot.height());
        out.flush();
        // 注意：此处不使用 app.exec()/exit()，直接返回，避免事件循环与
        //       平台插件在退出阶段的清理问题（见 AGENT.md §13 已知陷阱）。
        return saved ? 0 : 1;
    }

    // ---- 默认模式：正常显示主窗口 ----
    if (!selfTest) {
        window.show();
        return app.exec();
    }

    // ---- --selftest：GUI 冒烟测试 ----
    window.show();

    ccd::SelfTestContext ctx;
    int exitCode = 0;
    QTimer::singleShot(0, &app, [&] {
        ccd::runConsoleSelfTest(ctx);
        // GUI 冒烟断言：主窗口已建立且标题正确
        ctx.check(QStringLiteral("G1 主窗口已创建且标题包含系统名"),
                  window.windowTitle().contains(QStringLiteral("编译原理课程设计")),
                  window.windowTitle());

        // G1b：页签顺序必须与标题上的带圈数字一致。
        // 守的是"某个页签被摆错位置"这类缺陷 —— 本项目曾把⑦文法编辑器
        // 单独 addTab 到构造循环之后，它于是掉到页签栏最末尾，
        // 第 7 个位置显示的是⑧ 文法化简结果，使用者以为⑦没做出来。
        {
            const QString kCircled = QStringLiteral("①②③④⑤⑥⑦⑧⑨⑩⑪⑫⑬⑭⑮");
            const QStringList titles = window.tabTitles();
            QStringList mismatch;
            for (int i = 0; i < titles.size() && i < kCircled.size(); ++i) {
                if (!titles.at(i).startsWith(kCircled.at(i)))
                    mismatch << QStringLiteral("第%1个应为%2，实际%3")
                                    .arg(i + 1)
                                    .arg(kCircled.at(i))
                                    .arg(titles.at(i));
            }
            ctx.check(QStringLiteral("G1b 页签顺序与带圈数字一致（共 %1 个页签）")
                          .arg(titles.size()),
                      titles.size() >= 15 && mismatch.isEmpty(),
                      mismatch.isEmpty() ? titles.join(QStringLiteral(" | "))
                                         : mismatch.join(QStringLiteral("；")));
        }
        // GUI 功能断言：走与按钮点击相同的代码路径，验证页签①规格表与②NFA 表。
        // 注意：窗口构造时已通过 initialize() 载入随附语言数据，因此此处无需
        //       再手工填充编辑框；若编辑框为空则说明载入环节出了问题。
        QString uiSummary;
        const bool uiOk = window.runLexicalUiCheck(uiSummary);
        ctx.check(QStringLiteral("G2 界面流程“解析规格 → 构造 NFA”可用"), uiOk, uiSummary);

        // GUI 断言 G3：阶段 3 的界面链路（页签①→⑤ 生成源码，页签⑥ 编译运行）
        QString s3Summary;
        const bool s3Ok = window.runStage3UiCheck(true, s3Summary);
        ctx.check(QStringLiteral("G3 界面流程“完整词法流水线（含编译运行）”可用"), s3Ok, s3Summary);
        ctx.check(QStringLiteral("G4 页签⑤已显示生成的词法分析源程序"),
                  window.lexCodeText().contains(QStringLiteral("int main"))
                      && window.lexCodeText().size() > 2000,
                  QStringLiteral("%1 字节").arg(window.lexCodeText().size()));
        ctx.check(QStringLiteral("G5 页签⑥已显示单词编码文件内容且末行为 EOF"),
                  window.lexTokenText().trimmed().endsWith(QStringLiteral("EOF")),
                  QStringLiteral("末行：%1")
                      .arg(window.lexTokenText().trimmed().section(QLatin1Char('\n'), -1)));
        ctx.check(QStringLiteral("G6 页签⑥的 .lex 文件已落盘"),
                  !window.lexTokenPath().isEmpty()
                      && QFileInfo::exists(window.lexTokenPath()),
                  window.lexTokenPath());

        // GUI 断言 G7：阶段 4+5 的界面链路（页签⑦→⑧→⑨→⑩⑪→⑫）
        // 走 MainWindow::runSyntaxFrontend()，与页签⑦"载入并化简"按钮同一条路径。
        QString s45Summary;
        const bool s45Ok = window.runStage45UiCheck(s45Summary);
        ctx.check(QStringLiteral("G7 界面流程“载入文法→化简→First/Follow→"
                                 "项目集→分析表”可用"),
                  s45Ok, s45Summary);

        ctx.check(QStringLiteral("G8 页签⑫已显示分析表摘要且含状态数"),
                  window.parseTableSummary().contains(QStringLiteral("状态")),
                  window.parseTableSummary());

        ctx.check(QStringLiteral("G9 页签⑫已显示冲突报告"),
                  !window.parseTableConflicts().isEmpty(),
                  window.parseTableConflicts().left(120));

        // 页签⑩⑪共用同一面板：状态数应远大于 2（若闭包没展开会退化成 2，
        // 这正是阶段 5 调试时踩到的那个 bug 的界面侧守门员）
        ctx.check(QStringLiteral("G10 页签⑩⑪项目集状态数 > 10（实际 %1）")
                      .arg(window.lrItemsStateCount()),
                  window.lrItemsStateCount() > 10, window.lrItemsSummary());

        // GUI 断言 G11~G15：阶段 6 的界面链路（页签⑬⑭）
        // 走 MainWindow::runSyntaxAnalysis()，与页签⑬"开始语法分析"按钮同一条路径。
        // 这里刻意在 G7 之后再跑一次文法前端，是为了让"词法 → 文法 → 语法"
        // 三段流水线在同一次自测里都真实执行过（而不是读缓存）。
        QString s6Summary;
        const bool s6Ok = window.runStage6UiCheck(s6Summary);
        ctx.check(QStringLiteral("G11 界面流程“读 .lex → LALR(1) 分析 → 建树”可用"),
                  s6Ok, s6Summary);

        ctx.check(QStringLiteral("G12 页签⑬已显示分析过程（%1 行）")
                      .arg(window.syntaxStepRowCount()),
                  window.syntaxStepRowCount() > 10, window.syntaxProcessSummary());

        ctx.check(QStringLiteral("G13 页签⑭已显示语法树（%1 个结点）")
                      .arg(window.syntaxTreeNodeCount()),
                  window.syntaxTreeNodeCount() > 10, window.syntaxTreeText().left(160));

        ctx.check(QStringLiteral("G14 页签⑭语法树文本含非终结符结点名"),
                  window.syntaxTreeText().contains(QStringLiteral("stmt-sequence"))
                      && window.syntaxTreeBracket().startsWith(QLatin1Char('(')),
                  window.syntaxTreeBracket().left(160));

        ctx.check(QStringLiteral("G15 页签⑭已列出语法树结构定义（%1 条）")
                      .arg(window.syntaxSemanticsRowCount()),
                  window.syntaxSemanticsRowCount() > 10,
                  QStringLiteral("实际 %1 条").arg(window.syntaxSemanticsRowCount()));

        // ---- G14b~G14d：语法树图形视图必须"自动适应窗口 + 能拖动放大" ----
        // 用户反馈：⑭ 的语法树缩在左上角成一团，没法放大看。
        // 根因是布局坐标曾被当作像素用（82 个结点挤进约 40×20 像素），
        // 而且"适应窗口"按钮用的是控件自身尺寸算比例，永远算得 ≈0.95 形同虚设。
        // 现在默认自动适应 + 滚轮缩放 + 拖动平移，这三条断言把行为钉住。
        //
        // 注意：必须**先切到页签⑭**再断言 —— 非当前页的 QGraphicsView 还没被
        // 布局成最终尺寸，此时算出来的"适应窗口"比例会被下限钳住，不代表用户
        // 真正看到的画面。切过去会触发一次 resize，视图借机重新适应。
        {
            window.selectTab(13);
            QCoreApplication::processEvents();

            const double fitZoom = window.syntaxTreeGraphZoom();
            const QSize sceneSize = window.syntaxTreeGraphSceneSize();
            ctx.check(QStringLiteral("G14b 语法树图形视图已自动适应窗口（%1%，内容 %2×%3）")
                          .arg(fitZoom * 100.0, 0, 'f', 1)
                          .arg(sceneSize.width())
                          .arg(sceneSize.height()),
                      window.syntaxTreeGraphAtFitZoom() && window.syntaxTreeGraphFits()
                          && fitZoom < 1.0,
                      QStringLiteral("是否适应比例=%1 是否落在视口内=%2 比例=%3")
                          .arg(window.syntaxTreeGraphAtFitZoom() ? 1 : 0)
                          .arg(window.syntaxTreeGraphFits() ? 1 : 0)
                          .arg(fitZoom, 0, 'f', 3));

            ctx.check(QStringLiteral("G14c 语法树图形视图中的内容居中显示"),
                      window.syntaxTreeGraphCentered());

            // 放大到 300%：内容应超过视口 → 出现滚动条 → 可以拖动查看细节
            window.setSyntaxTreeGraphZoom(3.0);
            ctx.check(QStringLiteral("G14d 放大到 300% 后可拖动浏览（内容 %1×%2）")
                          .arg(sceneSize.width() * 3)
                          .arg(sceneSize.height() * 3),
                      qAbs(window.syntaxTreeGraphZoom() - 3.0) < 1e-6
                          && window.syntaxTreeGraphCanPan());

            // 复原成"适应窗口"，不影响后续断言与截图
            window.setSyntaxTreeGraphZoom(fitZoom);
        }

        // ---- G16~G21：阶段 7 —— mini-C 词法 + 语法端到端联调 ----
        // mini-C 覆盖 TINY 完全没有的语法：类型指示符、函数定义、参数表、
        // 数组声明与下标、do-while、复合语句与局部定义。
        // 尤其是圆括号 `(` 与方括号 `[` 在文法里分别叫 LLM 与 LMM，
        // 只有真的拿一份含函数定义与数组的源程序跑一遍，才能确认这两个名字
        // 没有被写反（本项目就真的写反过，见自测 T16.1）。
        const bool toMinic = window.setLanguage(ccd::LanguageId::MiniC);
        ctx.check(QStringLiteral("G16 已切换到 mini-C 语言"), toMinic,
                  QStringLiteral("当前语言 = %1")
                      .arg(window.currentLanguage() == ccd::LanguageId::MiniC
                               ? QStringLiteral("MiniC")
                               : QStringLiteral("Tiny")));

        if (toMinic) {
            // 词法流水线：生成 mini-C 词法分析源程序 → g++ 编译 → 跑 sample.mc
            QString mcLexSummary;
            const bool mcLexOk = window.runStage3UiCheck(true, mcLexSummary);
            ctx.check(QStringLiteral("G17 mini-C 词法流水线（生成→编译→运行 sample.mc）可用"),
                      mcLexOk, mcLexSummary);
            ctx.check(QStringLiteral("G18 mini-C 产出 .lex 且末行为 EOF（%1）")
                          .arg(window.lexTokenText().trimmed().section(QLatin1Char('\n'), -1)),
                      window.lexTokenText().trimmed().endsWith(QStringLiteral("EOF")),
                      window.lexTokenPath());

            // 语法流水线：读 .lex → LALR(1) 分析 → 建树
            QString mcParseSummary;
            const bool mcParseOk = window.runStage6UiCheck(mcParseSummary);
            ctx.check(QStringLiteral("G19 mini-C 语法流水线（读 .lex → 分析 → 建树）可用"),
                      mcParseOk, mcParseSummary);
            ctx.check(QStringLiteral("G20 mini-C 语法树非空且根为 program（%1 个结点）")
                          .arg(window.syntaxTreeNodeCount()),
                      window.syntaxTreeNodeCount() > 20
                          && window.syntaxTreeText().startsWith(QStringLiteral("program")),
                      window.syntaxTreeText().left(120));
            ctx.check(QStringLiteral("G21 mini-C 语法树体现函数定义、复合语句、变量定义与类型关键字"),
                      window.syntaxTreeBracket().contains(QStringLiteral("function-definition"))
                          && window.syntaxTreeBracket().contains(QStringLiteral("compound-stmt"))
                          && window.syntaxTreeBracket().contains(QStringLiteral("variable-definition"))
                          && (window.syntaxTreeBracket().contains(QStringLiteral("int"))
                              || window.syntaxTreeBracket().contains(QStringLiteral("void"))
                              || window.syntaxTreeBracket().contains(QStringLiteral("double"))),
                      window.syntaxTreeBracket());
        }

        // ---- G22~G24：阶段 8（选做）—— 四元式中间代码 ----
        // 注意：此时语言是 mini-C（上面的 G16 切过去了），因此这三条断言
        // 检查的是 mini-C 样例程序的四元式。
        ctx.check(QStringLiteral("G22 页签⑮已显示四元式（%1 条）")
                      .arg(window.intermediateQuadCount()),
                  window.intermediateQuadCount() > 10, window.intermediateSummary());

        // sample.mc 以 `int x; int fact; double y; int arr[100];` 四条全局定义开头，
        // 因此四元式首行应当是变量定义 `decl`；函数定义标记 `func/endfunc` 在后。
        ctx.check(QStringLiteral("G23 页签⑮首行为变量定义、且含函数边界标记（首行 %1）")
                      .arg(window.intermediateQuadCount() > 0
                               ? window.intermediateCellText(0, 1)
                               : QString()),
                  window.intermediateQuadCount() > 0
                      && window.intermediateCellText(0, 1) == QStringLiteral("decl")
                      && window.intermediateText().contains(QStringLiteral("(func,"))
                      && window.intermediateText().contains(QStringLiteral("(endfunc,")),
                  window.intermediateQuadCount() > 0
                      ? QStringLiteral("(%1, %2, %3, %4)")
                            .arg(window.intermediateCellText(0, 1),
                                 window.intermediateCellText(0, 2),
                                 window.intermediateCellText(0, 3),
                                 window.intermediateCellText(0, 4))
                      : QString());

        ctx.check(QStringLiteral("G24 页签⑮四元式文本含控制流与函数边界"),
                  window.intermediateText().contains(QStringLiteral("(func,"))
                      && window.intermediateText().contains(QStringLiteral("(label,"))
                      && window.intermediateText().contains(QStringLiteral("(goto,")),
                  window.intermediateText().left(400));

        // ---- G25：导出的 DOT 必须是纯 ASCII，且能被 Graphviz 真正解析 ----
        // 这是用户反馈的问题：自动机字母表覆盖全部 256 个字节值，边标签里会出现
        // NUL / CR / 高位字节，原样写进 .dot 时 Graphviz 报
        // `syntax error ... scanning a quoted string`，整张图都画不出来。
        // 这里断言 DOT 文本里既没有非 ASCII 码点、也没有裸控制字节，
        // 并把 DOT 落盘，供外部用 `dot -Tpng` 实测（自测进程内不调外部程序）。
        {
            const QString dot = window.automatonDotText(true);
            const QString treeDot = window.syntaxTreeDotText();

            const auto firstBad = [](const QString &text) -> QString {
                for (int i = 0; i < text.size(); ++i) {
                    const ushort u = text.at(i).unicode();
                    if (u > 0x7E || (u < 0x20 && u != uchar('\n') && u != uchar('\t')))
                        return QStringLiteral("位置 %1 出现 U+%2")
                            .arg(i)
                            .arg(QString::number(u, 16)
                                     .rightJustified(4, QLatin1Char('0'))
                                     .toUpper());
                }
                return QString();
            };

            ctx.check(QStringLiteral("G25 最小化 DFA 的 DOT 为纯 ASCII（%1 字符）")
                          .arg(dot.size()),
                      !dot.isEmpty() && firstBad(dot).isEmpty(),
                      firstBad(dot).isEmpty() ? dot.left(140) : firstBad(dot));

            ctx.check(QStringLiteral("G25b 语法树的 DOT 为纯 ASCII（%1 字符）")
                          .arg(treeDot.size()),
                      !treeDot.isEmpty() && firstBad(treeDot).isEmpty(),
                      firstBad(treeDot).isEmpty() ? treeDot.left(140) : firstBad(treeDot));

            // 落盘，供外部 graphviz 实测
            const QString dotDir = ccd::FileUtil::outputDir() + QStringLiteral("/dot");
            ccd::FileUtil::ensureDir(dotDir);
            ccd::FileUtil::writeText(dotDir + QStringLiteral("/selftest_automaton.dot"),
                                     dot, false);
            ccd::FileUtil::writeText(dotDir + QStringLiteral("/selftest_tree.dot"),
                                     treeDot, false);
        }

        // ---- G26~G28：字母表确实是"字符类"，而不是 256 个字节 ----
        // 用户反馈过：宏 letter=[A-Za-z] 被就地展开，状态转换表 258 列、图上
        // 每条边挂 200 多个符号，根本没法看。改造后一个字符集只占一个符号。
        {
            const QString dot = window.automatonDotText(true);
            // G26：图上的边标签应当出现字符类名，而且不再出现展开后的字母表
            const bool hasLetter = dot.contains(QStringLiteral("letter"));
            const bool hasDigit = dot.contains(QStringLiteral("digit"));
            const bool hasOther = dot.contains(QStringLiteral("<other>"));
            // 展开后的字母被当成类名连写的情况：出现 "A,B,C" 这种连续大写字母罗列
            const bool looksExpanded = dot.contains(QStringLiteral("A,B,C"));
            ctx.check(QStringLiteral("G26 自动机 DOT 的边标签使用字符类名（letter/digit/<other>）"),
                      hasLetter && hasDigit && hasOther && !looksExpanded,
                      QStringLiteral("letter=%1 digit=%2 <other>=%3 像展开的字母表=%4")
                          .arg(hasLetter ? 1 : 0)
                          .arg(hasDigit ? 1 : 0)
                          .arg(hasOther ? 1 : 0)
                          .arg(looksExpanded ? 1 : 0));

            // G27：状态转换表列数 = 字符类数 + 2（状态、说明）
            const int columns = window.automatonColumnCount(true);
            const int classes = window.automatonAlphabetSize();
            ctx.check(QStringLiteral("G27 状态转换表列数 = 字符类数 + 2（%1 列 / %2 类）")
                          .arg(columns)
                          .arg(classes),
                      classes > 0 && classes < 40 && columns == classes + 2,
                      QStringLiteral("列数 %1、字符类 %2").arg(columns).arg(classes));

            // G28：字符类图例逐类给出字节集（<other> 才不会歧义）
            const QString legend = window.charClassLegendText();
            ctx.check(QStringLiteral("G28 字符类对照图例含 letter 的字节集"),
                      legend.contains(QStringLiteral("letter = A-Z,a-z"))
                          && legend.contains(QStringLiteral("<other>")),
                      legend.left(120).replace(QLatin1Char('\n'), QLatin1Char(' ')));
        }

        // ---- G29~G30：NFA 的图必须画出 ε 边，且不能有孤立状态 ----
        // 用户反馈：DFA 与最小化 DFA 的图正常，但 NFA 的图"怪怪的"。
        // 根因：DOT 原本是从"状态转换表副本"里凑的（一符号一目的的 DFA 结构），
        // **ε 边与一格多目的状态都进不去**，而 Thompson 构造里 ε 边占绝大多数，
        // 于是图上一大片状态看着是孤立的。现在改成从显式边列表渲染。
        {
            const QString nfaDot = window.nfaDotText();
            const QString epsLabel = ccd::StateTableView::epsilonDotLabel();

            // 解析 DOT：节点行 `q7 [shape=...]`，边行 `q7 -> q9 [label="..."]`。
            // 注意 `__start -> q0;` 这一行不算数据边（它是画初态的那个小箭头），
            // 以 "q" 开头的过滤条件正好把它排除。
            // nodeId：从 from 起跳过非数字字符（即那个 'q'），再读出状态号。
            const auto nodeId = [](const QString &text, int from) {
                int id = -1;
                bool started = false;
                for (int i = from; i < text.size(); ++i) {
                    const QChar ch = text.at(i);
                    if (!ch.isDigit()) {
                        if (started)
                            break;
                        continue;
                    }
                    started = true;
                    if (id < 0)
                        id = 0;
                    id = id * 10 + ch.digitValue();
                }
                return id;
            };

            QStringList nodes;
            QStringList doubleCircled;
            QStringList withIncoming;
            QStringList withOutgoing;
            int epsEdges = 0;
            int symbolEdges = 0;
            for (const QString &rawLine : nfaDot.split(QLatin1Char('\n'))) {
                const QString line = rawLine.trimmed();
                if (!line.startsWith(QLatin1Char('q')))
                    continue;
                const int arrow = line.indexOf(QStringLiteral(" -> "));
                if (arrow < 0) {
                    const int id = nodeId(line, 1);
                    if (id < 0)
                        continue;
                    nodes << QString::number(id);
                    if (line.contains(QStringLiteral("doublecircle")))
                        doubleCircled << QString::number(id);
                    continue;
                }
                const int from = nodeId(line, 1);
                const int to = nodeId(line, arrow + 4);
                if (from < 0 || to < 0)
                    continue;
                withOutgoing << QString::number(from);
                withIncoming << QString::number(to);
                if (line.contains(QStringLiteral("label=\"%1\"").arg(epsLabel)))
                    ++epsEdges;
                else
                    ++symbolEdges;
            }

            ctx.check(QStringLiteral("G29.1 NFA 的图里有 ε 边（虚线、标 %1）").arg(epsLabel),
                      epsEdges > 0,
                      QStringLiteral("ε 边 %1 条 / 符号边 %2 条 / 节点 %3 个")
                          .arg(epsEdges)
                          .arg(symbolEdges)
                          .arg(nodes.size()));

            // 除统一初态（q0）外，每个节点都必须有入边 —— 这正是"看着孤立"的反面
            QStringList orphans;
            for (const QString &id : nodes) {
                if (id == QString::number(window.nfaStartState()))
                    continue;
                if (!withIncoming.contains(id))
                    orphans << id;
            }
            ctx.check(QStringLiteral("G29.2 除统一初态外每个状态都有入边（无孤立状态）"),
                      !nodes.isEmpty() && orphans.isEmpty(),
                      orphans.isEmpty() ? QStringLiteral("节点 %1 个").arg(nodes.size())
                                        : QStringLiteral("孤立状态：%1")
                                              .arg(orphans.mid(0, 12).join(QLatin1Char(','))));

            // 每个非接受态都必须有出边（Thompson 构造的固有性质）
            QStringList deadEnds;
            for (const QString &id : nodes) {
                if (doubleCircled.contains(id))
                    continue;
                if (!withOutgoing.contains(id))
                    deadEnds << id;
            }
            ctx.check(QStringLiteral("G29.3 每个非接受态都有出边"),
                      !nodes.isEmpty() && deadEnds.isEmpty(),
                      deadEnds.isEmpty() ? QString()
                                         : QStringLiteral("无出边：%1")
                                               .arg(deadEnds.mid(0, 12).join(QLatin1Char(','))));

            // 双圈节点数 == 接受态数（**含注释规则的接受态** —— 过去按"有编码"
            // 判断会把注释的接受态画成单圈，因为它 acceptCode == -1）
            const int acceptingCount = window.nfaAcceptingStateCount();
            ctx.check(QStringLiteral("G29.4 双圈节点数等于 NFA 接受态数（含注释规则）"),
                      acceptingCount > 0 && doubleCircled.size() == acceptingCount,
                      QStringLiteral("双圈 %1 个 / 接受态 %2 个")
                          .arg(doubleCircled.size())
                          .arg(acceptingCount));

            // 编码策略：NFA 图是全项目**唯一**含非 ASCII 的 DOT，
            // 只允许出现 ε 这一个字符，其余必须是可打印 ASCII 且不得有控制字节。
            // （若哪天把 epsilonDotLabel() 改成 "eps"，这里自动退化成"纯 ASCII"，无需改断言）
            QString badChar;
            const bool epsilonIsSingleChar = (epsLabel.size() == 1);
            for (const QChar &ch : nfaDot) {
                const ushort u = ch.unicode();
                if (epsilonIsSingleChar && u == epsLabel.at(0).unicode())
                    continue;
                if (u == uchar('\n') || u == uchar('\t'))
                    continue;
                if (u < 0x20 || u > 0x7E) {
                    badChar = QStringLiteral("出现 U+%1")
                                  .arg(QString::number(u, 16).rightJustified(4, QLatin1Char('0'))
                                           .toUpper());
                    break;
                }
            }
            ctx.check(QStringLiteral("G29.5 NFA 的 DOT 除 ε 外均为可打印 ASCII"),
                      !nfaDot.isEmpty() && badChar.isEmpty(),
                      badChar.isEmpty() ? nfaDot.left(120) : badChar);

            // G30：把 NFA 的 DOT 落盘，供外部用真实 Graphviz 实测
            // （自测进程内不调外部程序，与 G25 的做法一致）
            const QString dotDir = ccd::FileUtil::outputDir() + QStringLiteral("/dot");
            ccd::FileUtil::ensureDir(dotDir);
            const bool nfaSaved = ccd::FileUtil::writeText(
                dotDir + QStringLiteral("/selftest_nfa.dot"), nfaDot, false);
            ctx.check(QStringLiteral("G30 NFA 的 DOT 已落盘（%1 字节）").arg(nfaDot.size()), nfaSaved,
                      dotDir + QStringLiteral("/selftest_nfa.dot"));
        }

        // ---- G31~G33：状态栏的"当前操作"提示 ----
        // 守两件事：
        //  1. 界面文案不得用来表达"开发进度"—— 早期状态栏右端写着
        //     "当前进度：阶段 0（工程骨架）"，功能全部实现后就成了永久错误的文案；
        //  2. 进度提示必须有收尾清空 —— 子集构造约 9 秒期间状态栏显示进度
        //     （界面靠它证明没卡死），但过去构造结束后从不清除，跑完流水线
        //     状态栏会一直挂着"正在构造 DFA…已生成 N 个状态"。
        const QString statusBar = window.statusBarSummary();
        ctx.check(QStringLiteral("G31 状态栏显示当前语言与实际数据文件"),
                  statusBar.contains(QStringLiteral("语言："))
                      && statusBar.contains(QStringLiteral("数据：")),
                  statusBar);
        ctx.check(QStringLiteral("G32 状态栏不再出现“开发阶段”字样"),
                  !statusBar.contains(QStringLiteral("阶段")), statusBar);
        ctx.check(QStringLiteral("G33 流水线结束后状态栏的进度提示已清空"),
                  window.statusActivityText().isEmpty(), window.statusActivityText());

        emitReport(ctx.report());
        exitCode = (ctx.failedCount() == 0) ? 0 : 1;
        app.exit(exitCode);
    });

    app.exec();
    return exitCode;
}
