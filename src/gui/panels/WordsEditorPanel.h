/******************************************************************************
 * (1) 版权信息 : 编译原理课程设计 - 华南师范大学计算机学院
 * (2) 文件名称 : WordsEditorPanel.h
 *     标识符   : WordsEditorPanel
 *     摘要     : "① 单词规格编辑器"功能面板（任务书要求 1.1 与 1.3）。提供
 *                文本编辑界面用于输入/打开/保存"某一高级程序设计语言的所有单词
 *                对应的正则表达式"文件，并提供"解析规格"按钮把文本解析为单词
 *                规格表（RegexDef 列表）供后续 NFA 构造使用。
 * (3) 版本号   : 1.0.0       作者/修改者 : <学号 姓名>   完成日期 : 2026-XX-XX
 * (4) 版本历史 : 1.0.0  2026-XX-XX  首次实现（编辑器与文件读写；规格解析留待阶段 1）
 *****************************************************************************/

#pragma once

#include <QWidget>

#include "common/LanguageConfig.h"

class QLabel;
class QPlainTextEdit;
class QPushButton;
class QTableWidget;

namespace ccd {

/**
 * @brief 单词规格编辑器面板。
 *
 * 信号说明：面板本身不实现算法，用户点击按钮后发出信号，由 MainWindow
 *           （或后续阶段的控制器）决定如何响应，从而保持界面与算法解耦。
 */
class WordsEditorPanel : public QWidget
{
    Q_OBJECT

public:
    explicit WordsEditorPanel(QWidget *parent = nullptr);

    /** @brief 载入随程序分发的指定语言单词规格文件 */
    void loadLanguageWords(LanguageId id);

    /** @brief 取当前编辑框中的文本 */
    QString text() const;

    /** @brief 设置编辑框文本 */
    void setText(const QString &text);

signals:
    /** 用户请求解析当前编辑器中的规格文本 */
    void parseRequested();
    /** 用户请求执行"完整词法流水线"（正则→NFA→DFA→最小化→生成词法程序→编译测试） */
    void fullPipelineRequested();

public slots:
    /** @brief 打开规格文件（弹出文件对话框） */
    void openFile();
    /** @brief 保存规格文件（弹出文件对话框） */
    bool saveFileAs();
    /** @brief 就近保存到当前文件；若尚无文件名则转为另存为 */
    bool saveFile();
    /** @brief 由解析结果显示规格表（供后续阶段调用） */
    void showParseResult(const QStringList &headers,
                         const QList<QStringList> &rows,
                         const QString &summary);

private:
    /** @brief 构建界面控件与布局 */
    void buildUi();
    /** @brief 更新标题栏上的当前文件名提示 */
    void updateFileLabel();
    /** @brief 在状态标签上显示一条提示信息 */
    void setHint(const QString &text, bool isError = false);

    QPlainTextEdit *m_editor = nullptr;      ///< 规格文本编辑框
    QPushButton *m_openButton = nullptr;     ///< 打开按钮
    QPushButton *m_saveButton = nullptr;     ///< 保存按钮
    QPushButton *m_saveAsButton = nullptr;   ///< 另存为按钮
    QPushButton *m_parseButton = nullptr;    ///< 解析规格按钮
    QPushButton *m_pipelineButton = nullptr; ///< 执行完整流水线按钮
    QTableWidget *m_resultTable = nullptr;   ///< 规格解析结果表
    QLabel *m_fileLabel = nullptr;           ///< 当前文件提示
    QLabel *m_hintLabel = nullptr;           ///< 状态提示

    QString m_currentFile;  ///< 当前编辑的文件路径（空表示未保存过）
};

}  // namespace ccd
