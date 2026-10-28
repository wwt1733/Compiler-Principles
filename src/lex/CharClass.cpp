/******************************************************************************
 * (1) 版权信息 : 编译原理课程设计 - 华南师范大学计算机学院
 * (2) 文件名称 : CharClass.cpp
 *     标识符   : CharAtom / CharClassBuilder
 *     摘要     : 字符类字母表的实现：收集字符集原子 → 按成员签名做等价类划分
 *                → 命名与排序 → 生成 CharClassAlphabet。
 * (3) 版本号   : 1.0.0       作者/修改者 : <学号 姓名>   完成日期 : 2026-XX-XX
 * (4) 版本历史 : 1.0.0  2026-XX-XX  首次实现
 *****************************************************************************/

#include "lex/CharClass.h"

#include <algorithm>

#include "common/Logger.h"
#include "lex/RegexAst.h"

namespace ccd {

namespace {

/** 全字节数（取反字符全集的大小，见 AGENT.md §13.11） */
const int kByteCount = 256;

/**
 * 功能：把字符列表（每项是单字符）转成升序去重的字节列表。
 * 输入：chars —— 单字符文本列表。
 * 输出：字节值列表；多字符或空项被忽略（正常情况下不会出现）。
 * 注意：这里取的是"低 8 位"，因为正则式按**字节**而不是 Unicode 码点匹配：
 *       UTF-8 汉字的每个字节都要能单独作为自动机的输入符号。
 */
QVector<int> toBytes(const QStringList &chars)
{
    QVector<int> bytes;
    bytes.reserve(chars.size());
    for (const QString &ch : chars) {
        if (ch.isEmpty())
            continue;
        bytes.append(static_cast<int>(ch.at(0).unicode()) & 0xFF);
    }
    std::sort(bytes.begin(), bytes.end());
    bytes.erase(std::unique(bytes.begin(), bytes.end()), bytes.end());
    return bytes;
}

/** 功能：判断两个升序字节列表是否完全相同。输入：两个列表。输出：相同为 true。 */
bool sameBytes(const QVector<int> &a, const QVector<int> &b)
{
    if (a.size() != b.size())
        return false;
    for (int i = 0; i < a.size(); ++i) {
        if (a.at(i) != b.at(i))
            return false;
    }
    return true;
}

/**
 * 功能：递归遍历语法树，收集全部字符集原子。
 * 输入：ast —— 语法树；index —— 当前结点；atoms —— 输出列表（就地追加）。
 * 输出：无。
 * 注意：1. 用显式递归即可 —— 正则式深度很浅（本项目最深的不过十来层）；
 *       2. 只对 Charset 结点取字节集，其它结点（连接/选择/闭包）只是结构；
 *       3. 字节集合相同的原子会被合并：若新名字来自宏定义而旧名字不是，
 *          则用新名字替换 —— 这正是"宏名优先"的落地。
 */
void walkAtoms(const RegexAst &ast, int index, QVector<CharAtom> &atoms)
{
    if (index < 0 || index >= ast.size())
        return;

    const RegexNode &node = ast.node(index);
    if (node.kind == RegexNodeKind::Charset) {
        CharAtom atom;
        atom.bytes = toBytes(ast.resolveCharset(index));
        atom.name = node.atomName;
        atom.fromMacro = node.atomFromMacro;
        if (atom.name.isEmpty())
            atom.name = CharClassBuilder::bytesToSetText(atom.bytes);

        if (!atom.bytes.isEmpty()) {
            bool merged = false;
            for (CharAtom &existing : atoms) {
                if (!sameBytes(existing.bytes, atom.bytes))
                    continue;
                merged = true;
                // 宏名优先：同一字节集合既有宏名又有方括号/转义写法时，取宏名
                if (atom.fromMacro && !existing.fromMacro) {
                    existing.name = atom.name;
                    existing.fromMacro = true;
                }
                break;
            }
            if (!merged) {
                // 插入顺序即原子顺序；atoms.size() 在插入前等于该原子的下标，
                // 因此 order 严格递增且唯一，可直接作为"声明先后"的判据。
                atom.order = atoms.size();
                atoms.append(atom);
            }
        }
    }

    for (int child : node.children)
        walkAtoms(ast, child, atoms);
}

/**
 * 功能：把一个字节写成"字节集记号"里的单个元素。
 * 输入：byte —— 字节值。
 * 输出：可见 ASCII 记号。
 * 注意：与 visibleByte() 的区别是这里额外把 ','、'-'、'\' 写成 <NN> ——
 *       它们在本记号里分别是"项分隔符""区间连接符""转义引导符"，原样输出会有歧义。
 */
QString setItem(int byte)
{
    if (byte == ',' || byte == '-' || byte == '\\')
        return QStringLiteral("<%1>")
            .arg(QString::number(byte, 16).rightJustified(2, QLatin1Char('0')).toUpper());
    return CharClassBuilder::visibleByte(byte);
}

/** 一个"尚未命名排序"的字符类（build() 的内部中间结果） */
struct RawClass {
    QVector<int> bytes;      ///< 字节列表（升序）
    int nameAtom = -1;       ///< 与之字节集完全相等的原子下标（-1 = 没有）
    int minByte = 0;         ///< 最小字节（残留类排序用）
};

/** 功能：按 (是否宏名, 原子顺序, 最小字节) 给 sortKey，供排序。 */
QString sortKeyOf(const RawClass &cls, const QVector<CharAtom> &atoms)
{
    // rank：0 = 宏名类（先列），1 = 其它具名类，2 = 残留类（最后列）
    const int rank = (cls.nameAtom < 0) ? 2 : (atoms.at(cls.nameAtom).fromMacro ? 0 : 1);
    const int order = (cls.nameAtom < 0) ? cls.minByte : atoms.at(cls.nameAtom).order;
    return QStringLiteral("%1|%2").arg(rank).arg(order, 6, 10, QLatin1Char('0'));
}

}  // namespace

// ---------------------------------------------------------------------------
// 可见表示
// ---------------------------------------------------------------------------

/**
 * 功能：单字节 → 可见 ASCII 记号。
 * 输入：byte —— 字节值。
 * 输出：可打印字符原样返回；空格/制表/换行/回车用尖括号名字；其余写成 <NN>。
 * 注意：这是自动机标签的**唯一**字节渲染入口。自动机字母表可能覆盖全部 256 个
 *       字节值，若把 NUL、CR、0x0B 或 0x80~0xFF 原样写进 DOT，Graphviz 会报
 *       `syntax error ... scanning a quoted string` 并且整张图画不出来
 *       （AGENT.md §13.29）。
 */
QString CharClassBuilder::visibleByte(int byte)
{
    const int b = byte & 0xFF;
    switch (b) {
    case ' ':
        return QStringLiteral("<space>");
    case '\t':
        return QStringLiteral("<tab>");
    case '\n':
        return QStringLiteral("<LF>");
    case '\r':
        return QStringLiteral("<CR>");
    default:
        break;
    }
    if (b >= 0x21 && b <= 0x7E)
        return QString(QChar(static_cast<ushort>(b)));
    return QStringLiteral("<%1>")
        .arg(QString::number(b, 16).rightJustified(2, QLatin1Char('0')).toUpper());
}

/**
 * 功能：字节集合 → 紧凑可读文本。
 * 输入：bytes —— 字节列表（可乱序、可重复）。
 * 输出：形如 "0-9"、"A-Z,a-z"、"+"、"<00>-<08>,<space>" 的文本。
 * 注意：连续 3 个及以上的字节写成 "首-尾"（2 个以下逐个列出，避免 "a-b" 反而更长）。
 */
QString CharClassBuilder::bytesToSetText(const QVector<int> &bytes)
{
    QVector<int> sorted = bytes;
    std::sort(sorted.begin(), sorted.end());
    sorted.erase(std::unique(sorted.begin(), sorted.end()), sorted.end());
    if (sorted.isEmpty())
        return QStringLiteral("<>");

    QStringList items;
    int i = 0;
    while (i < sorted.size()) {
        int j = i;
        while (j + 1 < sorted.size() && sorted.at(j + 1) == sorted.at(j) + 1)
            ++j;
        const int runLength = j - i + 1;
        if (runLength >= 3) {
            items << QStringLiteral("%1-%2").arg(setItem(sorted.at(i)), setItem(sorted.at(j)));
        } else {
            for (int k = i; k <= j; ++k)
                items << setItem(sorted.at(k));
        }
        i = j + 1;
    }
    return items.join(QLatin1Char(','));
}

// ---------------------------------------------------------------------------
// 原子收集
// ---------------------------------------------------------------------------

/**
 * 功能：收集一棵语法树里的全部字符集原子。
 * 输入：ast —— 语法树。
 * 输出：atoms —— 原子列表（就地追加，字节集相同的原子会合并）。
 * 注意：调用者应按"规则在规格文件中的顺序"依次调用本函数，
 *       这样原子顺序（进而字符类的列序）才是稳定可复现的。
 */
void CharClassBuilder::collectAtoms(const RegexAst &ast, QVector<CharAtom> &atoms)
{
    if (ast.isEmpty())
        return;
    walkAtoms(ast, ast.root(), atoms);
}

// ---------------------------------------------------------------------------
// 等价类划分
// ---------------------------------------------------------------------------

/**
 * 功能：由原子列表求字符类字母表。
 * 输入：atoms —— 字符集原子列表。
 * 输出：字符类字母表。
 * 注意：1. 划分依据是"字节属于哪些原子"的**成员签名**，签名相同者必同类 ——
 *          这是唯一严格正确的合并方式（见头文件说明），近似做法会让自动机出错；
 *       2. 不属于任何原子的字节不产生字符类（它们不可能出现在任何自动机边上），
 *          byteClass 记 -1，生成的词法程序据此报"无法识别的字符"；
 *       3. 类的名字：有原子与该类字节集**完全相等** → 用该原子名（宏名优先）；
 *          否则叫 <other>（多个残留类依次 <other>/<other2>/…，保证唯一）。
 */
CharClassAlphabet CharClassBuilder::build(const QVector<CharAtom> &atoms)
{
    CharClassAlphabet table;
    if (atoms.isEmpty())
        return table;

    // ---- 1. 为每个原子建一张 256 位成员表（避免在双重循环里反复线性查找）----
    QVector<QVector<bool>> membership;
    membership.reserve(atoms.size());
    for (const CharAtom &atom : atoms) {
        QVector<bool> mask(kByteCount, false);
        for (int b : atom.bytes) {
            if (b >= 0 && b < kByteCount)
                mask[b] = true;
        }
        membership.append(mask);
    }

    // ---- 2. 按成员签名把字节分组 ----
    QMap<QString, QVector<int>> groups;  // 签名 → 字节列表（QMap 保证遍历有序）
    for (int b = 0; b < kByteCount; ++b) {
        QString signature;
        for (int a = 0; a < atoms.size(); ++a) {
            if (membership.at(a).at(b)) {
                signature += QString::number(a);
                signature += QLatin1Char(',');
            }
        }
        if (signature.isEmpty())
            continue;  // 不属于任何原子 → 不可能出现在自动机里
        groups[signature].append(b);
    }
    if (groups.isEmpty())
        return table;

    // ---- 3. 为每个类找出"字节集完全相等"的原子（用于命名）----
    QVector<RawClass> raw;
    raw.reserve(groups.size());
    for (auto it = groups.constBegin(); it != groups.constEnd(); ++it) {
        RawClass cls;
        cls.bytes = it.value();
        cls.minByte = cls.bytes.isEmpty() ? 0 : cls.bytes.first();
        for (int a = 0; a < atoms.size(); ++a) {
            if (!sameBytes(atoms.at(a).bytes, cls.bytes))
                continue;
            // 宏名优先；同为宏名或同为非宏名时取声明靠前者
            if (cls.nameAtom < 0) {
                cls.nameAtom = a;
            } else {
                const CharAtom &best = atoms.at(cls.nameAtom);
                const CharAtom &cand = atoms.at(a);
                if (cand.fromMacro && !best.fromMacro)
                    cls.nameAtom = a;
            }
        }
        raw.append(cls);
    }

    // ---- 4. 排序：宏名类 → 其它具名类 → 残留类（各自内部按声明顺序）----
    std::sort(raw.begin(), raw.end(), [&atoms](const RawClass &a, const RawClass &b) {
        const QString ka = sortKeyOf(a, atoms);
        const QString kb = sortKeyOf(b, atoms);
        if (ka != kb)
            return ka < kb;
        return a.minByte < b.minByte;
    });

    // ---- 5. 命名、编号、充实 ----
    QStringList usedNames;
    table.byteClass.fill(-1, kByteCount);
    table.classes.reserve(raw.size());

    int otherCount = 0;
    for (int k = 0; k < raw.size(); ++k) {
        const RawClass &cls = raw.at(k);
        CharClassInfo info;
        info.key = QStringLiteral("@%1").arg(k);
        info.bytes = cls.bytes;
        info.setText = bytesToSetText(cls.bytes);

        if (cls.nameAtom >= 0) {
            info.name = atoms.at(cls.nameAtom).name;
        } else {
            ++otherCount;
            info.name = (otherCount == 1) ? QStringLiteral("<other>")
                                          : QStringLiteral("<other%1>").arg(otherCount);
        }
        // 防御：名字必须唯一（正常情况下不可能重复；真重复了给出可见的区分后缀，
        // 而不是让界面/图例出现两条同名不同内容的列）
        QString unique = info.name;
        int dup = 1;
        while (usedNames.contains(unique)) {
            ++dup;
            unique = QStringLiteral("%1#%2").arg(info.name).arg(dup);
        }
        info.name = unique;
        usedNames.append(info.name);

        // 紧凑区间（供 LexCodeGen 生成区间判定）
        int i = 0;
        while (i < info.bytes.size()) {
            int j = i;
            while (j + 1 < info.bytes.size() && info.bytes.at(j + 1) == info.bytes.at(j) + 1)
                ++j;
            info.ranges.append(qMakePair(info.bytes.at(i), info.bytes.at(j)));
            i = j + 1;
        }

        for (int b : info.bytes) {
            if (b >= 0 && b < kByteCount)
                table.byteClass[b] = k;
        }
        table.classes.append(info);
    }

    LOG_INFO(QStringLiteral("字符类划分完成：%1 个原子 → %2 个字符类（覆盖 %3 个字节）。")
                 .arg(atoms.size())
                 .arg(table.classes.size())
                 .arg([&table] {
                     int n = 0;
                     for (int c : table.byteClass) {
                         if (c >= 0)
                             ++n;
                     }
                     return n;
                 }()));
    return table;
}

}  // namespace ccd
