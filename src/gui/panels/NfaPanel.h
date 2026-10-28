/******************************************************************************
 * (1) 版权信息 : 编译原理课程设计 - 华南师范大学计算机学院
 * (2) 文件名称 : NfaPanel.h
 *     标识符   : NfaPanel
 *     摘要     : "② NFA"功能面板（任务书要求 1.4）。以状态转换表呈现在 Z 由
 *                全部单词正则式合并构造出的 NFA，并提供：
 *                  （1）"构造 NFA"按钮 —— 由当前单词规格重新构造；
 *                  （2）逆波兰式对照区 —— 显示各定义的正则式与其中间形式，
 *                       便于在实验报告中说明"正则式 → 逆波兰式 → NFA"的过程；
 *                  （3）导出 DOT / PNG（由内部的状态转换表控件提供）。
 * (3) 版本号   : 1.0.0       作者/修改者 : <学号 姓名>   完成日期 : 2026-XX-XX
 * (4) 版本历史 : 1.0.0  2026-XX-XX  首次实现
 *****************************************************************************/

#pragma once

#include <QMap>
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
 * @brief NFA 展示面板。
 *
 * 调用注意：
 *  1. 用 setNfa() 注入已构造好的 NFA，面板只负责呈现；
 *  2. setPostfixMap() 注入"定义名 → 逆波兰式"的对照表，供报告取证。
 */
class NfaPanel : public QWidget
{
    Q_OBJECT

public:
    explicit NfaPanel(QWidget *parent = nullptr);

    /** @brief 显示 NFA 状态转换表 */
    void setNfa(const Nfa &nfa, const QString &caption);

    /** @brief 设置"定义名 → 逆波兰式"对照文本 */
    void setPostfixText(const QString &text);

    /** @brief 清空全部显示内容 */
    void clearAll();

    /**
     * @brief 【自测用】取状态转换表控件生成的 DOT 文本（透传）。
     * @note 供自测验证「NFA 图上的 ε 边确实画出来了、且没有孤立状态」。
     *       NFA 图是全项目唯一带非 ASCII（ε）的 DOT，因此不能套用
     *       "纯 ASCII" 的断言，只能断言"除 ε 外均为可打印 ASCII"。
     */
    QString dotText() const;

signals:
    /** 用户点击"构造 NFA"按钮，请求由当前规格重新构造 */
    void buildRequested();

private:
    /** @brief 构建界面控件与布局 */
    void buildUi();

    QPushButton *m_buildButton = nullptr;      ///< 构造 NFA
    QLabel *m_hintLabel = nullptr;             ///< 底部提示
    QSplitter *m_splitter = nullptr;           ///< 上下分割
    StateTableView *m_stateTable = nullptr;    ///< NFA 状态转换表
    QPlainTextEdit *m_postfixView = nullptr;   ///< 逆波兰式对照区
};

}  // namespace ccd
