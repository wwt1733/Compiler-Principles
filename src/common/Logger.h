/******************************************************************************
 * (1) 版权信息 : 编译原理课程设计 - 华南师范大学计算机学院
 * (2) 文件名称 : Logger.h
 *     标识符   : Logger
 *     摘要     : 全系统统一日志器（单例）。所有算法层与界面层的运行信息都通过
 *                本类输出，既记录在内存环形缓冲区中供"日志"页签显示，也通过
 *                信号实时通知界面，并可选落盘为日志文件。
 *                使用信号槽，因此本文件需要 QObject；Q_OBJECT 要求 moc 处理，
 *                但本文件只依赖 QtCore，算法层可安全包含。
 * (3) 版本号   : 1.0.0       作者/修改者 : <学号 姓名>   完成日期 : 2026-XX-XX
 * (4) 版本历史 : 1.0.0  2026-XX-XX  首次实现
 *****************************************************************************/

#pragma once

#include <QObject>
#include <QString>
#include <QStringList>

namespace ccd {

/** 日志级别 */
enum class LogLevel {
    Debug,    ///< 调试信息（发布版本可关闭）
    Info,     ///< 一般信息
    Success,  ///< 成功完成
    Warning,  ///< 警告（可继续执行）
    Error     ///< 错误（功能失败）
};

/**
 * @brief 统一日志器（单例）。
 *
 * 存储结构：
 *  - m_history : 环形缓冲区，保存最近 kMaxHistory 条格式化后的日志文本；
 *  - m_file    : 可选的日志文件对象，未调用 setLogFile 时为空。
 *
 * 调用注意：
 *  1. 必须先调用 instance() 获取单例，禁止自行 new；
 *  2. 算法层不要直接调用 qDebug()，一律使用 LOG_INFO / LOG_ERROR 宏（见文件末尾）；
 *  3. 界面在构造时连接 messageLogged() 信号即可实时刷新日志窗口。
 */
class Logger : public QObject
{
    Q_OBJECT

public:
    /** 内存中保留的最大日志条数 */
    static const int kMaxHistory = 2000;

    /** @brief 获取单例（线程内唯一，界面与算法共用） */
    static Logger *instance();

    /** @brief 写一条日志（一般由 LOG_* 宏调用，不直接使用） */
    void log(LogLevel level, const QString &message);

    /** @brief 设置日志落盘文件；传入空串则关闭落盘 */
    void setLogFile(const QString &filePath);

    /** @brief 取全部历史日志（用于界面首次显示） */
    QStringList history() const { return m_history; }

    /** @brief 清空历史日志 */
    void clear();

    /** @brief 日志级别转显示用短名（如 "[错误]"） */
    static QString levelName(LogLevel level);

signals:
    /** 每写一条日志即发出，参数为已格式化好的整行文本 */
    void messageLogged(const QString &formattedLine);

private:
    explicit Logger(QObject *parent = nullptr);
    ~Logger() override;
    Logger(const Logger &) = delete;
    Logger &operator=(const Logger &) = delete;

    QStringList m_history;   ///< 历史日志（环形缓冲：超出上限时移除最旧一条）
    QString m_filePath;      ///< 落盘日志路径（空表示不落盘）
};

}  // namespace ccd

// ---------------------------------------------------------------------------
// 便捷宏：AGENT.md §7.6 要求禁止在业务代码里散落 qDebug()
// ---------------------------------------------------------------------------
#define LOG_DEBUG(msg)   ::ccd::Logger::instance()->log(::ccd::LogLevel::Debug, (msg))
#define LOG_INFO(msg)    ::ccd::Logger::instance()->log(::ccd::LogLevel::Info, (msg))
#define LOG_SUCCESS(msg) ::ccd::Logger::instance()->log(::ccd::LogLevel::Success, (msg))
#define LOG_WARN(msg)    ::ccd::Logger::instance()->log(::ccd::LogLevel::Warning, (msg))
#define LOG_ERROR(msg)   ::ccd::Logger::instance()->log(::ccd::LogLevel::Error, (msg))
