/******************************************************************************
 * (1) 版权信息 : 编译原理课程设计 - 华南师范大学计算机学院
 * (2) 文件名称 : GrammarEditorPanel.h
 *     标识符   : GrammarEditorPanel
 *     摘要     : "⑦ 文法编辑器"功能面板（任务书要求 2.1）。提供文本编辑界面用于
 *                输入/打开/保存"某一高级程序设计语言的所有语法对应的 BNF 文法"
 *                文件，并提供"载入并化简"按钮触发文法载入与化简流程。
 * (3) 版本号   : 1.0.0       作者/修改者 : <学号 姓名>   完成日期 : 2026-XX-XX
 * (4) 版本历史 : 1.0.0  2026-XX-XX  首次实现（编辑器与文件读写；文法载入留待阶段 4）
 *****************************************************************************/

#pragma once

#include <QWidget>

#include "common/LanguageConfig.h"

class QCheckBox;
class QLabel;
class QPlainTextEdit;
class QPlainTextEdit;
class QPushButton;

namespace ccd {

/**
 * @brief 文法编辑器面板。
 *
 * 信号说明：与 WordsEditorPanel 一致，面板只负责编辑与文件读写，算法触发通过
 *           信号交给 MainWindow。
 */
class GrammarEditorPanel : public QWidget
{
    Q_OBJECT

public:
    explicit GrammarEditorPanel(QWidget *parent = nullptr);

    /** @brief 载入随程序分发的指定语言 BNF 文法文件 */
    void loadLanguageGrammar(LanguageId id, bool useFixedVersion = false);

    /** @brief 取当前编辑框中的文本 */
    QString text() const;

    /** @brief 设置编辑框文本 */
    void setText(const QString &text);

    /** @brief 当前选择的语言标识 */
    LanguageId currentLanguage() const { return m_language; }

    /** @brief 是否勾选了"使用修正版文法"（仅 mini-C 有意义） */
    bool useFixedGrammar() const;

signals:
    /** 用户请求载入并化简当前编辑器中的文法 */
    void loadAndSimplifyRequested();
    /** 用户请求执行"完整语法流水线"（化简→First/Follow→LR(0)→LALR(1)表→分析→语法树） */
    void fullPipelineRequested();
    /** 语言选择发生变化 */
    void languageChanged(LanguageId id);

public slots:
    /** @brief 打开文法文件（弹出文件对话框） */
    void openFile();
    /** @brief 另存为文法文件 */
    bool saveFileAs();
    /** @brief 保存到当前文件；无当前文件时转为另存为 */
    bool saveFile();

private:
    /** @brief 构建界面控件与布局 */
    void buildUi();
    /** @brief 刷新当前文件提示标签 */
    void updateFileLabel();
    /** @brief 设置底部提示文本 */
    void setHint(const QString &text, bool isError = false);

    QPlainTextEdit *m_editor = nullptr;       ///< 文法文本编辑框
    QPushButton *m_openButton = nullptr;      ///< 打开按钮
    QPushButton *m_saveButton = nullptr;      ///< 保存按钮
    QPushButton *m_saveAsButton = nullptr;    ///< 另存为按钮
    QPushButton *m_simplifyButton = nullptr;  ///< 载入并化简按钮
    QPushButton *m_pipelineButton = nullptr;  ///< 完整语法流水线按钮
    QCheckBox *m_fixedCheckBox = nullptr;     ///< 使用修正版文法（mini-C）
    QLabel *m_fileLabel = nullptr;            ///< 当前文件提示
    QLabel *m_hintLabel = nullptr;            ///< 状态提示

    QString m_currentFile;                     ///< 当前编辑的文件路径
    LanguageId m_language = LanguageId::Tiny;  ///< 当前语言
};

}  // namespace ccd
