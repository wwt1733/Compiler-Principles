/******************************************************************************
 * (1) 版权信息 : 编译原理课程设计 - 华南师范大学计算机学院
 * (2) 文件名称 : LrItemsPanel.h
 *     标识符   : LrItemsPanel
 *     摘要     : "⑩ LR(0) DFA / ⑪ LALR(1) 项目集"功能面板（任务书要求 2.4）。
 *                左侧列出全部状态（状态号、项目数、是否接受态、转移），
 *                右侧显示选中状态的项目集；LALR(1) 模式下每个项目还附带
 *                其向前看符号集合，并报告 LR(1) 合并为 LALR(1) 的状态数对比。
 * (3) 版本号   : 1.0.0       作者/修改者 : <学号 姓名>   完成日期 : 2026-XX-XX
 * (4) 版本历史 : 1.0.0  2026-XX-XX  首次实现（阶段 5 界面）
 *****************************************************************************/

#pragma once

#include <QString>
#include <QWidget>

#include "syntax/FirstFollow.h"
#include "syntax/Grammar.h"
#include "syntax/Lr1Items.h"
#include "syntax/LrItems.h"

class QComboBox;
class QLabel;
class QPlainTextEdit;
class QPushButton;
class QTableWidget;

namespace ccd {

/**
 * @brief LR(0) / LALR(1) 项目集规范族面板。
 *
 * 调用注意：
 *  1. setGrammar() 注入文法后会**立即**重新构造项目集（两种模式都算一遍），
 *     并把结果显示出来；算法全部在 `syntax/` 里完成，本类只负责展示；
 *  2. 模式切换只切换"看哪一份结果"，不重新计算 —— 两份结果在 setGrammar()
 *     时已缓存，避免用户来回切换时反复付构造代价；
 *  3. 项目文本里的圆点用 `·`（U+00B7），与 AGENT.md §4 的书写约定一致，
 *     不要写成 `.` —— 小数点与产生式里的符号容易混淆。
 */
class LrItemsPanel : public QWidget
{
    Q_OBJECT

public:
    /** @brief 构造面板（只建立控件，不载入数据） */
    explicit LrItemsPanel(QWidget *parent = nullptr);

    /**
     * @brief 注入文法并重新构造 LR(0) 与 LALR(1) 项目集。
     * @param grammar 已化简（建议已增广或由本类按需增广）的文法。
     * @note 传入**未增广**的文法也可以：本类内部会复制一份再增广，
     *       不会修改调用方的文法对象。
     */
    void setGrammar(const Grammar &grammar);

    /** @brief 清空全部显示 */
    void clearAll();

    /** @brief 当前选中的状态号（无选中为 -1） */
    int currentState() const;

    /** @brief 选择指定状态（同步表格选中行与项目显示） */
    void selectState(int state);

    /** @brief 取当前显示模式的名称（"LR(0)" 或 "LALR(1)"） */
    QString modeName() const;

    /**
     * @brief 设置模式并立即按该模式重建项目集。
     * @param index 0 = LR(0) 项目集规范族，1 = LALR(1) 项目集。
     *
     * @note 需要这个接口，是因为页签⑩（LR(0) DFA）与页签⑪（LALR(1) 项目集）
     *       必须各用一个**独立实例**：Qt 的 `addTab()` 对已经在页签栏里的控件
     *       是"移动"而不是"新增第二个页签"，所以早先"两个页签共用一个面板"的
     *       写法会让⑩的页签直接消失（页签栏里一共只有 14 个，找不到⑩）。
     *       改成两个实例后，用本接口把两者的默认模式分别定为 LR(0) 与 LALR(1)。
     */
    void setModeIndex(int index);

    /** @brief 【自测用】当前显示的项目集文本 */
    QString itemsText() const;

    /** @brief 【自测用】状态表的行数 */
    int stateRowCount() const;

    /** @brief 【自测用】摘要标签文本 */
    QString summaryText() const;

signals:
    /** 项目集构造完成（供 MainWindow 写日志或驱动后续步骤） */
    void lrAutomatonReady();

private slots:
    /** @brief 模式下拉框变化：切换显示 LR(0) 或 LALR(1) 结果 */
    void onModeChanged(int index);
    /** @brief 表格选中行变化：刷新右侧项目集显示 */
    void onStateSelectionChanged();

private:
    /** @brief 构建界面控件与布局 */
    void buildUi();

    /** @brief 用当前缓存的分析结果刷新状态表与摘要 */
    void refreshStateTable();

    /** @brief 刷新摘要标签（含 LR(1)→LALR(1) 的合并对比） */
    void updateSummary();

    /** @brief 当前是否处于 LALR(1) 模式 */
    bool isLalrMode() const;

    /** @brief 取当前模式下状态个数 */
    int activeStateCount() const;

    /** @brief 取当前模式下某状态的项目集显示文本 */
    QString activeStateItems(int state) const;

    QComboBox *m_modeCombo = nullptr;        ///< 模式：LR(0) / LALR(1)
    QPushButton *m_rebuildButton = nullptr;  ///< 重新构造
    QTableWidget *m_stateTable = nullptr;    ///< 状态列表
    QPlainTextEdit *m_itemsView = nullptr;   ///< 选中状态的项目集
    QPlainTextEdit *m_stepsView = nullptr;   ///< 构造过程说明
    QLabel *m_summaryLabel = nullptr;        ///< 摘要（状态数、合并对比）

    Grammar m_grammar;                 ///< 最近一次注入的文法（未增广）
    Lr0Automaton m_lr0;                ///< LR(0) 项目集规范族
    LalrAutomaton m_lalr;              ///< LALR(1) 项目集（同核心合并后）
    FirstFollowResult m_firstFollow;   ///< First/Follow（LALR 构造需要，也用于展示）
    bool m_valid = false;              ///< 项目集是否已成功构造
};

}  // namespace ccd
