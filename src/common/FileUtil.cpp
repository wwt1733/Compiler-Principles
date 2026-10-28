/******************************************************************************
 * (1) 版权信息 : 编译原理课程设计 - 华南师范大学计算机学院
 * (2) 文件名称 : FileUtil.cpp
 *     标识符   : FileUtil
 *     摘要     : 文本与路径工具集的实现。编码识别采用"先 UTF-8 严格解码，失败
 *                则回退 GBK"的策略，并使用启发式规则排除把 GBK 中文误判为
 *                UTF-8 的情况。
 * (3) 版本号   : 1.0.0       作者/修改者 : <学号 姓名>   完成日期 : 2026-XX-XX
 * (4) 版本历史 : 1.0.0  2026-XX-XX  首次实现
 *****************************************************************************/

#include "common/FileUtil.h"

#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QStringConverter>
#include <QTextStream>

#include <vector>

#ifdef Q_OS_WIN
#include <windows.h>
#endif

namespace ccd {

namespace {

/** UTF-8 BOM 字节序列 */
const char kBomBytes[] = {static_cast<char>(0xEF), static_cast<char>(0xBB),
                          static_cast<char>(0xBF)};

/**
 * 功能：判断一段字节能否被严格解码为 UTF-8。
 * 输入：data —— 原始字节。
 * 输出：可以严格解码为 UTF-8 时为 true。
 * 注意：不使用 QStringDecoder 的严格模式标志（该标志在不同 Qt 6 小版本中
 *       名称与可用性不一致），改用"先用宽松方式解码，再重新编码回字节并
 *       与原字节比较"的做法：若原字节不是合法 UTF-8，解码会产生替换字符，
 *       重新编码后的字节必然与原字节不同，据此完成严格性判断。
 */
bool isValidUtf8(const QByteArray &data)
{
    if (data.isEmpty())
        return true;

    const QString decoded = QString::fromUtf8(data);
    // U+FFFD 是解码失败时的替换字符，出现即说明原字节不是合法 UTF-8
    if (decoded.contains(QChar::ReplacementCharacter))
        return false;

    return decoded.toUtf8() == data;
}

/**
 * 功能：判断 UTF-8 文本中是否包含"通常由 GBK 误判产生"的特征字符。
 * 输入：text —— 已按 UTF-8 解码的文本。
 * 输出：疑似误判时为 true。
 * 注意：GBK 双字节中的部分组合恰好是合法的 UTF-8 三字节序列，会解出
 *       CJK 扩展区或私用区的生僻字。这里用"生僻字占比"做启发式判断：
 *       若出现 U+E000..U+F8FF（私用区）或 U+3400..U+4DBF（扩展 A）
 *       之外的非常用块，则判定为误判。
 */
bool looksLikeMisdecodedGbk(const QString &text)
{
    if (text.isEmpty() || text.size() > 200000)
        return false;

    int suspicious = 0;
    int cjk = 0;
    for (const QChar &ch : text) {
        const ushort u = ch.unicode();
        if ((u >= 0x4E00 && u <= 0x9FFF)) {
            ++cjk;  // 常用汉字区：正常
        } else if ((u >= 0xE000 && u <= 0xF8FF) ||      // 私用区
                   (u >= 0x3400 && u <= 0x4DBF) ||      // 扩展 A
                   (u >= 0xF900 && u <= 0xFAFF) ||      // 兼容汉字
                   (u >= 0x3000 && u <= 0x303F && u != 0x3000 && u != 0x3001)) {
            ++suspicious;
        }
    }
    if (suspicious == 0)
        return false;
    // 生僻字占比超过 20% 且数量超过 3 个，判为 GBK 误判
    return suspicious > 3 && cjk < suspicious * 4;
}

}  // namespace

// ---------------------------------------------------------------------------
// 读
// ---------------------------------------------------------------------------

/**
 * 功能：读取文本文件，自动识别 UTF-8 / GBK。
 * 输入：filePath；输出参数 ok、detectedEncoding。
 * 输出：文件内容；失败返回空串。
 * 注意：识别顺序为
 *        1) 带 BOM → UTF-8；
 *        2) 严格 UTF-8 解码通过且无 GBK 误判特征 → UTF-8；
 *        3) 其余 → GBK（任务书配套的 TINY 单词表即为此编码）。
 */
QString FileUtil::readText(const QString &filePath, bool *ok, QString *detectedEncoding)
{
    if (ok)
        *ok = false;
    if (detectedEncoding)
        detectedEncoding->clear();

    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly)) {
        return QString();
    }
    const QByteArray raw = file.readAll();
    file.close();

    if (raw.isEmpty()) {
        if (ok)
            *ok = true;
        if (detectedEncoding)
            *detectedEncoding = QStringLiteral("UTF-8");
        return QString();
    }

    // 1) 带 BOM：直接按 UTF-8 解码并跳过 BOM
    QString text;
    if (raw.startsWith(QByteArray(kBomBytes, 3))) {
        text = QString::fromUtf8(raw.mid(3));
        if (detectedEncoding)
            *detectedEncoding = QStringLiteral("UTF-8(BOM)");
    } else if (isValidUtf8(raw)) {
        text = QString::fromUtf8(raw);
        if (looksLikeMisdecodedGbk(text)) {
            // 2') 虽能通过 UTF-8 校验，但疑似 GBK 误判，改按 GBK 解码
            QStringDecoder gbk("GBK");
            text = gbk.decode(raw);
            if (detectedEncoding)
                *detectedEncoding = QStringLiteral("GBK");
        } else if (detectedEncoding) {
            *detectedEncoding = QStringLiteral("UTF-8");
        }
    } else {
        // 3) 非法 UTF-8，按 GBK 解码
        QStringDecoder gbk("GBK");
        text = gbk.decode(raw);
        if (detectedEncoding)
            *detectedEncoding = QStringLiteral("GBK");
    }

    if (ok)
        *ok = true;
    return normalizeNewlines(text);
}

// ---------------------------------------------------------------------------
// 写
// ---------------------------------------------------------------------------

/**
 * 功能：以 UTF-8 写出文本文件。
 * 输入：filePath —— 目标路径；text —— 内容；withBom —— 是否写 BOM。
 * 输出：成功为 true。
 * 注意：父目录不存在时自动创建；换行统一写为 CRLF，便于在 Windows 记事本
 *       中正常查看（这也是课程设计交付文档的常见要求）。
 */
bool FileUtil::writeText(const QString &filePath, const QString &text, bool withBom)
{
    const QFileInfo info(filePath);
    if (!info.absolutePath().isEmpty() && !ensureDir(info.absolutePath()))
        return false;

    QFile file(filePath);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate))
        return false;

    QTextStream out(&file);
    out.setEncoding(QStringConverter::Utf8);
    out.setGenerateByteOrderMark(withBom);
    out << text;
    out.flush();
    file.close();
    return true;
}

/**
 * 功能：统一换行符为 LF。
 * 输入：text —— 原始文本。
 * 输出：归一化后的文本。
 * 注意：先处理 CRLF 再处理单独的 CR，顺序不可颠倒。
 */
QString FileUtil::normalizeNewlines(const QString &text)
{
    QString s = text;
    s.replace(QStringLiteral("\r\n"), QStringLiteral("\n"));
    s.replace(QLatin1Char('\r'), QLatin1Char('\n'));
    return s;
}

/**
 * 功能：按行拆分并过滤空行与注释行。
 * 输入：text —— 文本；keepCommentLine —— 是否保留注释行。
 * 输出：有效行列表（每行已去除首尾空白）。
 * 注意：注释行指以 '#' 或 '//' 开头的行；keepCommentLine 为 true 时返回
 *       原始行（未去空白），供编辑器类界面回显使用。
 */
QStringList FileUtil::splitMeaningfulLines(const QString &text, bool keepCommentLine)
{
    const QStringList rawLines = normalizeNewlines(text).split(QLatin1Char('\n'));
    QStringList result;
    for (const QString &raw : rawLines) {
        const QString trimmed = raw.trimmed();
        if (trimmed.isEmpty())
            continue;
        const bool isComment =
            trimmed.startsWith(QLatin1Char('#')) || trimmed.startsWith(QStringLiteral("//"));
        if (isComment) {
            if (keepCommentLine)
                result.append(raw);
            continue;
        }
        result.append(keepCommentLine ? raw : trimmed);
    }
    return result;
}

/**
 * 功能：把任意文本转成可安全放进 Graphviz DOT 双引号标签的形式（保证纯 ASCII）。
 * 输入：text —— 原始文本。
 * 输出：只由可打印 ASCII 组成的文本。
 *
 * 注意 —— 这是"导出的 .dot 打不开"的直接原因，务必保证 ASCII：
 *   本项目自动机的字母表覆盖全部 256 个字节值（AGENT.md §13.11），
 *   边标签里因此含 NUL、CR、0x0B 等控制字节以及 0x80~0xFF 的高位字节。
 *   原样写进 .dot 后 Graphviz 会报
 *       syntax error ... scanning a quoted string (missing endquote?)
 *   并**整张图都画不出来**（不是"中文显示成方框"那种字体问题，而是无法解析）。
 *   尤其致命的是 NUL：Graphviz 的 C 词法器遇到它就认为字符串到此为止。
 *
 *   处理规则：
 *     · 可打印 ASCII 0x20~0x7E：原样保留；`"` → `\"`、`\` → `\\`（DOT 转义）
 *     · 其余单字节（0x00~0x1F、0x7F、0x80~0xFF）→ `<NN>` 十六进制可见表示
 *     · 超出单字节的字符（中文、全角标点等）→ `<U+XXXX>`
 *   这样输出的 .dot 与平台编码、Graphviz 版本都无关。
 */
QString FileUtil::escapeForDot(const QString &text)
{
    QString out;
    out.reserve(text.size() + 16);

    for (int i = 0; i < text.size(); ++i) {
        const ushort u = text.at(i).unicode();

        if (u == QLatin1Char('\\')) {
            out += QStringLiteral("\\\\");       // 反斜杠是 DOT 的转义引导符
        } else if (u == QLatin1Char('"')) {
            out += QStringLiteral("\\\"");       // 引号会提前结束标签
        } else if (u >= 0x20 && u <= 0x7E) {
            out += QChar(u);
        } else if (u <= 0xFF) {
            out += QStringLiteral("<%1>")
                       .arg(QString::number(u, 16).rightJustified(2, QLatin1Char('0')).toUpper());
        } else {
            out += QStringLiteral("<U+%1>")
                       .arg(QString::number(u, 16).rightJustified(4, QLatin1Char('0')).toUpper());
        }
    }
    return out;
}

// ---------------------------------------------------------------------------
// 路径
// ---------------------------------------------------------------------------

/** 功能：程序可执行文件所在目录。输入：无。输出：绝对路径（无末尾分隔符）。 */
QString FileUtil::appDir()
{
    return QCoreApplication::applicationDirPath();
}

/** 功能：随程序分发的数据目录。输入：无。输出：appDir()/data。 */
QString FileUtil::dataDir()
{
    return appDir() + QStringLiteral("/data");
}

/** 功能：构建输出目录（用于放置生成的源程序与 .lex 文件）。输入：无。输出：appDir()/output。 */
QString FileUtil::outputDir()
{
    return appDir() + QStringLiteral("/output");
}

/**
 * 功能：确保目录存在。
 * 输入：dirPath —— 目录路径。
 * 输出：成功或已存在为 true。
 * 注意：会递归创建多级目录。
 */
bool FileUtil::ensureDir(const QString &dirPath)
{
    if (dirPath.isEmpty())
        return false;
    QDir dir(dirPath);
    if (dir.exists())
        return true;
    return dir.mkpath(QStringLiteral("."));
}

/**
 * 功能：定位随附数据文件。
 * 输入：fileName —— 文件名。
 * 输出：实际存在的完整路径；找不到时返回 dataDir()/fileName。
 * 注意：查找顺序为
 *        1) appDir()/data/<fileName>          —— 构建产物旁的数据副本；
 *        2) appDir()/../../data/<fileName>    —— 从 build/<target>/release 回退到源码目录；
 *        3) appDir()/../../../data/<fileName> —— 兼容不同构建目录深度。
 */
QString FileUtil::locateDataFile(const QString &fileName)
{
    const QStringList candidates = {
        dataDir() + QLatin1Char('/') + fileName,
        appDir() + QStringLiteral("/../../data/") + fileName,
        appDir() + QStringLiteral("/../../../data/") + fileName,
        appDir() + QStringLiteral("/../data/") + fileName,
    };
    for (const QString &candidate : candidates) {
        if (QFileInfo::exists(candidate))
            return QDir::cleanPath(candidate);
    }
    return dataDir() + QLatin1Char('/') + fileName;
}

/** 功能：路径转 Windows 原生形式（便于界面显示）。输入：路径。输出：反斜杠路径。 */
QString FileUtil::toNativePath(const QString &path)
{
    return QDir::toNativeSeparators(path);
}

/**
 * 功能：取路径的 Windows 8.3 短路径形式（纯 ASCII）。
 * 输入：路径（可含中文）。
 * 输出：短路径；无法取得时返回原路径。
 * 实现：调用 GetShortPathNameW。因为本文件属于 QtCore 层，不引入 QGuiApplication，
 *       所以这里用 QDir 的 8.3 名称能力不现实（Qt 没有直接接口），
 *       只能走 Win32 API；非 Windows 平台直接返回原路径。
 * 注意：Windows 8.3 名称可能被禁用（如某些优化过的卷），此时返回原路径，
 *       调用方需要保证即使在中文路径下也能工作（本项目中编译改用工作目录方案）。
 */
QString FileUtil::shortPath(const QString &path)
{
#ifdef Q_OS_WIN
    if (path.isEmpty())
        return path;
    // 目标缓冲区：短路径不会长于原路径，留出余量即可
    const int bufferSize = path.length() + 64;
    std::vector<wchar_t> buffer(static_cast<size_t>(bufferSize), L'\0');
    const DWORD length = ::GetShortPathNameW(
        reinterpret_cast<const wchar_t *>(path.utf16()), buffer.data(), bufferSize);
    if (length > 0 && length < static_cast<DWORD>(bufferSize))
        return QString::fromWCharArray(buffer.data(), static_cast<int>(length));
    return path;
#else
    return path;
#endif
}

}  // namespace ccd
