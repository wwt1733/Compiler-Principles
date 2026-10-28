/******************************************************************************
 * (1) 版权信息 : 编译原理课程设计 - 华南师范大学计算机学院
 * (2) 文件名称 : Types.h
 *     标识符   : types
 *     摘要     : 全系统公共数据结构定义。包括：单词规格定义（RegexDef）、
 *                自动机状态转换边（Transition）、NFA / DFA 数据结构、
 *                文法产生式（Production）、文法（Grammar）、LR 项目
 *                （LrItem / Lr1Item）、分析表动作（ParseAction）、单词
 *                （Token）、语法树结点（SyntaxNode）。
 *                本文件不依赖任何 Qt GUI 模块，只依赖 QtCore 的容器与字符串，
 *                以便算法层（lex/、syntax/）能脱离界面被自测程序直接调用。
 * (3) 版本号   : 1.0.0       作者/修改者 : <学号 姓名>   完成日期 : 2026-XX-XX
 * (4) 版本历史 : 1.0.0  2026-XX-XX  首次定义公共数据结构
 *****************************************************************************/

#pragma once

#include <QMap>
#include <QPair>
#include <QSet>
#include <QString>
#include <QStringList>
#include <QVector>

namespace ccd {

// ============================================================================
//  一、第一项任务（词法分析）相关数据结构
// ============================================================================

/**
 * @brief 单词规格的类别。
 *
 * 该分类直接来自任务书 p.2 的输入格式说明：
 *  - Definition : 形如 digit=[0-9] 的宏定义，只做文本替换，不参与自动机构造；
 *  - Keyword    : 形如 Keyword103L=INT | DOUBLE | BOOL 的关键字定义，
 *                 语义上与 ID 正则重复，故不构造自动机，只占用连续编码；
 *  - Comment    : 形如 Comment_102=//(~eoln)* 的注释定义，参与自动机，
 *                 但词法分析时识别到注释不输出单词编码；
 *  - Normal     : 其余带编码的定义，参与自动机且需输出单词编码。
 */
enum class RegexKind {
    Definition,  ///< 宏定义（digit=、letter= 等），无编码
    Normal,      ///< 普通单词，参与 NFA 构造并输出编码
    Comment,     ///< 注释，参与 NFA 构造但不输出编码
    Keyword      ///< 关键字，不参与 NFA 构造，只登记编码
};

/**
 * @brief 一条单词规格定义（对应规格文件中的一行）。
 *
 * 存储结构说明（报告书"存储结构表"用）：
 *  - name       : 定义的名称（不含下划线编码部分）
 *  - code       : 单词编码；宏定义为 -1；带 L 后缀时为起始编码
 *  - launch     : 是否有 L 后缀（该定义对应多个单词，编码从 code 起连续分配）
 *  - keywordList: launch 为真时，按顺序保存各个具体关键字文本
 *  - rawRegex   : 定义行 '=' 右侧的原始正则式文本
 *  - order      : 在规格文件中的声明顺序，用于最长匹配出现平局时裁定优先级
 *  - needsDfa   : 是否需要为之生成 DFA 图（名字中带下划线+数字即为真）
 */
struct RegexDef {
    QString name;                 ///< 定义名称
    int code = -1;                ///< 单词编码（-1 表示无编码）
    bool launch = false;          ///< 是否带 L 后缀（多单词连续编码）
    RegexKind kind = RegexKind::Normal;  ///< 定义类别
    QStringList keywordList;      ///< 关键字定义的词表（展开后每项只含 1 个）
    QString rawRegex;             ///< 原始正则式文本
    QString expandedRegex;        ///< 展开宏定义后的正则式文本
    QString tokenText;            ///< 该条目对应的字面单词（关键字/符号，供常量表使用）
    int order = 0;                ///< 声明顺序
    bool needsDfa = false;        ///< 是否需要生成 DFA 图

    /** @brief 该定义实际占用的编码（launch 时为整段连续编码） */
    QVector<int> codeList() const;
};

/**
 * @brief 一个字符类：自动机字母表中的一个输入符号。
 *
 * 背景（本项目的字母表设计）：
 *  正则式里书写的**每一处**字符集都称为一个"字符集原子"：宏引用（letter）、
 *  方括号字符集（[A-Za-z]）、转义单字符（\+）、取反项（~ \}）都算一个原子。
 *  若把全部原子一并考虑，两个字节只要属于**完全相同的原子集合**，它们在自动机
 *  任何状态下的行为都完全一样（永远落在同一组边上），于是可以合并成一个符号。
 *  这样得到的等价类就是本结构描述的一个"字符类"。
 *
 * 为什么必须用等价类、而不能"归到最具体的那个原子"：
 *  原子会交叉（例如同时存在 [a-b] 与 [b-c]）。按"最具体原子"归类会把 a 与 b
 *  并成一类，但 b 还多属于另一个原子，两者的转移可以分叉 —— 自动机直接出错。
 *
 * 字母表由此从 256 个字节降到约 20 个字符类，状态转换表与 DOT 图才可读。
 * 具体划分与命名见 lex/CharClass.h。
 */
struct CharClassInfo {
    QString key;                     ///< 自动机内部使用的符号键（"@0"、"@1" …）
    QString name;                    ///< 显示名（letter / digit / <other> / + / { …）
    QVector<int> bytes;              ///< 该类包含的全部字节值（升序）
    QVector<QPair<int, int>> ranges; ///< 紧凑闭区间表示（端点均含），供生成判定代码
    QString setText;                 ///< 紧凑文本，形如 "[0-9]"、"<00>-<08>,<space>"
};

/**
 * @brief 字符类字母表：类的有序列表 + "字节 → 类下标" 映射。
 *
 * 约定：
 *  - `classes` 的**下标就是状态转换表的列号**，因此这里只放真正出现在自动机
 *    边上的类；没有边用到的类不进列表，对应字节在 `byteClass` 里记 -1；
 *  - `byteClass` 恒为 256 项（含全部字节值），值域为 [-1, classes.size())；
 *  - `isEmpty() == true` 表示"按字节字母表"的经典模式（符号键就是单个字符），
 *    此时界面与代码生成走原有逻辑。两条路径共用同一套数据结构。
 */
struct CharClassAlphabet {
    QVector<CharClassInfo> classes;  ///< 字符类列表（下标 == 列号）
    QVector<int> byteClass;          ///< 256 项：字节值 → classes 下标；-1 表示无类

    /** @brief 是否为空（空表示"按字节字母表"模式） */
    bool isEmpty() const { return classes.isEmpty(); }

    /** @brief 字节值 → 类下标；不属于任何类时返回 -1 */
    int indexOfByte(int byte) const;
    /** @brief 字节值 → 符号键；不属于任何类时返回空串 */
    QString symbolOfByte(int byte) const;
    /** @brief 字节值 → 显示名；不属于任何类时返回空串 */
    QString nameOfByte(int byte) const;
    /** @brief 符号键 → 显示名；未找到时返回空串 */
    QString nameOfSymbol(const QString &key) const;
    /** @brief 生成"名字 = 字节集"的逐行图例文本（供界面显示，纯 ASCII） */
    QString legendText() const;
};

/**
 * @brief 自动机的一条状态转换边。
 *
 * - from / to  : 状态编号
 * - symbol     : 输入符号；空串表示 ε 边
 *                · 按字节模式（CharClassAlphabet 为空）：单个字符
 *                · 按字符类模式：类的符号键（"@0"），其字节集见 CharClassInfo
 * - isEpsilon  : 是否为 ε 边
 *
 * 存储结构：邻接表（每条边一个对象，集中存放于 QVector<Transition>）。
 */
struct Transition {
    int from = 0;             ///< 源状态编号
    int to = 0;               ///< 目的状态编号
    QString symbol;           ///< 输入符号（单字符），ε 边时为空
    bool isEpsilon = false;   ///< 是否为 ε 边

    Transition() = default;
    Transition(int f, int t, const QString &s, bool eps = false)
        : from(f), to(t), symbol(s), isEpsilon(eps) {}
};

/** @brief NFA 接受状态携带的标签：识别出的单词对应的定义与编码。 */
struct AcceptTag {
    QString defName;  ///< 所属定义名
    int code = -1;    ///< 输出编码（注释类为 -1）
    bool isComment = false;  ///< 是否注释类（不输出编码）
};

/**
 * @brief 不确定的有穷自动机（NFA）。
 *
 * 存储结构：
 *  - stateCount : 状态总数（状态编号 0..stateCount-1）
 *  - edges      : 邻接表形式的边集合
 *  - start      : 开始状态（固定为 0）
 *  - accepts    : 接受状态集合，每个接受状态可携带多个标签
 *  - alphabet   : 字母表（不含 ε）。按字符类模式时元素是类的符号键（"@0"…），
 *                 顺序即状态转换表的列序；按字节模式时元素是单个字符。
 *  - alphabetInfo: 字符类字母表（为空表示按字节模式）
 */
struct Nfa {
    int stateCount = 0;                    ///< 状态总数
    QVector<Transition> edges;             ///< 邻接表：全部边
    int start = 0;                         ///< 开始状态
    QMap<int, QVector<AcceptTag>> accepts; ///< 接受状态 → 标签列表
    QStringList alphabet;                  ///< 字母表
    CharClassAlphabet alphabetInfo;        ///< 字符类字母表（空 = 按字节模式）

    /** @brief 求状态集合 S 的 ε 闭包 */
    QSet<int> epsilonClosure(const QSet<int> &s) const;
    /** @brief 求状态集合 S 经符号 ch 转移后的状态集合（不含 ε 闭包） */
    QSet<int> move(const QSet<int> &s, const QString &ch) const;
    /** @brief 收集全部输入符号（去重后按字符序排序） */
    QStringList collectAlphabet() const;
};

/**
 * @brief 确定的有穷自动机（DFA）。
 *
 * 存储结构：
 *  - trans       : 状态转换表 trans[状态编号][符号] = 目的状态编号；-1 表示无转移
 *                  符号的含义由 alphabetInfo 决定（类符号键或单个字符）
 *  - acceptCodes : 接受状态 → 输出编码（-1 表示非接受态或注释态）
 *  - subsets     : DFA 状态 → 对应的 NFA 状态集合（子集构造法的中间结果）
 *  - alphabetInfo: 字符类字母表（为空表示按字节模式）；由 NfaToDfa 从 NFA 拷贝而来
 */
struct Dfa {
    int stateCount = 0;                    ///< 状态总数
    int start = 0;                         ///< 开始状态
    QStringList alphabet;                  ///< 字母表（列标题）
    QVector<QMap<QString, int>> trans;     ///< 状态转换表
    QVector<bool> accepting;               ///< 状态是否为接受态
    QVector<int> acceptCode;               ///< 接受状态对应的编码（-1 = 不输出编码）
    QVector<bool> acceptIsComment;         ///< 接受状态是否为注释类
    QVector<QSet<int>> subsets;            ///< DFA 状态 → NFA 状态集合
    QVector<AcceptTag> acceptTag;          ///< 接受状态对应的标签
    CharClassAlphabet alphabetInfo;        ///< 字符类字母表（空 = 按字节模式）

    Dfa() = default;
    explicit Dfa(int n) { resize(n); }

    /** @brief 重新设定状态数并清空转换表 */
    void resize(int n);
};

// ============================================================================
//  二、第二项任务（语法分析）相关数据结构
// ============================================================================

/**
 * @brief 文法的一条产生式：left -> right。
 *
 * 特殊约定：
 *  - 空串在输入文件中用 '#'（任务书 p.3）或 '@'（任务书 p.8）表示，
 *    载入时统一存为符号名 kEpsilon（"#"），right 为空表示空产生式。
 *  - id 为产生式在文法中的序号（从 0 开始），LR 项目中用它定位。
 */
struct Production {
    QString left;             ///< 产生式左部（非终结符）
    QStringList right;        ///< 产生式右部符号序列（空表示为 ε 产生式）
    int id = -1;              ///< 产生式序号

    /** @brief 是否为 ε 产生式（右部为空） */
    bool isEpsilon() const { return right.isEmpty(); }
    /** @brief 规格化文本，形如 "exp -> exp addop term" */
    QString toString() const;
};

/** @brief 空串在内部统一使用的符号名 */
extern const QString kEpsilon;

// 注意：文法容器本身（载入、符号表、增广、ε 展开）定义在 src/syntax/Grammar.h
//      的 ccd::Grammar 类里。此处只保留 Production 这一纯数据结构，
//      避免同一个名字在两处各有一个定义（那会导致 ODR 冲突与
//      "'ccd::Grammar' has no member named ..." 这类连锁编译错误）。

/**
 * @brief LR(0) 项目：产生式 + 圆点位置。
 *
 * 例如项目 A -> B · C 表示为 (id=产生式序号, dot=1)。
 */
struct LrItem {
    int prodId = 0;   ///< 产生式序号
    int dot = 0;      ///< 圆点位置（0..right.size()）

    bool operator==(const LrItem &o) const { return prodId == o.prodId && dot == o.dot; }
    bool operator<(const LrItem &o) const
    {
        return prodId != o.prodId ? prodId < o.prodId : dot < o.dot;
    }
};

/** @brief 为 LrItem 提供 qHash，便于放入 QSet */
uint qHash(const LrItem &key, uint seed = 0);

/**
 * @brief LR(1) 项目：LR(0) 项目 + 向前看符号。
 */
struct Lr1Item {
    int prodId = 0;        ///< 产生式序号
    int dot = 0;           ///< 圆点位置
    QString lookahead;     ///< 向前看符号

    bool operator==(const Lr1Item &o) const
    {
        return prodId == o.prodId && dot == o.dot && lookahead == o.lookahead;
    }
};

/** @brief 为 Lr1Item 提供 qHash，便于放入 QSet */
uint qHash(const Lr1Item &key, uint seed = 0);

/**
 * @brief LR 项目集（一个 DFA 状态）。
 */
struct LrState {
    QVector<LrItem> items;          ///< LR(0) 项目集合
    QVector<Lr1Item> lr1Items;      ///< LALR(1) 项目集合（带向前看符号）
    QMap<QString, int> gotoTable;   ///< 该状态经各文法符号转移到的状态号
};

/**
 * @brief 分析表的动作类型。
 */
enum class ActionType {
    Error,   ///< 出错
    Shift,   ///< 移进
    Reduce,  ///< 规约
    Accept   ///< 接受
};

/**
 * @brief 分析表的一个动作。
 */
struct ParseAction {
    ActionType type = ActionType::Error;  ///< 动作类型
    int value = -1;                       ///< Shift 时为状态号，Reduce 时为产生式号

    /** @brief 规格化文本：s3 / r2 / acc / error */
    QString toString() const;
};

/** @brief 一条分析表冲突记录（用于界面报告文法不是 LALR(1) 的原因） */
struct TableConflict {
    int state = 0;             ///< 冲突所在状态
    QString symbol;            ///< 冲突符号
    QString oldAction;         ///< 原动作
    QString newAction;         ///< 新动作
    QString description;       ///< 冲突说明
};

// ============================================================================
//  三、单词编码文件（.lex）相关数据结构 —— 第一项任务与第二项任务的接口
// ============================================================================

/**
 * @brief 一个单词（记号）。
 *
 * 与 .lex 文件的对应关系（格式见 AGENT.md §4.3）：
 *   <code>\t<value>\t<line>
 */
struct Token {
    int code = -1;        ///< 单词编码
    QString value;        ///< 单词值（ID 为原文，NUM 为数值，关键字/符号为 '-'）
    int line = 1;         ///< 所在行号
    QString text;         ///< 用于语法分析的符号名（ID/NUM/关键字/符号本身）
};

// ============================================================================
//  四、语法树相关数据结构
// ============================================================================

/**
 * @brief 语法树结点。
 *
 * 采用"数组 + 下标"方式存储（不用裸指针），便于复制、绘制与内存管理：
 *  - nodes 为按创建顺序排列的结点数组，下标即结点编号。
 */
struct SyntaxNode {
    QString label;                ///< 结点标签（终结符文本或语法成分名）
    QString value;                ///< 单词原文（仅叶结点有效，如 ID 的 `xXx`、NUM 的 `007`）
    QVector<int> children;        ///< 子结点下标列表（由 parent 重建，见 SyntaxTreeData::rebuildChildren）
    int parent = -1;              ///< 父结点下标（-1 表示无父结点）
    bool isTerminal = false;      ///< 是否为叶结点（终结符）
    int line = 0;                 ///< 对应的源程序行号（叶结点有效）
};

/** @brief 一棵语法树：结点数组 + 根结点下标 */
struct SyntaxTreeData {
    QVector<SyntaxNode> nodes;   ///< 全部结点
    int root = -1;               ///< 根结点下标（-1 表示空树）

    /** @brief 新建一个结点并返回其下标 */
    int addNode(const QString &label, bool isTerminal = false, int line = 0);
    /** @brief 树中结点总数 */
    int size() const { return nodes.size(); }
    /** @brief 叶结点个数 */
    int leafCount() const;

    /**
     * @brief 按 `parent` 字段重建每个结点的 `children` 列表。
     *
     * @note 为什么把"父子关系"统一记在 `parent` 上、最后再重建 `children`：
     *       语义动作是"先建子结点、后建父结点"的顺序执行的（LR 规约自底向上），
     *       建子结点时父结点还不存在，无法往 `children` 里追加。
     *       而 `parent` 可以随时回填，这正是"操作符结点包住两侧操作数"
     *       （`R0 2 &0`）所必需的：建完第 0 组的操作符结点后，
     *       把左侧操作数的 `parent` 指向它即可。
     *
     *       重建时顺带做两件防护：① 跳过非法的父子下标（越界、自己指向自己）；
     *       ② 跳过会形成环的父子关系。这样即使语义动作写错最多是"结点不可达"，
     *       绝不会让遍历/绘图陷入死循环。
     */
    void rebuildChildren();

    /**
     * @brief 从根出发能走到的结点个数。
     * @return 可达结点数；空树返回 0。
     * @note 若该值小于 size()，说明有结点没有被挂到树上（"孤岛"）。
     */
    int reachableCount() const;

    /**
     * @brief 丢弃从根不可达的结点，并重新编号（保持父子关系与根）。
     *
     * @note 为什么需要它：移进时为**每个**终结符都建了一个叶结点，但语义动作
     *       有意不把纯分隔符（`;`、`then`、`end`、`until` …）挂进树里 ——
     *       它们对语法树结构没有信息量。这些叶结点于是留在结点池里成了孤岛：
     *       遍历与绘图时看不见，却会被 `size()`、`leafCount()` 统计进去，
     *       让"叶结点数"这类指标虚高。`pruneUnreachable()` 在分析结束后把它们
     *       清掉并重新编号，使"结点池里的结点"与"树上真正有的结点"完全一致。
     *       必须在 `rebuildChildren()` 之后调用（要先有 children 才能标记可达）。
     */
    void pruneUnreachable();
};

}  // namespace ccd
