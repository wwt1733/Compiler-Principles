/******************************************************************************
 * (1) 版权信息 : 编译原理课程设计 - 华南师范大学计算机学院
 * (2) 文件名称 : SelfTest.h
 *     标识符   : SelfTest
 *     摘要     : 无界面自测框架。按 AGENT.md §4.4 的约定，本课程设计系统的
 *                自测分两层：
 *                  （1）--console-selftest：只依赖 QtCore，不创建任何 GUI 对象，
 *                      因此不需要平台插件，可在无头环境稳定运行——这是算法与
 *                      数据正确性的**权威**回归手段；
 *                  （2）--selftest：创建 QMainWindow 做 GUI 冒烟测试，需要
 *                      offscreen 平台插件，属于可选验证。
 *                本文件属于第（1）层，只依赖 QtCore 与 FileUtil/LanguageConfig。
 * (3) 版本号   : 1.0.0       作者/修改者 : <学号 姓名>   完成日期 : 2026-XX-XX
 * (4) 版本历史 : 1.0.0  2026-XX-XX  首次实现（阶段 0：语言配置与数据文件检查）
 *****************************************************************************/

#pragma once

#include <QString>
#include <QStringList>

namespace ccd {

/**
 * @brief 自测上下文：累计断言结果并保存报告正文。
 *
 * 调用注意：一个上下文只用于一轮自测；report() 返回的文本可直接落盘。
 */
class SelfTestContext
{
public:
    /** @brief 记录一条断言 */
    void check(const QString &name, bool ok, const QString &detail = QString());

    /** @brief 追加一段无断言的说明文字（如环境信息、小节标题） */
    void note(const QString &text) { m_lines.append(text); }

    /** @brief 失败项个数 */
    int failedCount() const { return m_failed; }
    /** @brief 断言总个数 */
    int totalCount() const { return m_total; }
    /** @brief 生成报告全文 */
    QString report() const;

    /**
     * @brief 耗时报表明细（只列耗时超过阈值的步骤）。
     * @return 形如 `T6.3 计算 TINY DFA —— 12.34 秒` 的多行文本。
     * @note 用来回答"到底哪一步慢"：自测会把各算法全量算一遍，
     *       因此它的耗时报表就等于这些算法在界面上的真实耗时。
     *       没有这张表时只能靠猜，很容易去优化并不慢的地方。
     */
    QString slowStepsText() const;

private:
    int m_total = 0;      ///< 断言总数
    int m_failed = 0;     ///< 失败断言数
    QStringList m_lines;  ///< 报告正文

    /** @brief 一条慢步骤记录 */
    struct SlowStep {
        QString name;     ///< 断言名
        qint64 ms = 0;    ///< 该断言之前那段计算耗时（毫秒）
    };
    QVector<SlowStep> m_slowSteps;   ///< 耗时超过阈值的步骤
    qint64 m_lastMarkMs = 0;         ///< 上一次断言的时刻（用于算差值）
};

/**
 * @brief 执行不依赖 GUI 的自测（阶段 0 覆盖：语言配置表 + 随附数据文件）。
 * @param ctx 自测上下文。
 * @note 后续阶段在此追加各模块的断言，与 plan.md §9 的验收清单一一对应。
 */
void runConsoleSelfTest(SelfTestContext &ctx);

}  // namespace ccd
