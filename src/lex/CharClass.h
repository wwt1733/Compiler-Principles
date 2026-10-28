/******************************************************************************
 * (1) 版权信息 : 编译原理课程设计 - 华南师范大学计算机学院
 * (2) 文件名称 : CharClass.h
 *     标识符   : CharAtom / CharClassBuilder
 *     摘要     : 第一项任务的"字符类字母表"构造。
 *
 *                正则式里书写的一处字符集（宏引用 letter、方括号字符集
 *                [A-Za-z]、转义单字符 \+、取反项 ~ \} …）称为一个
 *                **字符集原子**。本模块做两件事：
 *                  （1）从语法树里收集全部原子，记下它们的字节集与显示名；
 *                  （2）按"属于完全相同的原子集合"把全部 256 个字节做
 *                       **等价类划分**，得到自动机真正使用的字母表。
 *
 *                为什么必须做等价类划分，而不是把每个字节直接当符号：
 *                  字节字母表有 256 个符号，状态转换表与 DOT 图都无法阅读
 *                  （letter 会展开成 52 个字母、~eoln 展开成 254 个符号）。
 *                为什么不能"把字节归到最具体的那个原子"：
 *                  原子之间会交叉（例如同时存在 [a-b] 与 [b-c]）。按"最具体
 *                  原子"归类会把 a 与 b 并成一类，而 b 还多属于另一个原子，
 *                  两者的转移可以分叉 —— 自动机直接出错。
 *                等价类划分是唯一既能把字母表缩小、又**严格保持语言不变**的做法：
 *                  两个字节属于完全相同的原子集合 ⟺ 它们在自动机任何状态下
 *                  的转移都相同（Thompson 构造对同一个字符集结点发的是同一对
 *                  (源,目的) 状态的平行边），因此合并绝对不会分叉。
 *
 *                本文件只依赖 QtCore。
 * (3) 版本号   : 1.0.0       作者/修改者 : <学号 姓名>   完成日期 : 2026-XX-XX
 * (4) 版本历史 : 1.0.0  2026-XX-XX  首次实现
 *****************************************************************************/

#pragma once

#include <QString>
#include <QVector>

#include "common/Types.h"

namespace ccd {

class RegexAst;

/**
 * @brief 一个"字符集原子"：正则式里书写的一处字符集。
 *
 * 调用注意：名字的优先级决定了字符类最终叫什么 —— 宏名最高（用户写
 * `letter=[A-Za-z]` 时希望看到 `letter` 而不是 `[A-Za-z]`），其次才是
 * 方括号文本/转义字符；同级之间取声明靠前者。
 */
struct CharAtom {
    QString name;            ///< 显示名（宏名 / 转义字符 / 规范的 [..] 文本）
    QVector<int> bytes;      ///< 该原子包含的字节值（升序、已去重）
    int order = 0;           ///< 出现顺序（决定字符类的列序，也用于命名平局裁定）
    bool fromMacro = false;  ///< 名字是否取自宏定义名（命名时优先级最高）
};

/**
 * @brief 字符类字母表的构造器。
 *
 * 使用流程：
 *   CharClassBuilder::collectAtoms(ast, atoms)   // 对每条规则的语法树各调一次
 *   CharClassAlphabet table = CharClassBuilder::build(atoms);
 * 然后把 table 交给 RegexToNfa 作为自动机的字母表。
 */
class CharClassBuilder
{
public:
    /**
     * @brief 收集一棵正则语法树里的全部字符集原子（递归遍历全部 Charset 结点）。
     * @param ast   语法树。
     * @param atoms 输入输出：原子列表；字节集相同的原子会被合并（保留优先级更高的名字）。
     * @note 取反（~）已经在语法树里表现为 negate 标志，resolveCharset() 会返回
     *       取反后的字节集，因此这里拿到的就是真实的字节集合。
     */
    static void collectAtoms(const RegexAst &ast, QVector<CharAtom> &atoms);

    /**
     * @brief 由原子列表求字符类字母表。
     * @param atoms 原子列表（来自 collectAtoms）。
     * @return 字符类字母表；原子为空时返回空表（表示继续使用"按字节字母表"的经典模式）。
     * @note 不属于任何原子的字节不会成为字符类（它们在任何自动机里都不可能有边），
     *       对应的 byteClass 记 -1；生成的词法程序据此报"无法识别的字符"，
     *       行为与原按字节实现完全一致。
     */
    static CharClassAlphabet build(const QVector<CharAtom> &atoms);

    /**
     * @brief 把字节集合写成紧凑的可读文本。
     * @param bytes 字节列表（无需有序、无需去重）。
     * @return 形如 "0-9"、"A-Z,a-z"、"+"、"<00>-<08>,<space>" 的文本。
     * @note 这是**显示用记号**，不是输入用的正则式语法：连续 3 个及以上的字节
     *       写成 "首-尾"，其余逐个列出，各项以 ',' 分隔；','、'-'、'\' 这三个
     *       会与记号本身冲突的字符写成 <2C>、<2D>、<5C>，因此结果无歧义。
     */
    static QString bytesToSetText(const QVector<int> &bytes);

    /**
     * @brief 把单个字节写成可见 ASCII 记号。
     * @param byte 字节值（0~255）。
     * @return 可打印字符原样返回；空格/制表/换行/回车分别写成 <space>/<tab>/<LF>/<CR>；
     *         其余写成 <NN>（两位大写十六进制）。
     * @note 自动机字母表覆盖全部 256 个字节值时，DOT 标签里绝不能出现裸的
     *       控制字节与 0x80~0xFF（会让 Graphviz 直接解析失败，见 AGENT.md §13.29），
     *       这个函数就是"字节 → 可见 ASCII"的唯一入口。
     */
    static QString visibleByte(int byte);
};

}  // namespace ccd
