/******************************************************************************
 * (1) 版权信息 : 编译原理课程设计 - 华南师范大学计算机学院
 * (2) 文件名称 : TINY_lexer.cpp
 *     标识符   : TINY 语言词法分析程序
 *     摘要     : 本文件由《编译原理课程设计》系统自动生成，请勿手工修改。
 *                生成依据：由 TINY 语言全部单词正则式经
 *                正则式 → NFA（Thompson 构造）→ DFA（子集构造法）→
 *                最小化 DFA（划分法）所得的 24 状态最小化 DFA。
 *                转换方法：方法二：switch(state)。
 * (3) 版本号   : 1.0.0       作者/修改者 : 自动生成   完成日期 : 自动
 * (4) 版本历史 : 1.0.0  由系统生成
 *
 * 运行方式：TINY_lexer <源程序文件> [输出的 .lex 文件]
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
    { "else", "105" },
    { "end", "106" },
    { "if", "103" },
    { "read", "109" },
    { "repeat", "107" },
    { "then", "104" },
    { "until", "108" },
    { "write", "110" },
};
static const int g_keywordCount = 8;

static const char* g_literalTable[][2] = {
    { "%", "204" },
    { "(", "216" },
    { ")", "217" },
    { "*", "202" },
    { "+", "200" },
    { "-", "201" },
    { "/", "203" },
    { ":=", "215" },
    { ";", "214" },
    { "<", "206" },
    { "<=", "207" },
    { "<>", "208" },
    { "=", "211" },
    { ">", "209" },
    { ">=", "210" },
    { "^", "205" },
    { "else", "105" },
    { "end", "106" },
    { "if", "103" },
    { "read", "109" },
    { "repeat", "107" },
    { "then", "104" },
    { "until", "108" },
    { "write", "110" },
    { "{", "212" },
    { "}", "213" },
};
static const int g_literalCount = 26;

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
/* 转换方法二：switch (state) 内嵌 switch (ch)。                      */
/*   外层按 DFA 状态分派，内层按输入字符分派；转移关系全部展开成       */
/*   switch 分支，因此不需要额外的状态转换表。                        */
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

/* 状态 → 该接受状态输出的编码（-1 表示非接受态或注释） */
static int acceptCodeOf(int state) {
    switch (state) {
    case S1: return 100;
    case S2: return 101;
    case S3: return 212;
    case S4: return 213;
    case S5: return 200;
    case S6: return 201;
    case S7: return 202;
    case S8: return 203;
    case S9: return 204;
    case S10: return 205;
    case S11: return 206;
    case S12: return 211;
    case S13: return 209;
    case S14: return 214;
    case S16: return 216;
    case S17: return 217;
    case S19: return -1;
    case S20: return 207;
    case S21: return 208;
    case S22: return 210;
    case S23: return 215;
    default: return -1;
    }
}

/* 功能：按当前状态与字符求下一个状态。
 * 输入：state —— 当前状态；ch —— 输入字符。
 * 输出：目的状态；无转移时返回 -1。
 * 说明：外层 switch 分派状态，内层逐个比较该状态有转移的符号。
 */
static int nextState(int state, unsigned char ch) {
    switch (state) {
    case S0:
        if ((ch >= '0' && ch <= '9')) return S1;    /* digit */
        if ((ch >= 'A' && ch <= 'Z') || (ch >= 'a' && ch <= 'z')) return S2;    /* letter */
        if (ch == '<') return S11;    /* < */
        if (ch == '=') return S12;    /* = */
        if (ch == '>') return S13;    /* > */
        if (ch == ';') return S14;    /* ; */
        if (ch == ':') return S15;    /* : */
        if (ch == '(') return S16;    /* ( */
        if (ch == ')') return S17;    /* ) */
        if (ch == '{') return S3;    /* { */
        if (ch == '}') return S4;    /* } */
        if (ch == '+') return S5;    /* + */
        if (ch == '-') return S6;    /* - */
        if (ch == '*') return S7;    /* * */
        if (ch == '/') return S8;    /* / */
        if (ch == '%') return S9;    /* % */
        if (ch == '^') return S10;    /* ^ */
        return -1;
    case S1:
        if ((ch >= '0' && ch <= '9')) return S1;    /* digit */
        return -1;
    case S2:
        if ((ch >= '0' && ch <= '9')) return S2;    /* digit */
        if ((ch >= 'A' && ch <= 'Z') || (ch >= 'a' && ch <= 'z')) return S2;    /* letter */
        return -1;
    case S3:
        if ((ch >= '0' && ch <= '9')) return S18;    /* digit */
        if ((ch >= 'A' && ch <= 'Z') || (ch >= 'a' && ch <= 'z')) return S18;    /* letter */
        if (ch == '<') return S18;    /* < */
        if (ch == '=') return S18;    /* = */
        if (ch == '>') return S18;    /* > */
        if (ch == ';') return S18;    /* ; */
        if (ch == ':') return S18;    /* : */
        if (ch == '(') return S18;    /* ( */
        if (ch == ')') return S18;    /* ) */
        if ((ch >= 0x00 && ch <= '$') || (ch >= '&' && ch <= 0x27) || ch == ',' || ch == '.' || (ch >= '?' && ch <= '@') || (ch >= '[' && ch <= ']') || (ch >= '_' && ch <= '`') || ch == '|' || (ch >= '~' && ch <= 0xff)) return S18;    /* <other> */
        if (ch == '{') return S18;    /* { */
        if (ch == '}') return S19;    /* } */
        if (ch == '+') return S18;    /* + */
        if (ch == '-') return S18;    /* - */
        if (ch == '*') return S18;    /* * */
        if (ch == '/') return S18;    /* / */
        if (ch == '%') return S18;    /* % */
        if (ch == '^') return S18;    /* ^ */
        return -1;
    case S4:
        return -1;
    case S5:
        return -1;
    case S6:
        return -1;
    case S7:
        return -1;
    case S8:
        return -1;
    case S9:
        return -1;
    case S10:
        return -1;
    case S11:
        if (ch == '=') return S20;    /* = */
        if (ch == '>') return S21;    /* > */
        return -1;
    case S12:
        return -1;
    case S13:
        if (ch == '=') return S22;    /* = */
        return -1;
    case S14:
        return -1;
    case S15:
        if (ch == '=') return S23;    /* = */
        return -1;
    case S16:
        return -1;
    case S17:
        return -1;
    case S18:
        if ((ch >= '0' && ch <= '9')) return S18;    /* digit */
        if ((ch >= 'A' && ch <= 'Z') || (ch >= 'a' && ch <= 'z')) return S18;    /* letter */
        if (ch == '<') return S18;    /* < */
        if (ch == '=') return S18;    /* = */
        if (ch == '>') return S18;    /* > */
        if (ch == ';') return S18;    /* ; */
        if (ch == ':') return S18;    /* : */
        if (ch == '(') return S18;    /* ( */
        if (ch == ')') return S18;    /* ) */
        if ((ch >= 0x00 && ch <= '$') || (ch >= '&' && ch <= 0x27) || ch == ',' || ch == '.' || (ch >= '?' && ch <= '@') || (ch >= '[' && ch <= ']') || (ch >= '_' && ch <= '`') || ch == '|' || (ch >= '~' && ch <= 0xff)) return S18;    /* <other> */
        if (ch == '{') return S18;    /* { */
        if (ch == '}') return S19;    /* } */
        if (ch == '+') return S18;    /* + */
        if (ch == '-') return S18;    /* - */
        if (ch == '*') return S18;    /* * */
        if (ch == '/') return S18;    /* / */
        if (ch == '%') return S18;    /* % */
        if (ch == '^') return S18;    /* ^ */
        return -1;
    case S19:
        return -1;
    case S20:
        return -1;
    case S21:
        return -1;
    case S22:
        return -1;
    case S23:
        return -1;
    default: return -1;
    }
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
    return nextState(state, ch);
}

/* 功能：判定状态是否为接受状态。
 * 输入：state —— 状态。输出：接受时为 1。
 * 注意：**不能用“编码是否为 -1”代替本判断**——注释状态也是接受状态，
 *       只是它不输出编码。若漏掉注释，最长匹配就不会记下注释的结束位置，
 *       注释内容会被逐字符当成符号切碎（本项目实际踩过这个坑）。
 */
static int isAcceptState(int state) {
    switch (state) {
    case S1: return 1;
    case S2: return 1;
    case S3: return 1;
    case S4: return 1;
    case S5: return 1;
    case S6: return 1;
    case S7: return 1;
    case S8: return 1;
    case S9: return 1;
    case S10: return 1;
    case S11: return 1;
    case S12: return 1;
    case S13: return 1;
    case S14: return 1;
    case S16: return 1;
    case S17: return 1;
    case S19: return 1;
    case S20: return 1;
    case S21: return 1;
    case S22: return 1;
    case S23: return 1;
    default: return 0;
    }
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
