/******************************************************************************
 * (1) 版权信息 : 编译原理课程设计 - 华南师范大学计算机学院
 * (2) 文件名称 : IntermediateCodePanel.h
 *     标识符   : IntermediateCodePanel
 *     摘要     : "⑮ 中间代码（选做）"功能面板（任务书要求 2.11）。
 *                以表格呈现从语法树归纳翻译得到的**四元式**序列
 *                `(运算符, 运算对象1, 运算对象2, 结果)`，
 *                并在下方给出纯文本形式（便于直接粘贴进实验报告）与生成告警。
 * (3) 版本号   : 1.0.0       作者/修改者 : <学号 姓名>   完成日期 : 2026-XX-XX
 * (4) 版本历史 : 1.0.0  2026-XX-XX  首次实现（阶段 8）
 *****************************************************************************/

#pragma once

#include <QString>
#include <QWidget>

#include "syntax/IntermediateCode.h"

class QLabel;
class QPlainTextEdit;
class QPushButton;
class QTableWidget;

namespace ccd {

/**
 * @brief 四元式中间代码面板。
 *
 * 调用注意：
 *  1. 本面板**只做显示**：生成由 `syntax/IntermediateCode::generate()` 完成，
 *     结果通过 `showCode()` 推入；
 *  2. 生成的前提是**已经有一次成功的语法分析**（语法树是输入）。
 *     若树为空，面板会显示提示而不是空白，避免被误认为"功能没做"。
 */
class IntermediateCodePanel : public QWidget
{
    Q_OBJECT

public:
    /** @brief 构造面板（只建立控件，不载入数据） */
    explicit IntermediateCodePanel(QWidget *parent = nullptr);

    /** @brief 推入一次生成结果并刷新显示 */
    void showCode(const IntermediateCode &code, const QString &title);

    /** @brief 清空全部显示 */
    void clearAll();

    /** @brief 【自测用】四元式表格的行数 */
    int quadRowCount() const;

    /** @brief 【自测用】某单元格的显示文本 */
    QString quadCellText(int row, int column) const;

    /** @brief 【自测用】摘要标签文本 */
    QString summaryText() const;

    /** @brief 【自测用】纯文本形式的内容 */
    QString textContent() const;

    /** @brief 【自测用】当前持有的四元式条数（-1 表示还没有结果） */
    int quadCount() const { return m_hasCode ? m_code.count() : -1; }

signals:
    /** 用户请求导出四元式（由主窗口弹出文件对话框） */
    void exportRequested();

private slots:
    /** @brief 切换"是否显示说明列" */
    void onToggleNote(bool on);

private:
    /** @brief 构建界面控件与布局 */
    void buildUi();

    /** @brief 按 m_code 重画表格与文本 */
    void refresh();

    QLabel *m_summaryLabel = nullptr;      ///< 摘要
    QTableWidget *m_quadTable = nullptr;   ///< 四元式表
    QPlainTextEdit *m_textView = nullptr;  ///< 纯文本形式
    QPlainTextEdit *m_warnView = nullptr;  ///< 生成告警
    QPushButton *m_exportButton = nullptr; ///< 导出

    IntermediateCode m_code;   ///< 最近一次生成结果
    QString m_title;           ///< 标题（源程序名）
    bool m_hasCode = false;    ///< 是否已有结果
};

}  // namespace ccd
