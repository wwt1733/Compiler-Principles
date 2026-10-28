/******************************************************************************
 * (1) 版权信息 : 编译原理课程设计 - 华南师范大学计算机学院
 * (2) 文件名称 : LexBuild.cpp
 *     标识符   : LexBuild
 *     摘要     : "生成 → 编译 → 运行"流水线的实现。用 QProcess 依次调用
 *                g++ 编译、运行生成的可执行文件，并把每一步的输出与返回码
 *                完整保留下来，作为任务书 1.9"能正确运行并产出 .lex"的证据。
 * (3) 版本号   : 1.0.0       作者/修改者 : <学号 姓名>   完成日期 : 2026-XX-XX
 * (4) 版本历史 : 1.0.0  2026-XX-XX  首次实现
 *****************************************************************************/

#include "lex/LexBuild.h"

#include <QDir>
#include <QElapsedTimer>
#include <QFile>
#include <QFileInfo>
#include <QProcess>
#include <QProcessEnvironment>
#include <QStandardPaths>

#include "common/FileUtil.h"
#include "common/Logger.h"

namespace ccd {

/**
 * 功能：生成一句话摘要。
 * 输入：无（使用本对象）。
 * 输出：摘要文本。
 * 注意：摘要按阶段拼接，未执行到的阶段不出现，便于快速定位卡在哪一步。
 */
QString LexBuild::Result::summary() const
{
    QStringList parts;
    parts << (generated ? QStringLiteral("生成源程序 ✓") : QStringLiteral("生成源程序 ✗"));
    parts << (compiled ? QStringLiteral("g++ 编译 ✓") : QStringLiteral("g++ 编译 ✗"));
    parts << (ran ? QStringLiteral("运行并产出 .lex ✓") : QStringLiteral("运行 ✗"));
    QString text = parts.join(QStringLiteral("　"));
    if (compileExitCode >= 0)
        text += QStringLiteral("（编译返回码 %1").arg(compileExitCode);
    if (runExitCode >= 0)
        text += QStringLiteral("，运行返回码 %1").arg(runExitCode);
    if (compileExitCode >= 0 || runExitCode >= 0)
        text += QStringLiteral("）");
    return text;
}

/**
 * 功能：执行一条外部命令并收集输出。
 * 输入：program —— 可执行文件；args —— 参数列表；exitCode —— 输出返回码。
 * 输出：stdout 与 stderr 合并后的文本。
 * 注意：
 *  1. 用 QProcess::start(program, args) 直接传参数列表，Qt 会负责加引号，
 *     因此含中文与空格的路径不会出错（曾经用字符串拼接 shell 命令踩过坑）；
 *  2. **必须显式继承并补充 PATH**：g++ 依赖同目录下的 ld.exe / collect2.exe
 *     以及 x86_64-w64-mingw32/bin 下的链接部件。若子进程拿到的 PATH 缺少这些
 *     目录，编译阶段能通过、**链接阶段**却会以 "ld.exe: cannot ..." 失败
 *     —— 本模块实际踩过这个坑，且因为错误信息被截断而极难定位；
 *  3. 超时设为 60 秒：g++ 编译本项目的生成代码只需几百毫秒，
 *     60 秒未结束基本可判定为环境异常；
 *  4. **必须先把裸程序名解析成绝对路径**（见下方实现开头的长注释）：GCC 靠 argv[0]
 *     定位自己的 cc1plus/ld，裸名 + 被改过的 PATH 会让它推错安装前缀，
 *     报 "cannot execute 'cc1plus'"（AGENT.md §13.38）。
 */
QString LexBuild::runCommand(const QString &program, const QStringList &args, int &exitCode,
                             const QString &workingDir)
{
    exitCode = -1;

    // ---- 先把"裸程序名"解析成绝对路径，再用它补子进程的 PATH ----
    // 为什么必须这样做：GCC 的驱动程序要靠 argv[0] 定位自己的 cc1plus / ld 等部件。
    // 若既传裸名 "g++"（argv[0] 就是 "g++"，不含任何目录信息），又把**另一套**工具链的
    // bin 目录前置进子进程 PATH，GCC 就会按 PATH 里第一个 g++ 的位置去推算自己的安装
    // 前缀，于是到错的目录里找 cc1plus，报：
    //   g++: fatal error: cannot execute 'cc1plus': CreateProcess: No such file or directory
    // 本项目实际踩到：机器上同时装了 D:\mingw\mingw64（g++ 15.2.0）与 Qt 自带的
    // mingw1310_64（g++ 13.1.0），父进程 PATH 里 D:\mingw 在前时，界面上的
    // "编译并测试"必然失败，而自测里按**绝对路径**传编译器的那几条却全通过 ——
    // 差别只在 argv[0] 有没有目录，极难定位。
    QString programPath = program;
    const auto hasDir = [](const QString &p) {
        return p.contains(QLatin1Char('/')) || p.contains(QLatin1Char('\\'));
    };
    if (!hasDir(program)) {
        const QString found = QStandardPaths::findExecutable(program);
        if (!found.isEmpty())
            programPath = found;
    }

    QProcess process;
    process.setProgram(programPath);
    process.setArguments(args);
    process.setProcessChannelMode(QProcess::MergedChannels);
    if (!workingDir.isEmpty())
        process.setWorkingDirectory(workingDir);

    // 继承系统环境，并把编译器目录（及其 x86_64-w64-mingw32/bin）前置到 PATH
    QProcessEnvironment env = QProcessEnvironment::systemEnvironment();
    QString path = env.value(QStringLiteral("PATH"));

    // 补 PATH 只用**解析后那个编译器自己**的目录，不许写死候选目录 ——
    // 把两套工具链的目录混在一起，正是上面那条报错的成因。
    QStringList extraDirs;
    if (hasDir(programPath)) {
        const QString binDir = QFileInfo(programPath).absolutePath();
        extraDirs << QDir::toNativeSeparators(binDir);
        // MinGW 的 ld.exe 通常在 <prefix>/x86_64-w64-mingw32/bin 下
        const QString triplet = QDir(binDir).absoluteFilePath(
            QStringLiteral("../x86_64-w64-mingw32/bin"));
        if (QFileInfo::exists(QDir(triplet).absolutePath()))
            extraDirs << QDir::toNativeSeparators(QDir(triplet).absolutePath());
    }

    for (const QString &dir : extraDirs) {
        if (!dir.isEmpty() && !path.contains(dir, Qt::CaseInsensitive))
            path = dir + QLatin1Char(';') + path;
    }
    env.insert(QStringLiteral("PATH"), path);
    process.setProcessEnvironment(env);

    process.start();

    if (!process.waitForStarted(10000)) {
        m_error = QStringLiteral("无法启动 %1：%2").arg(programPath, process.errorString());
        return m_error;
    }

    // 分片等待：默认的 waitForFinished(60000) 会把界面线程彻底冻住 ——
    // g++ 编译几秒、生成的词法程序再跑一会儿，累计超过 5 秒 Windows 就会把
    // 窗口标题标成「（未响应）」。这里改成每 100ms 让出一次，其间调用界面层
    // 安装的事件泵（未安装时退化为原来的纯阻塞等待，行为不变）。
    {
        QElapsedTimer timer;
        timer.start();
        const qint64 timeoutMs = 60000;
        while (process.state() != QProcess::NotRunning) {
            if (process.waitForFinished(100))
                break;
            if (m_eventPump)
                m_eventPump();
            if (timer.elapsed() > timeoutMs) {
                process.kill();
                process.waitForFinished(3000);
                m_error = QStringLiteral("%1 执行超时（60 秒）。").arg(programPath);
                return m_error;
            }
        }
    }

    exitCode = process.exitCode();
    const QByteArray output = process.readAllStandardOutput();
    // 编译器的诊断信息可能是本地编码（GBK）或 UTF-8，两种都试一遍
    QString text = QString::fromUtf8(output);
    if (text.contains(QChar::ReplacementCharacter))
        text = QString::fromLocal8Bit(output);
    return text;
}

/**
 * 功能：执行完整流水线（写源程序 → g++ 编译 → 运行 → 读回 .lex）。
 * 输入：见头文件说明。
 * 输出：结果结构。
 * 注意：
 *  1. 源程序与产物放在 programDir 下，目录不存在时自动创建；
 *  2. 编译命令形如：g++ -std=c++17 -O2 -o <exe 文件名> <cpp 文件名>
 *     —— 生成的代码只用 C 标准库，因此加 -std=c++17 也能正常编译；
 *  3. **链接器不能用含中文的绝对路径作为 -o 参数**：MinGW 的 ld.exe 收到的
 *     参数是本地 ANSI 编码（GBK），而 QProcess 按 UTF-16/UTF-8 传递，
 *     于是 ld 会报 "cannot open output file ... No such file or directory"。
 *     因此编译时把工作目录设为产物目录，只传**纯 ASCII 的相对文件名**；
 *     运行生成的可执行文件时再传绝对路径（Win32 层能正确处理中文路径）。
 *  4. 运行时把 .lex 路径作为第 2 个参数传入，这样生成的可执行文件
 *     既能被本模块驱动，也能被用户手工在命令行调用。
 */
LexBuild::Result LexBuild::run(const QString &code, const QString &identifier,
                               const QString &programDir, const QString &sourceFile,
                               bool lexLayoutOneLine)
{
    Q_UNUSED(lexLayoutOneLine);
    m_error.clear();
    Result result;

    if (code.trimmed().isEmpty()) {
        m_error = QStringLiteral("待生成的词法分析源程序为空。");
        result.compilerOutput = m_error;
        return result;
    }

    // ---- 准备目录 ----
    QDir dir(programDir);
    if (!dir.exists() && !dir.mkpath(QStringLiteral("."))) {
        m_error = QStringLiteral("无法创建输出目录：%1").arg(programDir);
        result.compilerOutput = m_error;
        return result;
    }
    // 工作目录用**短路径**（Windows 8.3 名称）：彻底避开中文路径带来的
    // 编码问题。短路径可能不存在（某些盘未启用 8.3），因此失败时回退到原路径。
    const QString workDir = FileUtil::shortPath(dir.absolutePath());

    const QString baseName = identifier + QStringLiteral("_lexer");
    const QString cppName = baseName + QStringLiteral(".cpp");
    const QString exeName = baseName + QStringLiteral(".exe");
    result.sourcePath = dir.absoluteFilePath(cppName);
    result.exePath = dir.absoluteFilePath(exeName);
    result.lexPath = dir.absoluteFilePath(identifier + QStringLiteral(".lex"));

    // ---- 第 1 步：写出源程序 ----
    if (!FileUtil::writeText(result.sourcePath, code)) {
        m_error = QStringLiteral("无法写出词法分析源程序：%1")
                      .arg(FileUtil::toNativePath(result.sourcePath));
        result.compilerOutput = m_error;
        return result;
    }
    result.generated = true;
    LOG_SUCCESS(QStringLiteral("已生成词法分析源程序：%1")
                    .arg(FileUtil::toNativePath(result.sourcePath)));

    // ---- 第 2 步：编译（只用相对文件名，避开中文路径编码问题）----
    const QStringList compileArgs = {QStringLiteral("-std=c++17"),
                                     QStringLiteral("-O2"),
                                     QStringLiteral("-o"),
                                     exeName,
                                     cppName};
    result.compilerOutput = runCommand(m_compiler, compileArgs, result.compileExitCode, workDir);
    result.compiled = (result.compileExitCode == 0);
    if (!result.compiled) {
        m_error = QStringLiteral("编译失败（返回码 %1）。").arg(result.compileExitCode);
        LOG_ERROR(QStringLiteral("g++ 编译失败：%1").arg(result.compilerOutput.left(500)));
        return result;
    }
    LOG_SUCCESS(QStringLiteral("g++ 编译成功：%1")
                    .arg(FileUtil::toNativePath(result.exePath)));

    // ---- 第 3 步：运行，产出 .lex ----
    result.runOutput = runCommand(result.exePath, {sourceFile, result.lexPath},
                                  result.runExitCode);
    result.ran = (result.runExitCode == 0);

    if (!result.ran) {
        m_error = QStringLiteral("生成的可执行文件运行失败（返回码 %1）。")
                      .arg(result.runExitCode);
        LOG_ERROR(m_error);
        return result;
    }

    // ---- 第 4 步：读回 .lex 内容 ----
    bool ok = false;
    result.lexContent = FileUtil::readText(result.lexPath, &ok);
    if (!ok) {
        m_error = QStringLiteral("运行成功但无法读回 .lex 文件：%1")
                      .arg(FileUtil::toNativePath(result.lexPath));
        LOG_ERROR(m_error);
        result.ran = false;
        return result;
    }

    LOG_SUCCESS(QStringLiteral("词法分析完成，已产出 %1（%2 行）。")
                    .arg(FileUtil::toNativePath(result.lexPath))
                    .arg(result.lexContent.count(QLatin1Char('\n'))));
    return result;
}

}  // namespace ccd
