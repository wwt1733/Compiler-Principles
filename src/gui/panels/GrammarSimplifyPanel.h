/******************************************************************************
 * (1) 版权信息 : 编译原理课程设计 - 华南师范大学计算机学院
 * (2) 文件名称 : GrammarSimplifyPanel.h
 *     标识符   : GrammarSimplifyPanel
 *     摘要     : "⑧ 文法化简结果"功能面板（任务书要求 2.2）。
 *                以四个页页分别呈现：化简后的文法全文、被删除的不可推导规则、
 *                被删除的不可达规则、以及完整的化简过程说明；
 *                另附"符号对照表"把文法里的终结符符号名与词法阶段的数字编码对应起来。
 * (3) 版本号   : 1.0.0       作者/修改者 : <学号 姓名>   完成日期 : 2026-XX-XX
 * (4) 版本历史 : 1.0.0  2026-XX-XX  首次实现（阶段 4 界面）
 *****************************************************************************/

#pragma once

#include <QString>
#include <QWidget>

#include "syntax/Grammar.h"
#include "syntax/GrammarSimplify.h"

class QLabel;
class QPlainTextEdit;
class QTableWidget;
class QTabWidget;

namespace ccd {

/**
 * @brief 文法化简结果面板。
 *
 * 调用注意：
 *  1. showSimplifyResult() 注入"原始文法 + 化简结果"，本类只负责显示；
 *     化简算法在 `syntax/GrammarSimplify` 里，由 MainWindow 调用；
 *  2. 表格与文本视图都必须允许用户复制内容 —— 实验报告要引用化简前后的
 *     产生式条数与被删规则清单，因此统一用只读但可选中复制的控件；
 *  3. 若化简未删除任何内容，界面要**明确说明这一点**，而不是留一片空白让
 *     使用者怀疑功能没跑 —— 这是"功能可用"的重要交互细节。
 */
class GrammarSimplifyPanel : public QWidget
{
    Q_OBJECT

public:
    /** @brief 构造面板 */
    explicit GrammarSimplifyPanel(QWidget *parent = nullptr);

    /**
     * @brief 注入原始文法与化简结果并刷新显示。
     * @param original 化简前的文法。
     * @param result   化简结果。
     */
    void showSimplifyResult(const Grammar &original, const SimplifyResult &result);

    /** @brief 清空全部显示 */
    void clearAll();

    /** @brief 【自测用】摘要标签文本 */
    QString summaryText() const;

    /** @brief 【自测用】"化简后的文法"页文本 */
    QString keptGrammarText() const;

    /** @brief 【自测用】化简过程页文本 */
    QString stepsText() const;

    /** @brief 【自测用】符号对照表的行数 */
    int symbolRowCount() const;

private:
    /** @brief 构建界面控件与布局 */
    void buildUi();

    /** @brief 刷新符号对照表（编码 ↔ 终结符名） */
    void refreshSymbolTable(const Grammar &grammar);

    /** @brief 把符号列表格式化为"每行一个"的文本，空列表给出说明 */
    static QString formatSymbolList(const QStringList &symbols, const QString &emptyHint);

    QLabel *m_summaryLabel = nullptr;            ///< 顶部摘要
    QTabWidget *m_tabs = nullptr;                ///< 四个展示页
    QPlainTextEdit *m_keptView = nullptr;        ///< 化简后文法
    QPlainTextEdit *m_nonProductiveView = nullptr;  ///< 删除的不可推导规则
    QPlainTextEdit *m_unreachableView = nullptr;    ///< 删除的不可达规则
    QPlainTextEdit *m_stepsView = nullptr;       ///< 化简过程
    QTableWidget *m_symbolTable = nullptr;       ///< 符号对照表
};

}  // namespace ccd
