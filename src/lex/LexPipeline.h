/******************************************************************************
 * (1) 版权信息 : 编译原理课程设计 - 华南师范大学计算机学院
 * (2) 文件名称 : LexPipeline.h
 *     标识符   : LexPipeline
 *     摘要     : 第一项任务的**完整流水线**编排（任务书要求 1.1~1.9）。
 *                把各算法模块串成一条链，供界面按钮与无界面自测**共用同一套逻辑**，
 *                避免"界面上能跑、自测里跑不通"这类不一致：
 *
 *                  单词规格文本
 *                    → TokenSpec 解析（宏、编码、L 展开、关键字/注释分类）
 *                    → RegexParser 解析各定义的正则式（含宏展开）
 *                    → RegexToNfa Thompson 构造 → NFA
 *                    → NfaToDfa 子集构造       → DFA
 *                    → DfaMinimize 划分法       → 最小化 DFA
 *                    → LexCodeGen 生成 C++ 词法分析源程序
 *                    → LexBuild 用 g++ 编译并运行 → .lex 文件
 *
 *                本文件只依赖 QtCore。
 * (3) 版本号   : 1.0.0       作者/修改者 : <学号 姓名>   完成日期 : 2026-XX-XX
 * (4) 版本历史 : 1.0.0  2026-XX-XX  首次实现
 *****************************************************************************/

#pragma once

#include <functional>
#include <QMap>
#include <QString>
#include <QStringList>

#include "common/LanguageConfig.h"
#include "common/Types.h"
#include "lex/DfaMinimize.h"
#include "lex/LexBuild.h"
#include "lex/LexCodeGen.h"
#include "lex/RegexToNfa.h"
#include "lex/TokenSpec.h"

namespace ccd {

/**
 * @brief 第一项任务的完整流水线。
 *
 * 调用注意：
 *  1. 所有中间结果都保存在结果结构中，界面各页签直接取用，
 *     因此**一次调用即可填满页签①~⑥**；
 *  2. 算法阶段失败时会在 failedStage 中标出停在哪一步，并给出 error；
 *  3. 编译运行阶段可选择跳过（例如只想看自动机与生成源码时），
 *     此时 build.generated 仍为 true，但 compiled/ran 为 false。
 */
class LexPipeline
{
public:
    /** 流水线结果 */
    struct Result {
        // ---- 阶段产物 ----
        TokenSpec spec;                 ///< 解析出的单词规格
        Nfa nfa;                        ///< Thompson 构造的 NFA
        Dfa dfa;                        ///< 子集构造的 DFA
        Dfa minDfa;                     ///< 划分法最小化后的 DFA
        QVector<NfaRule> rules;         ///< 参与自动机构造的规则
        QString postfixText;            ///< 各定义的逆波兰式对照文本
        QString postfixNotes;           ///< 自动机规模等统计说明
        QString generatedCode;          ///< 生成的词法分析源程序
        QString methodName;             ///< 所用转换方法名
        LexBuild::Result build;         ///< 编译与运行结果
        DfaMinimize minimizeDetail;     ///< 保留最小化过程（合并关系、划分轨迹）

        // ---- 状态 ----
        bool specOk = false;            ///< 规格解析成功
        bool nfaOk = false;             ///< NFA 构造成功
        bool dfaOk = false;             ///< DFA 构造成功
        bool minOk = false;             ///< 最小化成功
        bool codeOk = false;            ///< 源程序生成成功
        QString failedStage;            ///< 停在哪一步（空表示全部成功）
        QString error;                  ///< 失败原因

        /**
         * @brief 非致命告警（流程照常继续，但结果很可能不是用户想要的）。
         *
         * 目前只有一类：**关键字不可能被识别**。关键字与 ID 的正则式完全重合，
         * 因此关键字不建自动机，只能先由 ID 规则识别出标识符、再查表改判编码。
         * 若规格里没有能吃下该关键字的自动机规则（最常见的是漏了
         * `ID_101=letter(letter|digit)*`），这个关键字就永远不会出现在 .lex 里，
         * 而规格解析阶段一声不吭 —— 必须显式告警，否则极难排查。
         */
        QStringList warnings;

        /** @brief 是否已具备"生成源码"的前置条件 */
        bool readyForCode() const { return minOk; }
        /** @brief 一句话摘要，供界面与日志显示 */
        QString summary() const;
    };

    LexPipeline() = default;

    /** @brief 设置转换方法（任务书 p.6 的方法一 / p.7 的方法二） */
    void setMethod(LexCodeGen::Method method) { m_method = method; }
    /** @brief 取当前转换方法 */
    LexCodeGen::Method method() const { return m_method; }

    /** @brief 设置 .lex 输出排版 */
    void setLayout(LexCodeGen::Layout layout) { m_layout = layout; }

    /**
     * @brief 安装事件泵，透传给内部的 LexBuild。
     * @param pump 回调（可空）。
     * @note 编译与运行是阻塞等待的，不装泵会把界面冻住；
     *       泵的具体内容由界面层决定，本层只负责透传。
     */
    void setEventPump(const std::function<void()> &pump) { m_eventPump = pump; }
    /** @brief 取当前 .lex 输出排版 */
    LexCodeGen::Layout layout() const { return m_layout; }

    /**
     * @brief 执行流水线。
     * @param wordsText     单词规格文件内容。
     * @param programName   语言名（如 "TINY"），用于生成代码的文件头与文件名。
     * @param sourceFile    待词法分析的源程序路径（如 data/sample.tny）。
     * @param outputDir     产物输出目录（源码、exe、.lex 都放这里）。
     * @param identifier    产物文件名前缀（如 "tiny"）。
     * @param runBuild      true 时执行"g++ 编译 + 运行"；false 只生成源码。
     * @param compilerPath  g++ 路径；空则用 PATH 中的 "g++"。
     * @return 结果结构。
     */
    Result run(const QString &wordsText, const QString &programName, const QString &sourceFile,
               const QString &outputDir, const QString &identifier, bool runBuild,
               const QString &compilerPath = QString());

private:
    LexCodeGen::Method m_method = LexCodeGen::Method::JudgeChar;
    LexCodeGen::Layout m_layout = LexCodeGen::Layout::OnePerLine;
    std::function<void()> m_eventPump;   ///< 等待子进程期间的事件泵（由界面层安装）
};

}  // namespace ccd
