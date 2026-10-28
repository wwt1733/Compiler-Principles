/******************************************************************************
 * (1) 版权信息 : 编译原理课程设计 - 华南师范大学计算机学院
 * (2) 文件名称 : LexCodePanel.h
 *     标识符   : LexCodePanel
 *     摘要     : 词法分析源程序展示面板（页签⑤）。
 *                显示由最小化 DFA 生成的 C/C++ 词法分析源程序，
 *                并提供"转换方法一/方法二"与".lex 排版"两组选择，
 *                以及"重新生成""另存为"两个动作。
 * (3) 版本号   : 1.0.0       作者/修改者 : <学号 姓名>   完成日期 : 2026-XX-XX
 * (4) 版本历史 : 1.0.0  2026-XX-XX  首次实现
 *****************************************************************************/

#pragma once

#include <QString>
#include <QWidget>

#include "lex/LexCodeGen.h"

class QComboBox;
class QLabel;
class QPlainTextEdit;
class QPushButton;

namespace ccd {

/**
 * @brief 生成的词法分析源程序面板。
 *
 * 调用注意：
 *  1. setSource() 注入源码文本与摘要，界面只负责显示与保存，不做算法；
 *  2. 用户在界面上切换"方法/排版"时发出 regenerateRequested()，
 *     由 MainWindow 重新驱动 LexPipeline 生成，再回调 setSource()；
 *  3. 排版选项只影响 .lex 输出，因此两个面板共享同一套枚举。
 */
class LexCodePanel : public QWidget
{
    Q_OBJECT

public:
    explicit LexCodePanel(QWidget *parent = nullptr);

    /** @brief 注入生成的源程序文本与统计摘要 */
    void setSource(const QString &code, const QString &summary);

    /** @brief 取当前显示的源程序文本（供保存） */
    QString source() const;

    /**
     * @brief 取统计摘要 / 失败原因文本。
     * @return 与 `setSource()` 第二个参数相同的文本。
     * @note 生成失败时该文本就是失败原因，自测与日志靠它把"哪一步失败、为什么"
     *       带出来（否则界面上只显示一片空白，排查只能靠猜）。
     */
    QString summaryText() const;

    /** @brief 设置当前转换方法（同步下拉框，不触发信号） */
    void setMethod(LexCodeGen::Method method);

    /** @brief 取当前选中的转换方法 */
    LexCodeGen::Method method() const;

    /** @brief 设置当前 .lex 排版（同步下拉框，不触发信号） */
    void setLayout(LexCodeGen::Layout layout);

    /** @brief 取当前选中的 .lex 排版 */
    LexCodeGen::Layout layout() const;

    /** @brief 清空显示 */
    void clearAll();

signals:
    /** 用户改变了转换方法或排版，需要重新生成（参数为二者之一发生变化） */
    void regenerateRequested();

private:
    /** @brief 构建界面控件与布局 */
    void buildUi();

    /** @brief 保存当前源码到用户选择的文件 */
    void saveToFile();

    QComboBox *m_methodCombo = nullptr;   ///< 转换方法
    QComboBox *m_layoutCombo = nullptr;   ///< .lex 排版
    QPushButton *m_regenerateButton = nullptr;  ///< 重新生成
    QPushButton *m_saveButton = nullptr;        ///< 另存为
    QPlainTextEdit *m_codeView = nullptr;       ///< 源码显示
    QLabel *m_hintLabel = nullptr;              ///< 统计摘要
};

}  // namespace ccd
