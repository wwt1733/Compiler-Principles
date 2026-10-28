/******************************************************************************
 * (1) 版权信息 : 编译原理课程设计 - 华南师范大学计算机学院
 * (2) 文件名称 : ParseTablePanel.h
 *     标识符   : ParseTablePanel
 *     摘要     : "⑫ LALR(1) 分析表"功能面板（任务书要求 2.5）。
 *                以表格呈现 ACTION / GOTO 分析表，单元格按动作类型着色；
 *                可切换 LALR(1)/SLR(1)/LR(0) 三种表做能力对比；
 *                冲突明细单独成页，并可按需消解策略（移进优先 / 序号小的产生式优先）
 *                重新构造，便于在报告中说明取舍依据。
 * (3) 版本号   : 1.0.0       作者/修改者 : <学号 姓名>   完成日期 : 2026-XX-XX
 * (4) 版本历史 : 1.0.0  2026-XX-XX  首次实现（阶段 5 界面）
 *****************************************************************************/

#pragma once

#include <QString>
#include <QWidget>

#include "syntax/Grammar.h"
#include "syntax/ParseTable.h"

class QCheckBox;
class QComboBox;
class QLabel;
class QPlainTextEdit;
class QPushButton;
class QTableWidget;

namespace ccd {

/**
 * @brief ACTION / GOTO 分析表面板。
 *
 * 调用注意：
 *  1. setGrammar() 注入文法后按当前表种类立即构造；切换表种类或冲突消解策略
 *     会触发**重新构造**（不是重画）—— 因为不同策略下表的填法确实不同；
 *  2. 三种表共用同一套填表代码（见 `syntax/ParseTable.cpp`），界面上切换即可
 *     得到"LR(0) 冲突最多、SLR(1) 少一些、LALR(1) 最少"的对比数据，
 *     这正是实验报告里说明"为什么必须用 LALR(1)"的实证；
 *  3. 冲突**不视为失败**：表照常可用（冲突已按策略消解），面板同时给出
 *     冲突明细，让使用者能看到问题究竟出在哪个状态的哪个符号上。
 */
class ParseTablePanel : public QWidget
{
    Q_OBJECT

public:
    /** @brief 构造面板（只建立控件，不载入数据） */
    explicit ParseTablePanel(QWidget *parent = nullptr);

    /** @brief 注入文法并按当前设置构造分析表 */
    void setGrammar(const Grammar &grammar);

    /** @brief 设置表种类（同步下拉框并重新构造） */
    void setKind(ParseTableKind kind);

    /** @brief 取当前表种类 */
    ParseTableKind kind() const;

    /** @brief 清空全部显示 */
    void clearAll();

    /** @brief 取最近一次构造出的分析表 */
    const ParseTable &table() const { return m_table; }

    /** @brief 【自测用】分析表表格的行数 */
    int tableRowCount() const;

    /** @brief 【自测用】分析表表格的列数 */
    int tableColumnCount() const;

    /** @brief 【自测用】某单元格的显示文本 */
    QString cellText(int row, int column) const;

    /** @brief 【自测用】摘要标签文本 */
    QString summaryText() const;

    /** @brief 【自测用】冲突明细文本 */
    QString conflictsText() const;

signals:
    /** 分析表构造完成 */
    void parseTableReady();

private slots:
    /** @brief 表种类变化：重新构造 */
    void onKindChanged(int index);
    /** @brief 冲突消解策略变化：重新构造 */
    void onPolicyChanged();
    /** @brief 导出分析表到文本文件 */
    void onExportRequested();
    /** @brief 重新构造 */
    void onRebuildRequested();

private:
    /** @brief 构建界面控件与布局 */
    void buildUi();

    /** @brief 执行一次构造并刷新全部显示 */
    void rebuild();

    /** @brief 按分析表刷新 ACTION/GOTO 表格 */
    void refreshTableWidget();

    /** @brief 取当前冲突消解策略 */
    ParseTableBuilder::ConflictPolicy currentPolicy() const;

    QComboBox *m_kindCombo = nullptr;          ///< 表种类：LALR(1)/SLR(1)/LR(0)
    QCheckBox *m_shiftWinsBox = nullptr;       ///< 移进优先
    QCheckBox *m_lowerProdBox = nullptr;       ///< 序号小的产生式优先
    QPushButton *m_rebuildButton = nullptr;    ///< 重新构造
    QPushButton *m_exportButton = nullptr;     ///< 导出分析表
    QLabel *m_summaryLabel = nullptr;          ///< 摘要（状态数、冲突数）
    QTableWidget *m_tableWidget = nullptr;     ///< ACTION/GOTO 表
    QPlainTextEdit *m_conflictView = nullptr;  ///< 冲突明细
    QPlainTextEdit *m_stepsView = nullptr;     ///< 构造过程

    Grammar m_grammar;   ///< 最近一次注入的文法
    ParseTable m_table;  ///< 最近一次构造出的分析表
    bool m_valid = false;  ///< 分析表是否已成功构造
};

}  // namespace ccd
