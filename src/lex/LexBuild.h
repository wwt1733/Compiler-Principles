/******************************************************************************
 * (1) 版权信息 : 编译原理课程设计 - 华南师范大学计算机学院
 * (2) 文件名称 : LexBuild.h
 *     标识符   : LexBuild
 *     摘要     : 词法分析程序"生成 → 编译 → 运行"流水线（任务书要求 1.8、1.9）。
 *                本模块负责：
 *                  （1）把生成器产出的 C++ 源程序写到磁盘；
 *                  （2）调用 g++ 把它编译为可执行文件（通过 QProcess，
 *                       由 cmd /c 驱动，避免中文路径引号问题）；
 *                  （3）运行生成的可执行文件，对给定源程序做词法分析，
 *                       产出 .lex 文件并读回内容。
 *                本文件只依赖 QtCore（QProcess），因此可无界面自测。
 * (3) 版本号   : 1.0.0       作者/修改者 : <学号 姓名>   完成日期 : 2026-XX-XX
 * (4) 版本历史 : 1.0.0  2026-XX-XX  首次实现
 *****************************************************************************/

#pragma once

#include <QString>
#include <QStringList>
#include <functional>

namespace ccd {

/**
 * @brief 词法分析程序的编译与运行流水线。
 *
 * 调用注意：
 *  1. 所有产物统一放在 outputDir 下，输出目录不存在时自动创建；
 *  2. 编译使用 g++（默认从 PATH 中查找，也可用 setCompiler() 指定绝对路径）；
 *  3. 编译/运行的返回码、标准输出与标准错误全部保留在结果结构中，
 *     便于在界面上原样显示（这是任务书 1.9"能正确运行"的取证材料）。
 */
class LexBuild
{
public:
    /** 一次"生成 → 编译 → 运行"的完整结果 */
    struct Result {
        bool generated = false;   ///< 是否成功写出源程序
        bool compiled = false;    ///< 是否编译成功
        bool ran = false;         ///< 是否运行成功（返回码为 0）

        QString sourcePath;       ///< 生成的词法分析源程序路径
        QString exePath;          ///< 编译出的可执行文件路径
        QString lexPath;          ///< 产出的 .lex 文件路径

        QString compilerOutput;   ///< 编译器的输出（stdout + stderr）
        QString runOutput;        ///< 运行时的标准错误（词法错误信息等）
        QString lexContent;       ///< .lex 文件内容

        int compileExitCode = -1; ///< 编译返回码
        int runExitCode = -1;     ///< 运行返回码

        /** @brief 生成一句话摘要，供界面与日志显示 */
        QString summary() const;
    };

    LexBuild() = default;

    /**
     * @brief 安装"事件泵"回调，等待子进程期间由本模块周期性调用。
     *
     * @param pump 回调（可空）。界面层通常传一个
     *             `[]{ QCoreApplication::processEvents(QEventLoop::ExcludeUserInputEvents); }`。
     *
     * @note 为什么需要它：编译与运行都是用 `QProcess::waitForFinished()` 阻塞等待的，
     *       默认写法会把界面线程**完全冻住**——g++ 编译几秒、生成的词法程序再跑一会儿，
     *       累计超过 5 秒 Windows 就会给窗口标题加上「（未响应）」。
     *       装上事件泵后，等待被切成 100ms 一片，每片之间让消息队列走一次，
     *       窗口保持可重绘、可移动。
     *       用 `ExcludeUserInputEvents` 排除用户输入是**刻意**的：避免用户在
     *       上一次编译还没结束时又点一次按钮，造成重入。
     *       本模块只依赖 QtCore，不认识 QWidget —— 泵的具体内容由界面层决定。
     */
    void setEventPump(const std::function<void()> &pump) { m_eventPump = pump; }

    /**
     * @brief 设置 g++ 的路径（默认 "g++"，即从 PATH 查找）。
     * @note 可以传裸名，也可以传绝对路径；实际执行前一定会被解析成绝对路径 ——
     *       GCC 要靠 argv[0] 定位自己的 cc1plus/ld，裸名 + 改过的 PATH 会让它找错
     *       安装前缀（详见 LexBuild::runCommand 的注释）。
     */
    void setCompiler(const QString &compilerPath) { m_compiler = compilerPath; }
    /** @brief 取当前使用的编译器路径 */
    QString compiler() const { return m_compiler; }

    /**
     * @brief 执行完整流水线。
     * @param code        生成器产出的 C++ 源程序文本。
     * @param identifier 文件名前缀（如 "tiny"），最终生成 <identifier>_lexer.cpp/.exe/.lex。
     * @param programDir  产物输出目录。
     * @param sourceFile  待词法分析的源程序（如 data/sample.tny）。
     * @param lexLayoutOneLine 输出排版是否为"任务书示例（拼成一行）"。
     * @return 结果结构；失败原因通过 result.compilerOutput / runOutput 给出。
     */
    Result run(const QString &code, const QString &identifier, const QString &programDir,
               const QString &sourceFile, bool lexLayoutOneLine);

    /** @brief 最近一次错误信息 */
    QString error() const { return m_error; }

private:
    /**
     * @brief 通过 cmd /c 执行一条外部命令并收集输出。
     * @param program  可执行文件（如 g++）。
     * @param args     参数列表（内部会逐项加引号）。
     * @param exitCode 输出：返回码。
     * @param workingDir 子进程的工作目录；为空则继承本进程当前目录。
     * @return 合并后的 stdout + stderr 文本。
     * @note 用 QProcess::start 并直接传入参数列表，不经过 shell，
     *       因此中文路径与空格都无需自己拼引号（Qt 会正确转义）。
     */
    QString runCommand(const QString &program, const QStringList &args, int &exitCode,
                       const QString &workingDir = QString());

    QString m_compiler = QStringLiteral("g++");  ///< 编译器路径（裸名或绝对路径）
    QString m_error;             ///< 错误信息
    std::function<void()> m_eventPump;  ///< 等待子进程期间的事件泵（界面层安装）
};

}  // namespace ccd
