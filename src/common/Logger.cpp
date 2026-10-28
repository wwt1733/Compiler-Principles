/******************************************************************************
 * (1) 版权信息 : 编译原理课程设计 - 华南师范大学计算机学院
 * (2) 文件名称 : Logger.cpp
 *     标识符   : Logger
 *     摘要     : 统一日志器的实现：格式化时间戳、级别名与消息，维护内存环形
 *                缓冲区，可选写入日志文件，并通过信号通知界面。
 * (3) 版本号   : 1.0.0       作者/修改者 : <学号 姓名>   完成日期 : 2026-XX-XX
 * (4) 版本历史 : 1.0.0  2026-XX-XX  首次实现
 *****************************************************************************/

#include "common/Logger.h"

#include <QDateTime>
#include <QFile>
#include <QTextStream>

namespace ccd {

// ---------------------------------------------------------------------------
// 单例
// ---------------------------------------------------------------------------

/**
 * 功能：获取日志器单例。
 * 输入：无。
 * 输出：指向唯一 Logger 对象的指针（生命周期同进程）。
 * 注意：使用函数内静态局部变量，C++11 起保证线程安全的初始化；
 *       日志器不参与 Qt 对象树，由析构函数在进程退出时释放。
 */
Logger *Logger::instance()
{
    static Logger s_instance;
    return &s_instance;
}

Logger::Logger(QObject *parent) : QObject(parent) {}

Logger::~Logger() = default;

// ---------------------------------------------------------------------------
// 日志写入
// ---------------------------------------------------------------------------

/**
 * 功能：写一条日志。
 * 输入：level —— 日志级别；message —— 日志正文（可含换行）。
 * 输出：无（更新内存缓冲、可选落盘、发出信号）。
 * 注意：消息中的换行会被替换为 " | "，保证一条日志在界面上始终占一行，
 *       便于日志窗口按行显示与检索。
 */
void Logger::log(LogLevel level, const QString &message)
{
    const QString stamp = QDateTime::currentDateTime().toString(QStringLiteral("HH:mm:ss.zzz"));
    QString body = message;
    body.replace(QLatin1Char('\n'), QStringLiteral(" | "));
    body.replace(QLatin1Char('\r'), QString());

    const QString line =
        QStringLiteral("[%1] %2 %3").arg(stamp, levelName(level), body);

    m_history.append(line);
    while (m_history.size() > kMaxHistory)
        m_history.removeFirst();

    // 可选落盘：以只追加方式打开，写入一行后立即关闭，避免进程异常退出丢日志
    if (!m_filePath.isEmpty()) {
        QFile file(m_filePath);
        if (file.open(QIODevice::WriteOnly | QIODevice::Append | QIODevice::Text)) {
            QTextStream out(&file);
            out.setEncoding(QStringConverter::Utf8);
            out << line << '\n';
            file.close();
        }
    }

    emit messageLogged(line);
}

/**
 * 功能：设置日志落盘文件。
 * 输入：filePath —— 目标文件路径；传空串表示关闭落盘。
 * 输出：无。
 * 注意：设置时**先把旧文件截断**。落盘的实现是"每行打开→追加→关闭"（保证进程
 *       异常退出也不丢日志），所以不先清空的话这份文件会跨运行无限增长 ——
 *       本项目实际攒到过 3.3 MB，里面几乎全是历史运行的内容。
 *       约定：日志文件只反映**本次运行**。
 */
void Logger::setLogFile(const QString &filePath)
{
    m_filePath = filePath;
    if (m_filePath.isEmpty())
        return;

    {
        QFile file(m_filePath);
        if (file.open(QIODevice::WriteOnly | QIODevice::Truncate))
            file.close();
    }

    log(LogLevel::Info,
        QStringLiteral("=== 日志开始记录，输出文件：%1 ===").arg(m_filePath));
}

/**
 * 功能：清空内存中的历史日志。
 * 输入：无。
 * 输出：无。
 * 注意：不清空已落盘文件。
 */
void Logger::clear()
{
    m_history.clear();
}

/**
 * 功能：日志级别转界面显示用的短名。
 * 输入：level —— 日志级别。
 * 输出：如 "[信息]"、"[错误]"。
 * 注意：使用等宽对齐（均为 4 个汉字宽），保证日志窗口缩进整齐。
 */
QString Logger::levelName(LogLevel level)
{
    switch (level) {
    case LogLevel::Debug:
        return QStringLiteral("[调试]");
    case LogLevel::Info:
        return QStringLiteral("[信息]");
    case LogLevel::Success:
        return QStringLiteral("[成功]");
    case LogLevel::Warning:
        return QStringLiteral("[警告]");
    case LogLevel::Error:
        return QStringLiteral("[错误]");
    }
    return QStringLiteral("[未知]");
}

}  // namespace ccd
