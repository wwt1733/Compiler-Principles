/******************************************************************************
 * (1) 版权信息 : 编译原理课程设计 - 华南师范大学计算机学院
 * (2) 文件名称 : LexTestPanel.h
 *     标识符   : LexTestPanel
 *     摘要     : 词法分析编译与测试面板（页签⑥）。
 *                显示"调用编译器生成可执行程序 → 用源程序测试 → 输出单词编码"
 *                这条链路的全部取证材料：编译器路径、源程序路径、编译器输出、
 *                运行输出以及最终的 .lex 内容。
 * (3) 版本号   : 1.0.0       作者/修改者 : <学号 姓名>   完成日期 : 2026-XX-XX
 * (4) 版本历史 : 1.0.0  2026-XX-XX  首次实现
 *****************************************************************************/

#pragma once

#include <QString>
#include <QWidget>

#include "lex/LexBuild.h"

class QComboBox;
class QLabel;
class QLineEdit;
class QPlainTextEdit;
class QPushButton;
class QTabWidget;

namespace ccd {

/**
 * @brief 编译与词法测试面板。
 *
 * 调用注意：
 *  1. 面板本身不碰算法，只收集用户输入、发出 runRequested()，
 *     再由 MainWindow 驱动 LexPipeline 并把结果回灌给 setResult()；
 *  2. 输出区按"编译器输出 / 运行输出 / .lex 内容"分成三页，
 *     其中 .lex 内容同时提供"另存"按钮，便于交付 sample.lex。
 */
class LexTestPanel : public QWidget
{
    Q_OBJECT

public:
    explicit LexTestPanel(QWidget *parent = nullptr);

    /** @brief 注入一次"编译 + 运行"的完整结果 */
    void setResult(const LexBuild::Result &result, const QString &summary);

    /** @brief 只设置提示文本（例如算法阶段失败时的说明） */
    void setHint(const QString &text);

    /** @brief 取用户填写的待测源程序路径 */
    QString sourceFile() const;

    /** @brief 设置待测源程序路径 */
    void setSourceFile(const QString &path);

    /** @brief 取用户填写的编译器路径 */
    QString compilerPath() const;

    /** @brief 取产出的 .lex 文件路径（供"打开输出目录"使用） */
    QString lexPath() const { return m_lexPath; }

    /** @brief 清空全部输出 */
    void clearAll();

signals:
    /** 用户点击"编译并测试" */
    void runRequested();
    /** 用户点击"浏览源程序"（由外部弹文件对话框更便于记忆上次目录） */
    void browseRequested();

private:
    /** @brief 构建界面控件与布局 */
    void buildUi();

    /** @brief 把当前 .lex 内容另存为用户指定的文件 */
    void saveLexToFile();

    QLineEdit *m_sourceEdit = nullptr;      ///< 待测源程序路径
    QPushButton *m_browseButton = nullptr;  ///< 浏览
    QLineEdit *m_compilerEdit = nullptr;    ///< 编译器路径
    QPushButton *m_runButton = nullptr;     ///< 编译并测试
    QLabel *m_statusLabel = nullptr;        ///< 状态摘要
    QTabWidget *m_outputTabs = nullptr;     ///< 输出分页
    QPlainTextEdit *m_compilerView = nullptr;  ///< 编译器输出
    QPlainTextEdit *m_runView = nullptr;       ///< 运行输出
    QPlainTextEdit *m_lexView = nullptr;       ///< .lex 内容
    QPushButton *m_saveLexButton = nullptr;    ///< 另存 .lex
    QString m_lexPath;                      ///< 产出的 .lex 路径
};

}  // namespace ccd
