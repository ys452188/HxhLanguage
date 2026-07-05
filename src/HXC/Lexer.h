#ifndef HXHLANG_SRC_HXC_LEXER_H
#define HXHLANG_SRC_HXC_LEXER_H
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <wchar.h>
#include <wctype.h>

#include "Error.h"

typedef enum HxTokenType {
    TOK_NIL,
    TOK_ID,   // 标识符
    TOK_KW,   // 关键字
    TOK_VAL,  // 表面值
    TOK_OPR_MUL,
    TOK_OPR_DIV,
    TOK_OPR_ADD,
    TOK_OPR_SUB,
    TOK_OPR_SET,        // =
    TOK_OPR_EQU,        // ==
    TOK_OPR_NEQU,       // !=
    TOK_OPR_GT,         // >
    TOK_OPR_LT,         // <
    TOK_OPR_GTE,        // >=
    TOK_OPR_LTE,        // <=
    TOK_OPR_AND_LOGIC,  // &&,  &实际被识别为TOK_OPR_REFER
    TOK_OPR_OR_LOGIC,   // ||
    TOK_OPR_OR,
    TOK_OPR_NOT,       // !
    TOK_OPR_INC,       // ++
    TOK_OPR_DEC,       // --
    TOK_OPR_LQUOTE,    // (
    TOK_OPR_RQUOTE,    // )
    TOK_OPR_LBRACE,    // {
    TOK_OPR_RBRACE,    // }
    TOK_OPR_LBRACKET,  // [
    TOK_OPR_RBRACKET,  // ]
    TOK_OPR_COMMA,     // ,
    TOK_OPR_DOT,       // .
    TOK_OPR_COLON,     // :
    TOK_OPR_POINT,     // ->
    TOK_OPR_REFER,     // &
    TOK_END,           // 终结符 (;|；|。)
} HxTokenType;
typedef struct Token {
    HxTokenType type;
    wchar_t* value;
#define CH 0
#define STR 1
    char mark;  // 标记类型
    int line;
} Token;
typedef struct Tokens {  // Token流
    int size;
    int count;
    Token* tokens;
} Tokens;
wchar_t* keyword[] = {  // 关键字
    L"ret",      L"var",      L"con",        L"fun",      L"cls",     L"if",         L"int",        L"float",  L"str",
    L"char",     L"整型",     L"浮点型",     L"字符串型", L"字符型",  L"定义变量",   L"定义常量",   L"函数",   L"定义类",
    L"公有成员", L"私有成员", L"受保护成员", L"public",   L"private", L"proctected", L"类型是",     L"父类是", L"repeat",
    L"until",    L"循环",     L"直到",       L"若",       L"返回",    L"返回类型是", L"无返回类型",
    L"for", L"迭代循环", L"else", L"否则", NULL};
static inline wchar_t* escape(const wchar_t* src) noexcept;
static inline bool isKeyword(wchar_t* str) noexcept;  // 判断是否是关键字
static inline bool isOperator(wchar_t ch) noexcept;   // 判断是否是操作符
Tokens* lex(wchar_t* src, int* err) noexcept;         // 词法分析
void freeTokens(Tokens** tokens) noexcept;            // 释放

Tokens* lex(wchar_t* src, int* err) noexcept {
#define MEM_FAIL     \
    {                \
        *err = -1;   \
        return NULL; \
    }  // 内存错误
#define ERR          \
    {                \
        *err = 255;  \
        return NULL; \
    }  // 语法错误
    if (err == NULL) return NULL;
    if (src == NULL) MEM_FAIL;
    Tokens* tokens = (Tokens*)calloc(1, sizeof(Tokens));
    if (!tokens) MEM_FAIL;
    int index_src = 0;
    int length_src = wcslen(src);
    int line = 1;  // 当前行数
    int token_index = 0;
    tokens->tokens = (Token*)calloc(1, sizeof(Token));
    if (!(tokens->tokens)) MEM_FAIL;
    tokens->size = 1;
    for (index_src = 0; index_src < length_src; index_src++) {
        // printf("%lc\n", src[index_src]);
        if (src[index_src] == L'#') {  // 处理注释
            while (index_src < length_src) {
                if (src[index_src] == L'\n' || src[index_src] == L'\r') break;
                index_src++;
            }
            line++;
            continue;
        }
        if (src[index_src] == L'<') {
            if (index_src + 1 < length_src) {
                if (src[index_src + 1] == L'-') {
                    while (index_src < length_src) {
                        if (src[index_src] == L'\n' || src[index_src] == L'\r') break;
                        index_src++;
                    }
                    line++;
                    continue;
                }
            }
        }
        if (src[index_src] == L'/') {
            if (index_src + 1 < length_src) {
                if (src[index_src + 1] == L'/') {
                    while (index_src < length_src) {
                        if (src[index_src] == L'\n' || src[index_src] == L'\r') break;
                        index_src++;
                    }
                    line++;
                    continue;
                }
            }
        }
        if (token_index >= tokens->size) {
            int old_size = tokens->size;
            tokens->size = token_index + 4;
            int new_alloc_size = tokens->size - old_size;
            void* temp = realloc(tokens->tokens, (tokens->size) * sizeof(Token));
            if (!temp) MEM_FAIL;
            tokens->tokens = (Token*)temp;
            // 初始化新分配部分
            memset((tokens->tokens + old_size), 0, new_alloc_size * sizeof(Token));
#ifdef HX_DEBUG
            // log(L"tokens大小增加了");
#endif
        }
        tokens->tokens[token_index].mark = -1;
        if (src[index_src] == L'\n') line++;
        if (iswspace(src[index_src])) continue;
        if (src[index_src] == L'\'' || src[index_src] == L'‘' || src[index_src] == L'’') {  // 字符
            int start_index = index_src;
            int end_index = 0;
            while (index_src < length_src) {
                index_src++;
                if (src[index_src] == L'\n') line++;
                if (src[index_src] == L'\\') {
                    if (index_src + 1 >= length_src) {
                        wchar_t errCode[3] = {0};
                        errCode[0] = src[start_index];
                        if (start_index + 1 < length_src) errCode[1] = src[start_index + 1];
                        setError(ERR_CH_NO_END, line, errCode);
                        ERR;
                    }
                    index_src += 2;
                }
                if (src[index_src] == L'\'' || src[index_src] == L'‘' || src[index_src] == L'’') {
                    end_index = index_src;
                    break;
                }
            }
            // 字符没结尾
            if (end_index == 0) {
                wchar_t errCode[3] = {0};
                errCode[0] = src[start_index];
                if (start_index + 1 < length_src) errCode[1] = src[start_index + 1];
                setError(ERR_CH_NO_END, line, errCode);
                ERR;
            }
            start_index++;
            int len = end_index - start_index;
            tokens->tokens[token_index].mark = CH;
            tokens->tokens[token_index].type = TOK_VAL;
            tokens->tokens[token_index].value = (wchar_t*)calloc(len + 1, sizeof(wchar_t));
            if (!(tokens->tokens[token_index].value)) MEM_FAIL;
            if (len != 0) wcsncpy(tokens->tokens[token_index].value, &(src[start_index]), len);
            wchar_t* escapedString = escape(tokens->tokens[token_index].value);
            free(tokens->tokens[token_index].value);
            tokens->tokens[token_index].value = escapedString;
            tokens->tokens[token_index].line = line;
#ifdef HX_DEBUG
            log(L"已创建一个新的字符型Token");
#endif
            tokens->count++;
            token_index++;
        } else if (src[index_src] == L'\"' || src[index_src] == L'“' || src[index_src] == L'”') {  // 字符串
            int start_index = index_src;
            int end_index = 0;
            while (index_src < length_src) {
                index_src++;
                if (src[index_src] == L'\n') line++;
                if (src[index_src] == L'\\') {
                    if (index_src + 1 >= length_src) {
                        wchar_t errCode[3] = {0};
                        errCode[0] = src[start_index];
                        if (start_index + 1 < length_src) errCode[1] = src[start_index + 1];
                        setError(ERR_STR_NO_END, line, errCode);
                        ERR;
                    }
                    index_src += 2;
                }
                if (src[index_src] == L'\"' || src[index_src] == L'“' || src[index_src] == L'”') {
                    end_index = index_src;
                    break;
                }
            }
            // 字符串没结尾
            if (end_index == 0) {
                wchar_t errCode[3] = {0};
                errCode[0] = src[start_index];
                if (start_index + 1 < length_src) errCode[1] = src[start_index + 1];
                setError(ERR_STR_NO_END, line, errCode);
                ERR;
            }
            start_index++;
            int len = end_index - start_index;
            tokens->tokens[token_index].mark = STR;
            tokens->tokens[token_index].type = TOK_VAL;
            tokens->tokens[token_index].value = (wchar_t*)calloc(len + 1, sizeof(wchar_t));
            if (!(tokens->tokens[token_index].value)) MEM_FAIL;
            if (len != 0) wcsncpy(tokens->tokens[token_index].value, &(src[start_index]), len);
            wchar_t* escapedString = escape(tokens->tokens[token_index].value);
            free(tokens->tokens[token_index].value);
            tokens->tokens[token_index].value = escapedString;
            tokens->tokens[token_index].line = line;
#ifdef HX_DEBUG
            // log(L"已创建一个新的字符串型Token");
#endif
            token_index++;
            tokens->count++;
        } else if (src[index_src] == L';' || src[index_src] == L'；' || src[index_src] == L'。') {  // 结束符
            tokens->tokens[token_index].type = TOK_END;
            tokens->tokens[token_index].value = (wchar_t*)calloc(2, sizeof(wchar_t));
            if (!(tokens->tokens[token_index].value)) MEM_FAIL;
            tokens->tokens[token_index].value[0] = src[index_src];
            tokens->tokens[token_index].line = line;
#ifdef HX_DEBUG
            // log(L"已创建一个新的终结符Token");
#endif
            token_index++;
            tokens->count++;
        } else if (isOperator(src[index_src])) {  // 操作符
            // printf("%lc\n", src[index_src]);
            // 偷看是否为两个字符组成的操作符
            if (index_src + 1 < length_src) {
                if ((src[index_src] == L'>' && src[index_src + 1] == L'=') ||
                    (src[index_src] == L'<' && src[index_src + 1] == L'=') ||  //<=
                    (src[index_src] == L'=' && src[index_src + 1] == L'=') ||  //==
                    (src[index_src] == L'+' && src[index_src + 1] == L'+') ||  //++
                    (src[index_src] == L'-' && src[index_src + 1] == L'-') ||  //--
                    (src[index_src] == L'-' && src[index_src + 1] == L'>') ||  //->
                    (src[index_src] == L'&' && src[index_src + 1] == L'&') ||  //&&
                    (src[index_src] == L'|' && src[index_src + 1] == L'|')) {
                    if ((src[index_src] == L'>' && src[index_src + 1] == L'=')) {
                        tokens->tokens[token_index].type = TOK_OPR_GTE;
                    } else if ((src[index_src] == L'<' && src[index_src + 1] == L'=')) {
                        tokens->tokens[token_index].type = TOK_OPR_LTE;
                    } else if ((src[index_src] == L'=' && src[index_src + 1] == L'=')) {
                        tokens->tokens[token_index].type = TOK_OPR_EQU;
                    } else if ((src[index_src] == L'+' && src[index_src + 1] == L'+')) {
                        tokens->tokens[token_index].type = TOK_OPR_INC;
                    } else if ((src[index_src] == L'-' && src[index_src + 1] == L'-')) {
                        tokens->tokens[token_index].type = TOK_OPR_DEC;
                    } else if ((src[index_src] == L'-' && src[index_src + 1] == L'>')) {
                        tokens->tokens[token_index].type = TOK_OPR_POINT;
                    } else if ((src[index_src] == L'|' && src[index_src + 1] == L'|')) {
                        tokens->tokens[token_index].type = TOK_OPR_OR_LOGIC;
                    } else if ((src[index_src] == L'&' && src[index_src + 1] == L'&')) {
                        tokens->tokens[token_index].type = TOK_OPR_AND_LOGIC;
                    }
                    tokens->tokens[token_index].value = (wchar_t*)calloc(3, sizeof(wchar_t));
                    if (!(tokens->tokens[token_index].value)) MEM_FAIL;
                    wcsncpy(tokens->tokens[token_index].value, &(src[index_src]), 2);
                    tokens->tokens[token_index].line = line;
                    index_src++;
#ifdef HX_DEBUG
                    // log(L"已创建一个新的操作符Token");
#endif
                } else {
                    if (src[index_src] == L'+') {
                        tokens->tokens[token_index].type = TOK_OPR_ADD;
                    } else if (src[index_src] == L'-') {
                        tokens->tokens[token_index].type = TOK_OPR_SUB;
                    } else if (src[index_src] == L'*' || src[index_src] == L'×') {
                        tokens->tokens[token_index].type = TOK_OPR_MUL;
                    } else if (src[index_src] == L'/' || src[index_src] == L'÷') {
                        tokens->tokens[token_index].type = TOK_OPR_DIV;
                    } else if (src[index_src] == L'=') {
                        tokens->tokens[token_index].type = TOK_OPR_SET;
                    } else if (src[index_src] == L'>') {
                        tokens->tokens[token_index].type = TOK_OPR_GT;
                    } else if (src[index_src] == L'<') {
                        tokens->tokens[token_index].type = TOK_OPR_LT;
                    } else if (src[index_src] == L'&') {
                        tokens->tokens[token_index].type = TOK_OPR_REFER;
                    } else if (src[index_src] == L'|') {
                        tokens->tokens[token_index].type = TOK_OPR_OR;
                    } else if (src[index_src] == L'!' || src[index_src] == L'！') {
                        tokens->tokens[token_index].type = TOK_OPR_NOT;
                    } else if (src[index_src] == L'(' || src[index_src] == L'（') {
                        tokens->tokens[token_index].type = TOK_OPR_LQUOTE;
                    } else if (src[index_src] == L')' || src[index_src] == L'）') {
                        tokens->tokens[token_index].type = TOK_OPR_RQUOTE;
                    } else if (src[index_src] == L'{') {
                        tokens->tokens[token_index].type = TOK_OPR_LBRACE;
                    } else if (src[index_src] == L'}') {
                        tokens->tokens[token_index].type = TOK_OPR_RBRACE;
                    } else if (src[index_src] == L'[' || src[index_src] == L'【') {
                        tokens->tokens[token_index].type = TOK_OPR_LBRACKET;
                    } else if (src[index_src] == L']' || src[index_src] == L'】') {
                        tokens->tokens[token_index].type = TOK_OPR_RBRACKET;
                    } else if (src[index_src] == L',' || src[index_src] == L'，') {
                        tokens->tokens[token_index].type = TOK_OPR_COMMA;
                    } else if (src[index_src] == L'.') {
                        tokens->tokens[token_index].type = TOK_OPR_DOT;
                    } else if (src[index_src] == L':' || src[index_src] == L'：') {
                        tokens->tokens[token_index].type = TOK_OPR_COLON;
                    } else if (src[index_src] == L'≤') {
                        tokens->tokens[token_index].type = TOK_OPR_LTE;
                    } else if (src[index_src] == L'≥') {
                        tokens->tokens[token_index].type = TOK_OPR_GTE;
                    } else if (src[index_src] == L'≠') {
                        tokens->tokens[token_index].type = TOK_OPR_NEQU;
                    } else if (src[index_src] == L'＆' || src[index_src] == L'&') {
                        tokens->tokens[token_index].type = TOK_OPR_REFER;
                    } else {
                        tokens->tokens[token_index].type = TOK_NIL;
                    }
                    tokens->tokens[token_index].value = (wchar_t*)calloc(2, sizeof(wchar_t));
                    if (!(tokens->tokens[token_index].value)) MEM_FAIL;
                    tokens->tokens[token_index].value[0] = src[index_src];
                    tokens->tokens[token_index].line = line;
#ifdef HX_DEBUG
                    // log(L"已创建一个新的操作符Token");
#endif
                }
            } else {
                if (src[index_src] == L'+') {
                    tokens->tokens[token_index].type = TOK_OPR_ADD;
                } else if (src[index_src] == L'-') {
                    tokens->tokens[token_index].type = TOK_OPR_SUB;
                } else if (src[index_src] == L'*' || src[index_src] == L'×') {
                    tokens->tokens[token_index].type = TOK_OPR_MUL;
                } else if (src[index_src] == L'/' || src[index_src] == L'÷') {
                    tokens->tokens[token_index].type = TOK_OPR_DIV;
                } else if (src[index_src] == L'=') {
                    tokens->tokens[token_index].type = TOK_OPR_SET;
                } else if (src[index_src] == L'>') {
                    tokens->tokens[token_index].type = TOK_OPR_GT;
                } else if (src[index_src] == L'<') {
                    tokens->tokens[token_index].type = TOK_OPR_LT;
                } else if (src[index_src] == L'&') {
                    tokens->tokens[token_index].type = TOK_OPR_REFER;
                } else if (src[index_src] == L'|') {
                    tokens->tokens[token_index].type = TOK_OPR_OR;
                } else if (src[index_src] == L'!' || src[index_src] == L'！') {
                    tokens->tokens[token_index].type = TOK_OPR_NOT;
                } else if (src[index_src] == L'(' || src[index_src] == L'（') {
                    tokens->tokens[token_index].type = TOK_OPR_LQUOTE;
                } else if (src[index_src] == L')' || src[index_src] == L'）') {
                    tokens->tokens[token_index].type = TOK_OPR_RQUOTE;
                } else if (src[index_src] == L'{') {
                    tokens->tokens[token_index].type = TOK_OPR_LBRACE;
                } else if (src[index_src] == L'}') {
                    tokens->tokens[token_index].type = TOK_OPR_RBRACE;
                } else if (src[index_src] == L'[' || src[index_src] == L'【') {
                    tokens->tokens[token_index].type = TOK_OPR_LBRACKET;
                } else if (src[index_src] == L']' || src[index_src] == L'】') {
                    tokens->tokens[token_index].type = TOK_OPR_RBRACKET;
                } else if (src[index_src] == L',' || src[index_src] == L'，') {
                    tokens->tokens[token_index].type = TOK_OPR_COMMA;
                } else if (src[index_src] == L'.') {
                    tokens->tokens[token_index].type = TOK_OPR_DOT;
                } else if (src[index_src] == L':' || src[index_src] == L'：') {
                    tokens->tokens[token_index].type = TOK_OPR_COLON;
                } else if (src[index_src] == L'≤') {
                    tokens->tokens[token_index].type = TOK_OPR_LTE;
                } else if (src[index_src] == L'≥') {
                    tokens->tokens[token_index].type = TOK_OPR_GTE;
                } else if (src[index_src] == L'≠') {
                    tokens->tokens[token_index].type = TOK_OPR_NEQU;
                } else if (src[index_src] == L'＆' || src[index_src] == L'&') {
                    tokens->tokens[token_index].type = TOK_OPR_REFER;
                } else {
                    tokens->tokens[token_index].type = TOK_NIL;
                }
                tokens->tokens[token_index].value = (wchar_t*)calloc(2, sizeof(wchar_t));
                if (!(tokens->tokens[token_index].value)) MEM_FAIL;
                tokens->tokens[token_index].value[0] = src[index_src];
                tokens->tokens[token_index].line = line;
#ifdef HX_DEBUG
                // log(L"已创建一个新的操作符或未知Token（末尾字符）");
#endif
            }
            token_index++;
            tokens->count++;
        } else if (iswdigit(src[index_src])) {  // 数字
            int start = index_src;
            while ((src[index_src] != L'\0') && ((!isOperator(src[index_src])) && (!iswspace(src[index_src])))) {
                index_src++;
            }
            if (src[index_src] == L'.') {             // 小数
                if (!iswdigit(src[index_src + 1])) {  // 114.  ->错误
                    int len = index_src + 1 - start;
                    wchar_t* errCode = (wchar_t*)calloc(len + 1, sizeof(wchar_t));
                    if (!errCode) MEM_FAIL;
                    wcsncpy(errCode, &(src[start]), len);
                    setError(ERR_VAL, line, errCode);
                    free(errCode);
                    errCode = NULL;
                    ERR;
                }
                index_src++;
                while ((src[index_src] != L'\0') && ((!isOperator(src[index_src])) && (!iswspace(src[index_src])))) {
                    index_src++;
                }
            }
            int end = index_src;
            int len = end - start;
            tokens->tokens[token_index].type = TOK_VAL;
            tokens->tokens[token_index].value = (wchar_t*)calloc(len + 1, sizeof(wchar_t));
            if (!(tokens->tokens[token_index].value)) MEM_FAIL;
            if (len != 0) wcsncpy(tokens->tokens[token_index].value, &(src[start]), len);
            tokens->tokens[token_index].line = line;
            // printf("%ls\n", tokens->tokens[token_index].value);
#ifdef HX_DEBUG
            // log(L"已创建一个新的字面量Token");
#endif
            token_index++;
            // 此时src[index_src]不为数字和字母,但在这次循环后index++,将导致忽略该Token,因此最后应index--;
            index_src--;
            tokens->count++;
        } else {  // 标识符或关键字
            int start = index_src;
            while ((src[index_src] != L'\0') && ((!isOperator(src[index_src])) && (!iswspace(src[index_src])))) {
                index_src++;
            }
            int end = index_src;
            int len = end - start;
            tokens->tokens[token_index].value = (wchar_t*)calloc(len + 1, sizeof(wchar_t));
            if (!(tokens->tokens[token_index].value)) MEM_FAIL;
            if (len != 0) wcsncpy(tokens->tokens[token_index].value, &(src[start]), len);
            tokens->tokens[token_index].line = line;
            tokens->tokens[token_index].type = TOK_ID;
            if (isKeyword(tokens->tokens[token_index].value)) {
                tokens->tokens[token_index].type = TOK_KW;
            }
            // printf("%ls\n", tokens->tokens[token_index].value);
#ifdef HX_DEBUG
            if (isKeyword(tokens->tokens[token_index].value)) {
                // log(L"已创建一个新的关键字Token");
            } else {
                // log(L"已创建一个新的标识符Token");
            }
#endif
            token_index++;
            tokens->count++;
            index_src--;
        }
    }
    return tokens;
}

static bool isKeyword(wchar_t* str) noexcept {
    if (str == NULL) return false;
    int index = 0;
    while (keyword[index] != NULL) {
        if (wcscmp(keyword[index], str) == 0) return true;
        index++;
    }
    return false;
}
static bool isOperator(wchar_t ch) noexcept {
    return (ch == L'+' || ch == L'-' || ch == L'*' || ch == L'×' || ch == L',' || ch == L'，' || ch == L'/' || ch == L'÷' ||
            ch == L'=' || ch == L'\"' || ch == L'.' || ch == L'\'' || ch == L'“' || ch == L'‘' || ch == L'’' || ch == L'”' ||
            ch == L'|' || ch == L'&' || ch == L'^' || ch == L'%' || ch == L'!' || ch == L'！' || ch == L'{' || ch == L'}' ||
            ch == L'(' || ch == L')' || ch == L'（' || ch == L'）' || ch == L';' || ch == L'；' || ch == L'。' || ch == L'[' ||
            ch == L'【' || ch == L']' || ch == L'】' || ch == L':' || ch == L'：' || ch == L'>' || ch == L'<' || ch == L'≥' ||
            ch == L'≤');
}
void freeTokens(Tokens** tokens) noexcept {
#ifdef HX_DEBUG
    log(L"释放tokens");
#endif
    if (tokens == NULL || *tokens == NULL) return;
    if ((*tokens)->tokens) {
        for (int i = 0; i < (*tokens)->size; i++) {
            if ((*tokens)->tokens[i].value) {
                free((*tokens)->tokens[i].value);
                (*tokens)->tokens[i].value = NULL;
            }
        }
        free((*tokens)->tokens);
        (*tokens)->tokens = NULL;
    }
    free((*tokens));
    *tokens = NULL;
    return;
}
#ifdef HX_DEBUG
void showTokens(Tokens* tokens) {
    log(L"tokens的图形化展现：");
    if (tokens == NULL) return;
    if (tokens->tokens) {
        fwprintf(logStream, L"\33[33m\t");
        for (int i = 0; i < (tokens)->count; i++) {
            switch (tokens->tokens[i].type) {
                case TOK_ID: {
                    fwprintf(logStream, L"[ID]");
                    break;
                }
                case TOK_END: {
                    fwprintf(logStream, L"[END]");
                    break;
                }
                case TOK_KW: {
                    fwprintf(logStream, L"[KW]");
                    break;
                }
                case TOK_NIL: {
                    fwprintf(logStream, L"[NIL]");
                    break;
                }
                case TOK_VAL: {
                    fwprintf(logStream, L"[VAL]");
                    break;
                }
                case TOK_OPR_MUL: {
                    fwprintf(logStream, L"[MUL]");
                    break;
                }
                case TOK_OPR_DIV: {
                    fwprintf(logStream, L"[DIV]");
                    break;
                }
                case TOK_OPR_ADD: {
                    fwprintf(logStream, L"[ADD]");
                    break;
                }
                case TOK_OPR_SUB: {
                    fwprintf(logStream, L"[SUB]");
                    break;
                }
                case TOK_OPR_SET: {
                    fwprintf(logStream, L"[SET]");
                    break;
                }
                case TOK_OPR_EQU: {
                    fwprintf(logStream, L"[EQU]");
                    break;
                }
                case TOK_OPR_NEQU: {
                    fwprintf(logStream, L"[NEQU]");
                    break;
                }
                case TOK_OPR_GT: {
                    fwprintf(logStream, L"[GT]");
                    break;
                }
                case TOK_OPR_LT: {
                    fwprintf(logStream, L"[LT]");
                    break;
                }
                case TOK_OPR_GTE: {
                    fwprintf(logStream, L"[GTE]");
                    break;
                }
                case TOK_OPR_LTE: {
                    fwprintf(logStream, L"[LTE]");
                    break;
                }
                case TOK_OPR_AND_LOGIC: {
                    fwprintf(logStream, L"[AND(逻辑与)]");
                    break;
                }
                case TOK_OPR_REFER: {
                    fwprintf(logStream, L"[REF(或与)]");
                    break;
                }
                case TOK_OPR_OR: {
                    fwprintf(logStream, L"[OR]");
                    break;
                }
                case TOK_OPR_OR_LOGIC: {
                    fwprintf(logStream, L"[OR(逻辑或)]");
                    break;
                }
                case TOK_OPR_NOT: {
                    fwprintf(logStream, L"[NOT]");
                    break;
                }
                case TOK_OPR_INC: {
                    fwprintf(logStream, L"[INC]");
                    break;
                }
                case TOK_OPR_DEC: {
                    fwprintf(logStream, L"[DEC]");
                    break;
                }
                case TOK_OPR_LQUOTE: {
                    fwprintf(logStream, L"[LQUOTE]");
                    break;
                }
                case TOK_OPR_RQUOTE: {
                    fwprintf(logStream, L"[RQUOTE]");
                    break;
                }
                case TOK_OPR_LBRACE: {
                    fwprintf(logStream, L"[LBRACE]");
                    break;
                }
                case TOK_OPR_RBRACE: {
                    fwprintf(logStream, L"[RBRACE]");
                    break;
                }
                case TOK_OPR_LBRACKET: {
                    fwprintf(logStream, L"[LBRACKET]");
                    break;
                }
                case TOK_OPR_RBRACKET: {
                    fwprintf(logStream, L"[RBRACKET]");
                    break;
                }
                case TOK_OPR_COMMA: {
                    fwprintf(logStream, L"[COMMA]");
                    break;
                }
                case TOK_OPR_DOT: {
                    fwprintf(logStream, L"[DOT]");
                    break;
                }
                case TOK_OPR_COLON: {
                    fwprintf(logStream, L"[COLON]");
                    break;
                }
                case TOK_OPR_POINT: {
                    fwprintf(logStream, L"[POINT]");
                    break;
                }
            }
            fwprintf(logStream, L"(\"%ls\")", tokens->tokens[i].value ? tokens->tokens[i].value : L"(NULL)\0");
            if (tokens->tokens[i].type == TOK_END) {
                fwprintf(logStream, L"\n\t");
            } else {
                fwprintf(logStream, L" ");
            }
            fwprintf(logStream, L"\33[0m\n");
        }
    } else
        log(L"(NULL)\n");
    return;
}
#endif
#include <wctype.h>

static int hexValue(wchar_t c) noexcept {
    if (c >= L'0' && c <= L'9') return c - L'0';
    if (c >= L'a' && c <= L'f') return c - L'a' + 10;
    if (c >= L'A' && c <= L'F') return c - L'A' + 10;
    return -1;
}
// 字符串转义
wchar_t* escape(const wchar_t* src) noexcept {
    if (!src) return NULL;

    size_t len = wcslen(src);
    wchar_t* out = (wchar_t*)malloc(sizeof(wchar_t) * (len + 1));
    if (!out) return NULL;

    const wchar_t* p = src;
    wchar_t* q = out;

    while (*p) {
        if (*p != L'\\') {
            *q++ = *p++;
            continue;
        }

        p++;  // 跳过 '\'

        if (*p == L'n') {
            *q++ = L'\n';
            p++;
        } else if (*p == L'r') {
            *q++ = L'\r';
            p++;
        } else if (*p == L't') {
            *q++ = L'\t';
            p++;
        } else if (*p == L'\\') {
            *q++ = L'\\';
            p++;
        } else if (*p == L'0') {  // 八进制 \0xx
            int value = 0;
            int count = 0;
            while (count < 3 && *p >= L'0' && *p <= L'7') {
                value = (value << 3) + (*p - L'0');
                p++;
                count++;
            }
            *q++ = (wchar_t)value;
        } else if (*p == L'u') {
            p++;
            int value = 0;
            int digits = 0;

            while (digits < 4) {
                int hv = hexValue(*p);
                if (hv < 0) break;
                value = (value << 4) | hv;
                p++;
                digits++;
            }

            *q++ = (wchar_t)value;
        } else if (L'0' < *p && *p <= L'9') {
            int value = 0;
            int count = 0;
            while (count < 3 && *p >= L'0' && *p <= L'9') {
                value = (value << 3) + (*p - L'0');
                p++;
                count++;
            }
            *q++ = (wchar_t)value;
        } else {
            /* 未知转义：原样保留 */
            *q++ = L'\\';
            if (*p) *q++ = *p++;
        }
    }

    *q = L'\0';
    return out;
}

#undef MEM_FAIL
#undef ERR
#endif