/******************************************************************************
 * (1) 版权信息 : 编译原理课程设计 - 华南师范大学计算机学院
 * (2) 文件名称 : StateTableView.h
 *     标识符   : StateTableView
 *     摘要     : 自动机状态转换表视图（可复用控件）。以表格形式呈现 NFA / DFA /
 *                最小化 DFA 的"状态转换表"，并提供导出 Graphviz DOT 文件与
 *                PNG 图片的能力，用于满足任务书"需要提供窗口以便用户查看转换
 *                得到的 NFA/DFA/最小化 DFA（用状态转换表呈现即可）"以及
 *                "生成 DFA 图"的要求。
 * (3) 版本号   : 1.0.0       作者/修改者 : <学号 姓名>   完成日期 : 2026-XX-XX
 * (4) 版本历史 : 1.0.0  2026-XX-XX  首次实现
 *****************************************************************************/

#pragma once

#include <QStringList>
#include <QWidget>

#include "common/Types.h"

class QGroupBox;
class QLabel;
class QPushButton;
class QTableWidget;

namespace ccd {

/**
 * @brief 自动机状态转换表视图。
 *
 * 表格布局（与实验报告中的状态转换表写法一致）：
 *  - 第 1 列："状态"（状态编号）
 *  - 第 2 列："说明"（是否为初态、接受态及其单词编码、对应的 NFA 状态集合）
 *  - 其后各列：字母表中的每个输入符号，单元格内容为目的状态编号（空表示无转移）
 *
 * 调用注意：
 *  1. 用 setNfa() / setDfa() 填充数据，二者会自行推断列标题；
 *  2. 导出 DOT 需要系统安装了 Graphviz 的 dot.exe，未安装时界面给出提示而不报错；
 *  3. 本控件只依赖 QtCore/QtGui/QtWidgets，不含任何算法逻辑。
 */
class StateTableView : public QWidget
{
    Q_OBJECT

public:
    explicit StateTableView(QWidget *parent = nullptr);

    /** @brief 用 DFA 数据填充表格（也适用于最小化后的 DFA） */
    void setDfa(const Dfa &dfa, const QString &caption);

    /** @brief 用 NFA 数据填充表格 */
    void setNfa(const Nfa &nfa, const QString &caption);

    /** @brief 清空表格与标题 */
    void clearAll();

    /** @brief 设置表格上方的说明文字 */
    void setCaption(const QString &caption);

    /**
     * @brief 【自测用】当前表格的列数（含 状态 / 说明 两列）。
     * @note 按字符类字母表时列数应从 258（256 字节 + 2）降到"类数 + 2"，
     *       自测据此守住"字母表真的变成字符类了"。
     */
    int columnCount() const;

    /**
     * @brief 【自测用】取字符类图例文本（无字符类字母表时为空）。
     * @note 形如 "letter = A-Z,a-z" 的逐行文本。
     */
    QString charClassLegend() const;

    /**
     * @brief ε 边的标签文本（公开是为了让自测断言认得它，改标签时断言自动跟随）。
     * @return `ε`（U+03B5）。
     * @note **这是全项目 DOT 里唯一允许出现的非 ASCII 字符**，因此它**不能**
     *       过 `FileUtil::escapeForDot()`（那会把它变成 `<U+03B5>`）。
     *       其余所有标签仍然必须是可打印 ASCII（AGENT.md §13.29）。
     *       若某台机器上 Graphviz 把 `ε` 渲染成方框，把本函数改成返回
     *       `"eps"` 即可，别处（含断言）无需改动。
     */
    static QString epsilonDotLabel();

    /**
     * @brief 【自测用】取最近一次生成的 DOT 文本。
     * @note 供自测断言「导出的 .dot 是纯可打印 ASCII、能被 Graphviz 解析」。
     *       这一点必须能自动验证：自动机字母表覆盖全部 256 个字节值时，
     *       边标签里会出现控制字节与高位字节，处理不当会让整张图都画不出来。
     */
    QString dotText() const { return m_dotText; }

signals:
    /** 状态数/接受态数等统计信息变化，供主窗口更新状态栏 */
    void statisticsChanged(const QString &summary);

private slots:
    /** @brief 导出 Graphviz DOT 文件 */
    void exportDot();
    /** @brief 调用 Graphviz 把 DOT 渲染为 PNG */
    void exportPng();

private:
    /** @brief 构建界面控件与布局 */
    void buildUi();
    /** @brief 按给定的列标题重建表格骨架 */
    void resetTable(const QStringList &headers);
    /** @brief 写入一行数据 */
    void appendRow(const QStringList &cells, bool highlight);
    /**
     * @brief DOT 用的一条边（已把同一条 (源,目的) 上的同类标签合并）。
     *
     * 为什么要单独维护一份边列表，而不是像以前那样从状态转换表里凑：
     *  - 状态转换表是"一符号一目的状态"的结构（`QMap<QString,int>`），
     *    **ε 边与"一格多目的状态"都进不去**；
     *  - 于是过去导出的 NFA 图里**一条 ε 边都没有**，而 Thompson 构造里
     *    ε 边占绝大多数 —— 图上大半状态看着是孤立的（用户反馈"怪怪的"）。
     * 现在填充数据时就把"要画的边"原样记下来，DOT 与数据一一对应。
     */
    struct DotEdge {
        int from = 0;         ///< 源状态
        int to = 0;           ///< 目的状态
        QStringList labels;   ///< 标签（字符类名；ε 边时为空）
        bool epsilon = false; ///< 是否 ε 边（决定虚线样式与 ε 标签）
    };

    /** @brief 生成 DOT 文本（依赖 m_dotEdges / m_acceptLabel / m_startState） */
    QString buildDotText() const;
    /**
     * @brief 把一条边加入 m_dotEdges：同一 (源,目的,是否ε) 上的标签会合并去重。
     * @param from    源状态。
     * @param to      目的状态。
     * @param epsilon 是否 ε 边。
     * @param label   标签（ε 边传空串）。
     */
    void addDotEdge(int from, int to, bool epsilon, const QString &label);

    /**
     * @brief 把一个字母表符号显示成列标题 / 图上的边标签。
     * @param symbol 字母表里的符号（按字符类模式是 "@0" 这样的类符号键）。
     * @return 字符类模式下返回类名（letter、digit、<other>…）；
     *         按字节模式下返回该字节的可见 ASCII 记号。
     * @note 统一入口，保证表格列标题与 DOT 边标签永远一致。
     */
    QString symbolText(const QString &symbol) const;
    /** @brief 刷新"字符类对照"图例（无字符类字母表时隐藏） */
    void updateLegend();

    QLabel *m_captionLabel = nullptr;        ///< 说明文字
    QTableWidget *m_table = nullptr;         ///< 状态转换表
    QPushButton *m_exportDotButton = nullptr;///< 导出 DOT
    QPushButton *m_exportPngButton = nullptr;///< 导出 PNG
    QLabel *m_hintLabel = nullptr;           ///< 底部提示
    QGroupBox *m_legendGroup = nullptr;      ///< "字符类对照"折叠分组
    QLabel *m_legendLabel = nullptr;         ///< 图例正文

    QStringList m_alphabet;                          ///< 字母表（列标题）
    CharClassAlphabet m_alphabetInfo;                ///< 字符类字母表（空 = 按字节模式）
    int m_stateCount = 0;                            ///< 状态数（DOT 的节点个数）
    QVector<DotEdge> m_dotEdges;                     ///< DOT 用的边列表（含 ε 边）
    QVector<QString> m_acceptLabel;                  ///< 双圈节点第二行；空串 = 非接受态
    int m_startState = 0;                            ///< 初态编号
    QString m_dotText;                               ///< 最近一次生成/导出的 DOT 文本
    QString m_lastDotPath;                           ///< 最近一次导出的 DOT 文件路径
};

}  // namespace ccd
