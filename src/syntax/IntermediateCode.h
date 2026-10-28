/******************************************************************************
 * (1) 版权信息 : 编译原理课程设计 - 华南师范大学计算机学院
 * (2) 文件名称 : IntermediateCode.h
 *     标识符   : IntermediateCode
 *     摘要     : 四元式中间代码生成（第二项任务 2.11，选做）。
 *                以**语法分析阶段建出的语法树**为输入，按结点标签做归纳翻译，
 *                产出 `(运算符, 运算对象1, 运算对象2, 结果)` 形式的四元式序列。
 *                本文件只依赖 QtCore。
 * (3) 版本号   : 1.0.0       作者/修改者 : <学号 姓名>   完成日期 : 2026-XX-XX
 * (4) 版本历史 : 1.0.0  2026-XX-XX  首次实现（阶段 8）
 *****************************************************************************/

#pragma once

#include <QString>
#include <QStringList>
#include <QVector>

#include "syntax/SyntaxTree.h"

namespace ccd {

/**
 * @brief 一条四元式。
 *
 * 字段含义随 `op` 变化，但四个位置的含义固定为
 * `(运算符, 运算对象1, 运算对象2, 结果)`；用不到的槽位写 `-`。
 */
struct Quadruple {
    QString op;      ///< 运算符 / 动作码（如 `:=`、`+`、`call`、`goto`、`label`）
    QString arg1;    ///< 第一个运算对象
    QString arg2;    ///< 第二个运算对象
    QString result;  ///< 结果（临时变量 / 目标变量 / 标号）
    int line = 0;    ///< 对应的源程序行号（0 表示无处可归，如函数首尾标记）
    QString note;    ///< 注释（例如"函数定义开始"），便于阅读

    /** @brief 规范写法 `(op, arg1, arg2, result)`；有空槽时补 `-` */
    QString toText() const;
};

/**
 * @brief 一次中间代码生成的结果。
 */
struct IntermediateCode {
    QVector<Quadruple> quads;   ///< 四元式序列
    QStringList warnings;       ///< 生成过程中的告警（遇到不认识的结点等）
    int tempCount = 0;          ///< 用掉的临时变量个数
    int labelCount = 0;         ///< 用掉的标号个数

    /** @brief 是否成功产出（至少一条） */
    bool isEmpty() const { return quads.isEmpty(); }
    /** @brief 四元式条数 */
    int count() const { return quads.size(); }

    /** @brief 纯文本形式（每行一条，带序号） */
    QString toText(bool withHeader = true) const;
    /** @brief 便于界面显示的表：行 = 序号 / 运算符 / 运算对象1 / 运算对象2 / 结果 / 说明 */
    void buildTable(QStringList &headers, QList<QStringList> &rows) const;
    /** @brief 一句话摘要 */
    QString summary() const;

    /**
     * @brief 从语法树生成四元式。
     * @param tree 语法树（由 `syntax/Parser` 在规约时按语义动作建出）。
     * @return 生成结果（含告警）。
     * @note 生成是**树驱动**的：用户通过数据目录下的语义动作文件（AGENT.md §10）
     *       决定"哪些语法成分在树上成结点、每个结点带哪些子结点"，
     *       结点标签决定用哪条翻译规则。
     *       因此换一份语义动作文件就会换一种中间代码，符合需求 2.11
     *       "在语义动作中生成中间代码"的本意。
     */
    static IntermediateCode generate(const SyntaxTreeData &tree);
};

}  // namespace ccd
