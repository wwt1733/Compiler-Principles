/******************************************************************************
 * (1) 版权信息 : 编译原理课程设计 - 华南师范大学计算机学院
 * (2) 文件名称 : DfaPanel.h
 *     标识符   : DfaPanel
 *     摘要     : DFA 展示面板，同时服务两个页签：
 *                  （1）③ DFA —— 显示子集构造法得到的确定有穷自动机；
 *                  （2）④ 最小化DFA —— 显示划分法最小化后的结果，
 *                       并附"哪些原状态被合并"的对照与划分过程记录。
 * (3) 版本号   : 1.0.0       作者/修改者 : <学号 姓名>   完成日期 : 2026-XX-XX
 * (4) 版本历史 : 1.0.0  2026-XX-XX  首次实现
 *****************************************************************************/

#pragma once

#include <QString>
#include <QWidget>

#include "common/Types.h"

class QLabel;
class QPlainTextEdit;
class QPushButton;
class QSplitter;

namespace ccd {

class StateTableView;

/**
 * @brief DFA 展示面板。
 *
 * 调用注意：
 *  1. setDfa() 注入状态机数据；
 *  2. setMergeText() 注入"合并关系/划分过程"说明文本（最小化页签用）；
 *  3. 若不需要下半部分的说明区，可调用 setDetailVisible(false)。
 */
class DfaPanel : public QWidget
{
    Q_OBJECT

public:
    /**
     * @param title        页签标题
     * @param requirement  任务书要求编号（显示在标题下）
     * @param buttonText   按钮文字；为空则不创建按钮
     * @param parent       父控件
     */
    explicit DfaPanel(const QString &title, const QString &requirement,
                      const QString &buttonText, QWidget *parent = nullptr);

    /** @brief 显示 DFA 状态转换表 */
    void setDfa(const Dfa &dfa, const QString &caption);

    /** @brief 设置下半部分说明文本（如合并关系与划分过程） */
    void setDetailText(const QString &text);

    /** @brief 设置说明区标题 */
    void setDetailTitle(const QString &title);

    /** @brief 是否显示下半部分说明区 */
    void setDetailVisible(bool visible);

    /** @brief 清空全部显示内容 */
    void clearAll();

    /**
     * @brief 【自测用】取状态转换表控件生成的 DOT 文本（透传）。
     * @note 供自测/主窗口验证「导出的 .dot 能被 Graphviz 解析」。
     */
    QString dotText() const;

    /** @brief 【自测用】状态转换表的列数（含 状态/说明 两列） */
    int columnCount() const;

    /** @brief 【自测用】字符类对照图例文本（无字符类字母表时为空） */
    QString charClassLegend() const;

signals:
    /** 用户点击操作按钮 */
    void actionRequested();

private:
    /** @brief 构建界面控件与布局 */
    void buildUi();

    QString m_title;         ///< 页签标题
    QString m_requirement;   ///< 任务书要求编号
    QString m_buttonText;    ///< 按钮文字

    QPushButton *m_actionButton = nullptr;   ///< 操作按钮（可能为空）
    QLabel *m_hintLabel = nullptr;           ///< 底部统计提示
    QSplitter *m_splitter = nullptr;         ///< 上下分割
    StateTableView *m_stateTable = nullptr;  ///< DFA 状态转换表
    QPlainTextEdit *m_detailView = nullptr;  ///< 说明区（合并关系/划分过程）
    QWidget *m_detailContainer = nullptr;    ///< 说明区容器
};

}  // namespace ccd
