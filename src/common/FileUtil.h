/******************************************************************************
 * (1) 版权信息 : 编译原理课程设计 - 华南师范大学计算机学院
 * (2) 文件名称 : FileUtil.h
 *     标识符   : FileUtil
 *     摘要     : 文本与路径工具集。负责：以 UTF-8 优先、GBK 回退的方式读取文本
 *                文件（任务书配套的 TINY 单词表实际为 GBK 编码，必须兼容）、
 *                统一换行符、按 UTF-8 写出文本、以及应用目录/数据目录的定位。
 *                本文件不依赖 Qt GUI，算法层可直接包含。
 * (3) 版本号   : 1.0.0       作者/修改者 : <学号 姓名>   完成日期 : 2026-XX-XX
 * (4) 版本历史 : 1.0.0  2026-XX-XX  首次实现
 *****************************************************************************/

#pragma once

#include <QString>
#include <QStringList>

namespace ccd {

/**
 * @brief 文本文件读写与路径工具（全部为静态函数，无需实例化）。
 *
 * 调用注意：
 *  1. 读取一律使用 readText()，不要直接 QFile::readAll()，否则 GBK 文件会乱码；
 *  2. 写出一律使用 writeText()，保证输出为 UTF-8；
 *  3. dataDir() 依赖 QCoreApplication::applicationDirPath()，在 QApplication
 *     构造之前调用会得到空路径，必须在 main() 中先创建 QApplication。
 */
class FileUtil
{
public:
    /**
     * @brief 读取文本文件（自动识别 UTF-8 / GBK）。
     * @param filePath 文件路径。
     * @param ok       输出参数，成功为 true；可为 nullptr。
     * @param detectedEncoding 输出参数，实际使用的编码名（"UTF-8"/"GBK"）；
     *                         可为 nullptr。
     * @return 文件内容；失败返回空串（ok 为 false）。
     * @note 会去除 UTF-8 BOM，并把 CRLF/CR 统一为 LF。
     */
    static QString readText(const QString &filePath, bool *ok = nullptr,
                            QString *detectedEncoding = nullptr);

    /**
     * @brief 以 UTF-8 写出文本文件。
     * @param filePath 目标路径，父目录不存在时自动创建。
     * @param text     内容（内部使用 LF 换行）。
     * @param withBom  是否写 UTF-8 BOM（Windows 记事本查看中文时需要）。
     * @return 成功为 true。
     */
    static bool writeText(const QString &filePath, const QString &text,
                          bool withBom = false);

    /**
     * @brief 把任意换行符统一为 LF，并去除行尾多余空白。
     * @param text 原始文本。
     * @return 归一化后的文本。
     */
    static QString normalizeNewlines(const QString &text);

    /**
     * @brief 按行拆分（忽略空行、去除首尾空白、支持 '#' 与 '//' 起始的注释行）。
     * @param text           文本内容。
     * @param keepCommentLine 为 true 时保留注释行原样（用于编辑器回显）。
     * @return 有效行列表。
     */
    static QStringList splitMeaningfulLines(const QString &text,
                                            bool keepCommentLine = false);

    /**
     * @brief 把任意文本转成"可安全放进 Graphviz DOT 双引号标签"的形式。
     * @param text 原始文本（可能含引号、反斜杠、控制字节、中文等任意内容）。
     * @return **只由可打印 ASCII 组成**的文本。
     *
     * @note 为什么必须保证纯 ASCII：本项目的自动机字母表覆盖全部 256 个字节值
     *       （见 AGENT.md §13.11），边标签里于是会出现 NUL、CR、0x0B 等控制字节
     *       以及 0x80~0xFF 的高位字节。把它们原样写进 .dot 后，
     *       Graphviz 会直接报
     *       `syntax error ... scanning a quoted string (missing endquote?)`
     *       而**整张图都画不出来** —— 注意这不是"中文显示成方框"那种字体问题，
     *       而是文件根本无法解析。
     *       本函数把每一类字符都换成可见的 ASCII 表示：
     *         · 可打印 ASCII（0x20~0x7E）原样保留（`"` 与 `\` 按 DOT 规则转义）
     *         · 其余字节（控制字符、0x7F、0x80~0xFF）写成 `<NN>` 十六进制
     *         · 超出单字节的字符（如中文）写成 `<U+XXXX>`
     *       这样输出的 .dot 在任何平台、任何 Graphviz 版本上都能解析。
     */
    static QString escapeForDot(const QString &text);

    /** @brief 程序可执行文件所在目录（末尾不带分隔符） */
    static QString appDir();

    /** @brief 随程序分发的数据目录（appDir()/data） */
    static QString dataDir();

    /** @brief 构建输出目录（appDir() 的上一级，用于放置生成的源程序与 .lex） */
    static QString outputDir();

    /** @brief 确保目录存在；成功或已存在返回 true */
    static bool ensureDir(const QString &dirPath);

    /**
     * @brief 定位随附数据文件：先查 dataDir()，再查源码目录旁的 data/。
     * @param fileName 文件名（如 "tiny_words.txt"）。
     * @return 实际存在的路径；都不存在时返回 dataDir()/fileName（供报错提示）。
     */
    static QString locateDataFile(const QString &fileName);

    /** @brief 把路径统一为 Windows 反斜杠形式（用于界面显示与日志） */
    static QString toNativePath(const QString &path);

    /**
     * @brief 取路径的 Windows 8.3 短路径形式（纯 ASCII）。
     * @param path 原始路径（可含中文）。
     * @return 短路径；取不到时返回原路径。
     * @note 用途：MinGW 的 ld.exe 收到的参数是本地 ANSI 编码，
     *       若把含中文的路径作为 -o 参数会报 "cannot open output file"。
     *       短路径把各级目录名换成 8.3 名称（如 "2026-02 ~1"），
     *       从而让整个路径落在 ASCII 范围内。
     *       在未启用 8.3 名称的卷上取不到，此时调用方需另配方案。
     */
    static QString shortPath(const QString &path);
};

}  // namespace ccd
