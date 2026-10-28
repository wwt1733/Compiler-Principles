/******************************************************************************
 * (1) 版权信息 : 编译原理课程设计 - 华南师范大学计算机学院
 * (2) 文件名称 : FirstFollowPanel.h
 *     标识符   : FirstFollowPanel
 *     摘要     : "⑨ First/Follow"功能面板（任务书要求 2.3）。
 *                以表格逐行呈现每个非终结符的 First 集、Follow 集与是否可空，
 *                并附推导过程说明与"任意符号串的 First 查询"工具。
 * (3) 版本号   : 1.0.0       作者/修改者 : <学号 姓名>   完成日期 : 2026-XX-XX
 * (4) 版本历史 : 1.0.0  2026-XX-XX  首次实现（阶段 4 界面）
 *****************************************************************************/

#pragma once

#include <QString>
#include <QWidget>

#include "syntax/FirstFollow.h"
#include "syntax/Grammar.h"

class QLabel;
class QLineEdit;
class QPlainTextEdit;
class QPushButton;
class QTableWidget;

namespace ccd {

/**
 * @brief First / Follow 集面板。
 *
 * 调用注意：
 *  1. setGrammar() 注入文法后会**立即**计算一次（用户切语言时无需再点按钮），
 *     「重新计算」按钮用于同一文法下的重算（例如调试时改动数据文件后）；
 *  2. First/Follow 的计算必须作用在**已增广**的文法上，否则 Follow(开始符号)
 *     里不会出现结束符 `$`（增广产生式 S' → S 才是 `$` 的来源）。
 *     本类内部复制一份再增广，不修改调用方的文法；
 *  3. 集合一律用 `FirstFollowResult::formatSet()` 输出 —— 它内部会排序，
 *     保证同一个文法两次计算显示结果完全一致（QSet 遍历顺序不稳定的问题）。
 */
class FirstFollowPanel : public QWidget
{
    Q_OBJECT

public:
    /** @brief 构造面板 */
    explicit FirstFollowPanel(QWidget *parent = nullptr);

    /**
     * @brief 注入文法并立即计算 First/Follow。
     * @param grammar 待计算的文法（未增广也可以）。
     */
    void setGrammar(const Grammar &grammar);

    /** @brief 清空全部显示 */
    void clearAll();

    /** @brief 取最近一次的计算结果 */
    const FirstFollowResult &result() const { return m_result; }

    /** @brief 【自测用】结果表的行数（等于非终结符个数） */
    int tableRowCount() const;

    /** @brief 【自测用】摘要标签文本 */
    QString summaryText() const;

    /** @brief 【自测用】推导过程文本 */
    QString stepsText() const;

    /** @brief 【自测用】按非终结符名取其 First 集的显示文本 */
    QString firstSetText(const QString &nonterminal) const;

    /** @brief 【自测用】按非终结符名取其 Follow 集的显示文本 */
    QString followSetText(const QString &nonterminal) const;

signals:
    /** First/Follow 计算完成（供 MainWindow 写日志或驱动后续步骤） */
    void firstFollowReady();

private slots:
    /** @brief 重新计算 */
    void onRecomputeRequested();
    /** @brief 执行一次符号串查询 */
    void onQueryRequested();

private:
    /** @brief 构建界面控件与布局 */
    void buildUi();

    /** @brief 执行计算并刷新表格、推导过程与摘要 */
    void recompute();

    /** @brief 刷新结果表格 */
    void refreshTable();

    /** @brief 按名字取 First 集（不存在返回空集） */
    QSet<QString> firstOf(const QString &nonterminal) const;
    /** @brief 按名字取 Follow 集（不存在返回空集） */
    QSet<QString> followOf(const QString &nonterminal) const;

    QPushButton *m_recomputeButton = nullptr;  ///< 重新计算
    QLabel *m_summaryLabel = nullptr;          ///< 摘要
    QTableWidget *m_table = nullptr;           ///< First/Follow 结果表
    QPlainTextEdit *m_stepsView = nullptr;     ///< 推导过程
    QLineEdit *m_queryEdit = nullptr;          ///< 查询输入（符号串）
    QPushButton *m_queryButton = nullptr;      ///< 执行查询
    QLabel *m_queryResult = nullptr;           ///< 查询结果

    Grammar m_grammar;           ///< 最近一次注入的文法（未增广）
    FirstFollowResult m_result;  ///< 计算结果（作用于已增广文法）
    bool m_valid = false;        ///< 是否已成功计算
};

}  // namespace ccd
