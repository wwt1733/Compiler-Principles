/******************************************************************************
 * (1) 版权信息 : 编译原理课程设计 - 华南师范大学计算机学院
 * (2) 文件名称 : LanguageConfig.h
 *     标识符   : LanguageConfig
 *     摘要     : 测试语言（TINY / mini-C）的配置中心。集中登记每种语言的单词
 *                规格文件、BNF 文法文件、语法树结构定义文件与示例源程序，
 *                避免各功能面板各自硬编码文件路径。
 *                本文件不依赖 Qt GUI。
 * (3) 版本号   : 1.0.0       作者/修改者 : <学号 姓名>   完成日期 : 2026-XX-XX
 * (4) 版本历史 : 1.0.0  2026-XX-XX  首次实现
 *****************************************************************************/

#pragma once

#include <QMap>
#include <QString>
#include <QStringList>

namespace ccd {

/** 支持的测试语言（对应任务书第五部分：先 TINY，后 mini-C） */
enum class LanguageId {
    Tiny,    ///< TINY 语言（第一、二项任务的第一轮测试数据）
    MiniC    ///< mini-C 语言（前两项任务完成后的第二轮测试数据）
};

/**
 * @brief 一种测试语言的配置。
 *
 * 存储结构：全部为文件基名与显示名，真实路径由 FileUtil::locateDataFile() 解析。
 */
struct LanguageConfig {
    LanguageId id = LanguageId::Tiny;  ///< 语言标识
    QString key;                       ///< 程序内使用的短键（"tiny"/"minic"）
    QString displayName;               ///< 界面显示名（"TINY 语言"）
    QString wordsFile;                 ///< 单词规格（正则式）文件名
    QString grammarFile;               ///< BNF 文法文件名
    QString grammarFixedFile;          ///< 修正版文法文件名（可为空，用于对比实验）
    QString semanticsFile;             ///< 语法树结构定义文件名
    QString sampleFile;                ///< 示例源程序文件名
    QStringList keywords;              ///< 关键字列表（供生成的词法程序做查表识别）
};

/**
 * @brief 语言配置的注册表（单例，只读使用）。
 *
 * 调用注意：
 *  1. 用 get() 取配置，不要自行构造 LanguageConfig 以外的路径；
 *  2. all() 返回顺序固定为 TINY、mini-C，界面下拉框直接使用该顺序。
 */
class LanguageConfigRegistry
{
public:
    /** @brief 取指定语言的配置 */
    static const LanguageConfig &get(LanguageId id);

    /** @brief 全部语言配置（顺序：TINY、mini-C） */
    static const QVector<LanguageConfig> &all();

    /** @brief 由短键取语言标识，未知键返回 Tiny */
    static LanguageId fromKey(const QString &key);

    /**
     * @brief 取"单词编码 → 文法终结符名"的对照表。
     * @param id 语言标识。
     * @return 编码到终结符名的映射。
     *
     * **为什么需要这张表（重要设计说明）**：
     * 任务一生成的词法分析程序只输出**数字编码**（如 `207`），而任务二的 BNF
     * 文法用的是**符号名**（如 `LTEQ`）。两套命名各成体系、并不相同：
     * 例如 TINY 的 `:=` 编码 215 在文法里叫 `ASSIGN`，`<>` 编码 208 叫 `NE`。
     * 因此语法分析器读入 `.lex` 后，必须先把编码翻译成终结符名才能查分析表。
     * 这张表就是那次翻译的唯一权威来源，且**按语言分别定义**
     * （同一个编码 100 在 TINY 里是 `NUMBER`、在 mini-C 里是 `NUM`）。
     *
     * 注意：注释（TINY 102、mini-C 102）**不在**表中 —— 它不输出编码，
     * 绝不会出现在 `.lex` 里，因此不存在"编码 → 终结符名"的翻译需求。
     */
    static const QMap<int, QString> &terminalNames(LanguageId id);

    /**
     * @brief 由终结符名取编码（terminalNames() 的逆向）。
     * @param id   语言标识。
     * @param name 终结符名。
     * @return 单词编码；名字不在表中时返回 -1。
     * @note 逆表按需惰性构建并缓存，避免每次线性扫描。
     */
    static int terminalCode(LanguageId id, const QString &name);
};

}  // namespace ccd
