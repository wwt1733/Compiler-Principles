/******************************************************************************
 * (1) 版权信息 : 编译原理课程设计 - 华南师范大学计算机学院
 * (2) 文件名称 : SyntaxProcessPanel.h
 *     标识符   : SyntaxProcessPanel
 *     摘要     : "⑬ 语法分析过程"功能面板（任务书要求 2.6）。
 *                载入第一项任务输出的单词编码文件（`.lex`），用 LALR(1) 方法
 *                逐行显示语法分析过程：步号、状态栈、符号栈、剩余输入、当前动作；
 *                并按**顶层语句**分段，使"每条语句各自经历了哪些步骤"一目了然。
 *                分析若出错，另行列出错误明细（行号 / 出错符号 / 该状态下可接受的
 *                符号集合），便于定位是文法问题还是源程序问题。
 * (3) 版本号   : 1.0.0       作者/修改者 : <学号 姓名>   完成日期 : 2026-XX-XX
 * (4) 版本历史 : 1.0.0  2026-XX-XX  首次实现（阶段 6 界面）
 *****************************************************************************/

#pragma once

#include <QString>
#include <QWidget>

#include "syntax/LexReader.h"
#include "syntax/ParseTable.h"
#include "syntax/Parser.h"

class QLabel;
class QLineEdit;
class QPlainTextEdit;
class QPushButton;
class QSpinBox;
class QTableWidget;

namespace ccd {

/**
 * @brief 语法分析过程面板。
 *
 * 调用注意：
 *  1. 本面板**只做显示**，不承担任何算法工作：`.lex` 的读取由
 *     `syntax/LexReader` 完成，分析由 `syntax/Parser` 完成，
 *     结果通过 `showParseResult()` 一次性推给面板（见 AGENT.md §7 红线）；
 *  2. 「分析表」由外部注入（`setTable` / `setSemantics`），因为分析表的构造
 *     属于阶段 5 的职责，页签⑫已经算过一遍，这里复用即可，避免重复计算；
 *  3. 过程记录的条数可调（`maxSteps`）：TINY 样例约 300 步，全量显示便于
 *     写实验报告时截图，只看头部则利于快速核对。
 */
class SyntaxProcessPanel : public QWidget
{
    Q_OBJECT

public:
    /** @brief 构造面板（只建立控件，不载入数据） */
    explicit SyntaxProcessPanel(QWidget *parent = nullptr);

    /** @brief 注入分析表与语义动作（后续"开始分析"按钮直接用它们） */
    void setTable(const ParseTable &table);

    /** @brief 注入语义动作 */
    void setSemantics(const SemanticsActionSet &semantics);

    /**
     * @brief 回填 `.lex` 文件路径到输入框。
     * @param path 文件路径。
     * @note 由主窗口在"自动定位到词法阶段输出"后调用，让用户看得见实际用的是哪个
     *       文件，也方便直接改路径重跑（比让面板自己去猜路径更透明）。
     */
    void setLexPath(const QString &path);

    /**
     * @brief 推入一次分析结果并刷新显示。
     * @param result  分析结果（过程、错误、按语句切分的片段）。
     * @param lexPath 本次分析的 `.lex` 文件路径，仅用于标题显示。
     */
    void showParseResult(const ParseResult &result, const QString &lexPath);

    /** @brief 清空全部显示 */
    void clearAll();

    /** @brief 【自测用】过程表格的行数 */
    int stepRowCount() const;

    /** @brief 【自测用】某行的显示文本（列序：步/状态栈/符号栈/输入/动作） */
    QString stepCellText(int row, int column) const;

    /** @brief 【自测用】摘要标签文本 */
    QString summaryText() const;

    /** @brief 【自测用】错误明细文本 */
    QString errorsText() const;

signals:
    /** 用户点击「开始分析」：请求主窗口跑一次"读 .lex → 分析 → 建树" */
    void analyzeRequested(const QString &lexPath);

private slots:
    /** @brief 选择 `.lex` 文件 */
    void onBrowseRequested();
    /** @brief 开始分析 */
    void onAnalyzeRequested();
    /** @brief 导出分析过程 */
    void onExportRequested();
    /** @brief 过程显示条数变化：按新上限重画 */
    void onLimitChanged(int value);

private:
    /** @brief 构建界面控件与布局 */
    void buildUi();

    /** @brief 按 m_result 重画过程表格 */
    void refreshSteps();

    QLineEdit *m_pathEdit = nullptr;        ///< `.lex` 文件路径
    QPushButton *m_browseButton = nullptr;  ///< 浏览
    QPushButton *m_analyzeButton = nullptr; ///< 开始分析
    QPushButton *m_exportButton = nullptr;  ///< 导出过程
    QSpinBox *m_limitSpin = nullptr;        ///< 过程显示上限（0 = 全部）
    QLabel *m_summaryLabel = nullptr;       ///< 摘要
    QTableWidget *m_stepTable = nullptr;    ///< 过程表
    QPlainTextEdit *m_statementView = nullptr; ///< 按语句分段的片段
    QPlainTextEdit *m_errorView = nullptr;     ///< 错误明细

    ParseTable m_table;          ///< 分析表（外部注入）
    SemanticsActionSet m_sem;    ///< 语义动作（外部注入）
    ParseResult m_result;        ///< 最近一次分析结果
    QString m_lexPath;           ///< 最近一次分析的 .lex 路径
    bool m_hasResult = false;    ///< 是否已有结果
};

}  // namespace ccd
