/******************************************************************************
 * (1) 版权信息 : 编译原理课程设计 - 华南师范大学计算机学院
 * (2) 文件名称 : mini-C 语言_lexer.cpp
 *     标识符   : mini-C 语言 语言词法分析程序
 *     摘要     : 本文件由《编译原理课程设计》系统自动生成，请勿手工修改。
 *                生成依据：由 mini-C 语言 语言全部单词正则式经
 *                正则式 → NFA（Thompson 构造）→ DFA（子集构造法）→
 *                最小化 DFA（划分法）所得的 31 状态最小化 DFA。
 *                转换方法：方法一：Judgechar/GetNext/GetToken。
 * (3) 版本号   : 1.0.0       作者/修改者 : 自动生成   完成日期 : 自动
 * (4) 版本历史 : 1.0.0  由系统生成
 *
 * 运行方式：mini-C 语言_lexer <源程序文件> [输出的 .lex 文件]
 *           省略第 2 个参数时把单词编码输出到标准输出。
 * 输出格式：<编码> <值>，每行一个单词；
 *           ID 的值带双引号，NUM 的值保留原文（含前导零），
 *           关键字与专用符号只输出编码；注释不输出任何内容；
 *           文件末尾输出一行 EOF。
 *****************************************************************************/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* 单词缓冲区容量：取足够大以容纳最长可能单词 */
#define WORD_MAX 4096

static FILE* g_out = NULL;      /* .lex 输出文件 */
static int   g_idCode = 101;     /* ID 的编码，用于判断是否需要查关键字表 */
static const int g_oneLine = 0;/* 输出排版：1=任务书示例（拼成一行） */

/* ------------------------------------------------------------------ */
/* 字面单词常量表：关键字与专用符号的“原文 → 编码”映射。              */
/* 说明：关键字与 ID 的正则式完全重合，因此不建自动机，           */
/*       而是在识别出标识符后查表改判；专用符号同理。               */
/* ------------------------------------------------------------------ */
static const char* g_keywordTable[][2] = {
    { "do", "110" },
    { "double", "107" },
    { "else", "103" },
    { "float", "106" },
    { "for", "112" },
    { "if", "104" },
    { "int", "105" },
    { "return", "108" },
    { "void", "109" },
    { "while", "111" },
};
static const int g_keywordCount = 10;

static const char* g_literalTable[][2] = {
    { "!", "227" },
    { "!=", "213" },
    { "%", "226" },
    { "(", "219" },
    { ")", "220" },
    { "*", "203" },
    { "+", "201" },
    { "++", "217" },
    { ",", "216" },
    { "-", "202" },
    { "--", "218" },
    { ".", "207" },
    { "/", "204" },
    { ";", "215" },
    { "<", "208" },
    { "<=", "209" },
    { "=", "214" },
    { "==", "212" },
    { ">", "210" },
    { ">=", "211" },
    { "[", "221" },
    { "]", "222" },
    { "^", "206" },
    { "do", "110" },
    { "double", "107" },
    { "else", "103" },
    { "float", "106" },
    { "for", "112" },
    { "if", "104" },
    { "int", "105" },
    { "return", "108" },
    { "void", "109" },
    { "while", "111" },
    { "{", "223" },
    { "}", "224" },
};
static const int g_literalCount = 35;

/* 功能：在关键字表中查找标识符，命中则返回其编码。
 * 输入：str —— 标识符原文。
 * 输出：关键字编码；不是关键字时返回 -1。                  */
static int findKeyword(const char* str) {
    for (int i = 0; i < g_keywordCount; i++) {
        if (strcmp(str, g_keywordTable[i][0]) == 0)
            return atoi(g_keywordTable[i][1]);
    }
    return -1;
}

/* 功能：在字面单词表中查找符号原文，命中则返回其编码。
 * 输入：str —— 符号原文。
 * 输出：符号编码；未命中时返回 -1。                  */
static int findLiteral(const char* str) {
    for (int i = 0; i < g_literalCount; i++) {
        if (strcmp(str, g_literalTable[i][0]) == 0)
            return atoi(g_literalTable[i][1]);
    }
    return -1;
}

/* ------------------------------------------------------------------ */
/* 转换方法一：Judgechar / GetNext / GetToken 三函数式。              */
/*   Judgechar(c) —— 判断下一个字符能否使自动机继续前进；            */
/*   GetNext()    —— 取下一个字符，累积到当前单词缓冲区；            */
/*   GetToken()   —— 驱动自动机跑完一个单词并回退到最近接受位置。     */
/* ------------------------------------------------------------------ */

static const int S0 = 0;
static const int S1 = 1;
static const int S2 = 2;
static const int S3 = 3;
static const int S4 = 4;
static const int S5 = 5;
static const int S6 = 6;
static const int S7 = 7;
static const int S8 = 8;
static const int S9 = 9;
static const int S10 = 10;
static const int S11 = 11;
static const int S12 = 12;
static const int S13 = 13;
static const int S14 = 14;
static const int S15 = 15;
static const int S16 = 16;
static const int S17 = 17;
static const int S18 = 18;
static const int S19 = 19;
static const int S20 = 20;
static const int S21 = 21;
static const int S22 = 22;
static const int S23 = 23;
static const int S24 = 24;
static const int S25 = 25;
static const int S26 = 26;
static const int S27 = 27;
static const int S28 = 28;
static const int S29 = 29;
static const int S30 = 30;

/* 状态 → 是否为接受状态 */
static const int g_accept[31] = {
    0,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,0,1,
    1,1,1,1,1,1,1
};

/* 状态 → 该接受状态输出的编码（-1 表示不输出，如注释） */
static const int g_acceptCode[31] = {
    -1,100,101,204,207,201,202,203,206,208,214,210,227,215,216,219,220,221,222,223,224,226,-1,-1,
    217,218,209,212,211,213,205
};

/* 状态转换表：g_trans[状态][字母表列号] = 目的状态，-1 表示无转移。
 * 列号与字母表的对应关系见下面的 charIndex()；按字符类模式时
 * 每一列是一个字符类（letter、digit、+ 等），而不是单个字节。 */
static const int g_trans[31][23] = {
    { 1,2,3,4,2,5,6,7,8,9,10,11,12,13,14,15,16,17,18,19,20,21,-1 },
    { 1,-1,-1,22,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1 },
    { 2,2,-1,-1,2,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1 },
    { -1,-1,23,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1 },
    { -1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1 },
    { -1,-1,-1,-1,-1,24,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1 },
    { -1,-1,-1,-1,-1,-1,25,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1 },
    { -1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1 },
    { -1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1 },
    { -1,-1,-1,-1,-1,-1,-1,-1,-1,-1,26,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1 },
    { -1,-1,-1,-1,-1,-1,-1,-1,-1,-1,27,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1 },
    { -1,-1,-1,-1,-1,-1,-1,-1,-1,-1,28,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1 },
    { -1,-1,-1,-1,-1,-1,-1,-1,-1,-1,29,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1 },
    { -1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1 },
    { -1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1 },
    { -1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1 },
    { -1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1 },
    { -1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1 },
    { -1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1 },
    { -1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1 },
    { -1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1 },
    { -1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1 },
    { 30,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1 },
    { 23,23,23,23,23,23,23,23,23,23,23,23,23,23,23,23,23,23,23,23,23,23,23 },
    { -1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1 },
    { -1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1 },
    { -1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1 },
    { -1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1 },
    { -1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1 },
    { -1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1 },
    { 30,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1 },
};

/* 功能：把字符映射为状态转换表的列号。
 * 输入：c —— 待判定字符。
 * 输出：列号；该字符不属于字母表里任何一列时返回 -1。
 * 说明：按字符类模式时，每一条 if 判断的是"这个字节属于哪个字符类"
 *       （一次覆盖一个区间，而不是 256 条等值比较）。            */
static int charIndex(unsigned char c) {
    if ((c >= '0' && c <= '9')) return 0;    /* digit */
    if ((c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z')) return 1;    /* letter */
    if (c == '/') return 2;    /* / */
    if (c == '.') return 3;    /* . */
    if (c == '_') return 4;    /* _ */
    if (c == '+') return 5;    /* + */
    if (c == '-') return 6;    /* - */
    if (c == '*') return 7;    /* * */
    if (c == '^') return 8;    /* ^ */
    if (c == '<') return 9;    /* < */
    if (c == '=') return 10;    /* = */
    if (c == '>') return 11;    /* > */
    if (c == '!') return 12;    /* ! */
    if (c == ';') return 13;    /* ; */
    if (c == ',') return 14;    /* , */
    if (c == '(') return 15;    /* ( */
    if (c == ')') return 16;    /* ) */
    if (c == '[') return 17;    /* [ */
    if (c == ']') return 18;    /* ] */
    if (c == '{') return 19;    /* { */
    if (c == '}') return 20;    /* } */
    if (c == '%') return 21;    /* % */
    if ((c >= 0x00 && c <= 0x09) || (c >= 0x0b && c <= 0x0c) || (c >= 0x0e && c <= ' ') || (c >= '"' && c <= '$') || (c >= '&' && c <= 0x27) || c == ':' || (c >= '?' && c <= '@') || c == 0x5c || c == '`' || c == '|' || (c >= '~' && c <= 0xff)) return 22;    /* <other> */
    return -1;
}

/* ------------------------------------------------------------------ */
/* 驱动逻辑：最长匹配 + 回退 + 按格式输出                              */
/* ------------------------------------------------------------------ */

/* 功能：把单词原文与编码按 AGENT.md 4.3 节的格式写出。
 * 输入：code —— 单词编码；text —— 单词原文；length —— 原文长度。
 * 输出：无。
 * 说明：ID（编码等于 g_idCode）的值带双引号；
 *       NUM（编码 100）输出原文以保留前导零；
 *       其它（关键字/专用符号）只输出编码。                       */
static void emitToken(int code, const char* text, int length) {
    const char* sep = g_oneLine ? " " : "\n";
    if (code == g_idCode) {
        /* 标识符：先查关键字表，命中则改判为关键字编码 */
        char word[WORD_MAX];
        int n = (length < WORD_MAX - 1) ? length : WORD_MAX - 1;
        memcpy(word, text, (size_t)n);
        word[n] = '\0';
        const int keywordCode = findKeyword(word);
        if (keywordCode >= 0) {
            fprintf(g_out, "%d%s", keywordCode, sep);
            return;
        }
        fprintf(g_out, "%d \"%s\"%s", code, word, sep);
        return;
    }
    if (code == 100) {
        /* 数字：输出原文，保留前导零 */
        char word[WORD_MAX];
        int n = (length < WORD_MAX - 1) ? length : WORD_MAX - 1;
        memcpy(word, text, (size_t)n);
        word[n] = '\0';
        fprintf(g_out, "%d %s%s", code, word, sep);
        return;
    }
    /* 关键字与专用符号：只输出编码 */
    fprintf(g_out, "%d%s", code, sep);
}

/* 功能：取当前状态经字符 ch 转移后的状态。
 * 输入：state —— 当前状态；ch —— 字符。
 * 输出：目的状态；无转移时返回 -1。                    */
static int stepState(int state, unsigned char ch) {
    const int column = charIndex(ch);
    if (column < 0)
        return -1;
    return g_trans[state][column];
}

/* 功能：判断该状态是否为接受状态。
 * 输入：state —— 状态。输出：接受时为 1。
 * 注意：用独立的 g_accept 表判断，**不能**用“编码是否为 -1”代替 ——
 *       注释状态也是接受状态，只是它不输出编码。
 */
static int isAcceptState(int state) {
    return (state >= 0) ? g_accept[state] : 0;
}

/* 功能：取该接受状态应输出的编码（-1 表示不输出，如注释）。  */
static int acceptCodeOf(int state) {
    return (state >= 0) ? g_acceptCode[state] : -1;
}

/* 功能：读取源程序文本，逐个识别单词并按格式输出。
 * 输入：text —— 源程序全文；length —— 全文长度。
 * 输出：无。
 * 说明：核心是最长匹配——先沿自动机一直前进，记录最近一次
 *       处于接受状态的位置；无法前进时回退到该位置输出单词，
 *       再从该位置继续扫描。若最近接受位置是注释，则丢弃这段
 *       文本（不输出编码）。完全无法识别时报告词法错误。       */
static void scan(const char* text, int length) {
    int pos = 0;
    /* 跳过 UTF-8 BOM（EF BB BF）：源程序常由记事本等工具保存，
     * 带 BOM 时若不当成空白跳过，首个单词会被 BOM 字节污染
     * （本项目实际踩过：'read' 被识别成 'ead'）。 */
    if (length >= 3 && (unsigned char)text[0] == 0xEF
        && (unsigned char)text[1] == 0xBB && (unsigned char)text[2] == 0xBF)
        pos = 3;
    while (pos < length) {
        /* 跳过空白字符（空格、制表、回车、换行） */
        const unsigned char first = (unsigned char)text[pos];
        if (first == ' ' || first == '\t' || first == '\r' || first == '\n') {
            pos++;
            continue;
        }

        int state = S0;          /* DFA 初态 */
        int lastAcceptCode = -1; /* 最近一次接受状态应输出的编码 */
        int lastAcceptPos = pos; /* 最近一次接受状态对应的输入位置 */
        int p = pos;

        while (p < length) {
            const int next = stepState(state, (unsigned char)text[p]);
            if (next < 0)
                break;           /* 无法继续前进，结束本单词 */
            state = next;
            p++;
            if (isAcceptState(state)) {
                lastAcceptCode = acceptCodeOf(state);
                lastAcceptPos = p;
            }
        }

        if (lastAcceptPos == pos) {
            /* 一个字符都没能识别：词法错误 */
            fprintf(stderr, "词法错误：位置 %d 处的字符 '%c'（ASCII %d）无法识别。\n",
                    pos, text[pos], (int)(unsigned char)text[pos]);
            pos++;
            continue;
        }

        if (lastAcceptCode < 0) {
            /* 回退到的是注释：参与自动机但不输出编码，丢弃后继续扫描 */
            pos = lastAcceptPos;
            continue;
        }

        emitToken(lastAcceptCode, text + pos, lastAcceptPos - pos);
        pos = lastAcceptPos;
    }
    /* 拼成一行排版时，单词之间用空格分隔且不换行，
     * 因此 EOF 必须另起一行；逐行排版时直接跟在最后一个换行之后。 */
    if (g_oneLine)
        fprintf(g_out, "\nEOF\n");
    else
        fprintf(g_out, "EOF\n");
}

/* 功能：程序入口。
 * 输入：argv[1] 源程序路径；argv[2] 可选，输出的 .lex 文件路径。
 * 输出：成功返回 0。                                              */
int main(int argc, char** argv) {
    if (argc < 2) {
        fprintf(stderr, "用法：%s <源程序文件> [输出的 .lex 文件]\n", argv[0]);
        return 1;
    }

    FILE* in = fopen(argv[1], "rb");
    if (in == NULL) {
        fprintf(stderr, "无法打开源程序文件：%s\n", argv[1]);
        return 2;
    }
    if (argc >= 3) {
        g_out = fopen(argv[2], "wb");
        if (g_out == NULL) {
            fprintf(stderr, "无法创建输出文件：%s\n", argv[2]);
            fclose(in);
            return 3;
        }
    } else {
        g_out = stdout;
    }

    /* 一次性读入全文（课程设计的测试源程序规模很小，无需分块） */
    static char buffer[1024 * 1024];
    const size_t read = fread(buffer, 1, sizeof(buffer) - 1, in);
    buffer[read] = '\0';
    fclose(in);

    scan(buffer, (int)read);

    if (g_out != stdout)
        fclose(g_out);
    return 0;
}
