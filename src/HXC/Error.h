#ifndef HXHLANG_SRC_HXC_ERROR_H
#define HXHLANG_SRC_HXC_ERROR_H
#include <locale.h>
#include <stdio.h>
#include <wchar.h>
#define ERROR_BUF_SIZE 1024
wchar_t errorMessageBuffer[ERROR_BUF_SIZE];  // 错误信息缓冲区
typedef enum ErrorType {
    ERR_GLOBAL_UNKOWN,        // 未知的全局定义
    ERR_NO_END,               // 语句没结尾
    ERR_CH_NO_END,            // 字符没结尾
    ERR_STR_NO_END,           // 字符串没结尾
    ERR_VAL,                  // 字面量写错了
    ERR_HUAKUOHAO_NOT_CLOSE,  // 花括号末正确闭合
    ERR_DEF_VAR,              // 定义变量语法错误
    ERR_VAR_REPEATED,         // 变量重复定义
    ERR_DEF_CLASS,
    ERR_DEF_CLASS_ACCESS,              // 定义类时访问权限修饰符使用错误
    ERR_DEF_CLASS_DOUBLE_DEFINED_SYM,  // 定义类时重复声明符号
    ERR_FUN,
    ERR_FUN_ARG,
    ERR_FUN_REPEATED,  // 函数重复定义
    ERR_TYPE,
    ERR_MAIN,
    ERR_COUNLD_NOT_FIND_PARENT,
    ERR_UNKOWN_TYPE,
    ERR_NO_MAIN,
    ERR_CANNOT_FIND_SYMBOL,
    ERR_EXP,
    ERR_OUT_OF_VALUE,    // 数值溢出
    ERR_CLASS_REPEATED,  // 类重复定义
    ERR_RET,             // 返回值错误 （语法错误）
    ERR_RET_VAL,         // 返回值错误
    ERR_UNKNOWN_TYPE,    // 未知类型
    ERR_REPEAT,
    ERR_IF,
    ERROR_UNCOMPLETED_CLASS,  // 类相互包含
    ERROR_INC_OR_DEC_OP_VAR,  // 非法自增/减操作数
    ERR_FOR,                  // for语句语法错误
    ERR_NO_VAR,
} ErrorType;
void initLocale(void) noexcept {
    // 设置Locale
    if (!setlocale(LC_ALL, "zh_CN.UTF-8")) {
        if (!setlocale(LC_ALL, "en_US.UTF-8")) {
            setlocale(LC_ALL, "C.UTF-8");
        }
    }
    // 设置宽字符流的定向
    fwide(stdout, 1);  // 1 = 宽字符定向
    return;
}

#ifdef LANG_zh_TW
void setError(ErrorType e, int errorLine, const wchar_t* errCode) noexcept {
    initLocale();
    switch (e) {
        case ERR_NO_END: {
            swprintf(errorMessageBuffer, ERROR_BUF_SIZE,
                     L"\33[31m[ERR]\33[0m語句缺少分號結尾喵(位於第%"
                     L"d行)\n "
                     L"\33[36m[NOTE]\33[0m後面應該是分號喵->\33[4m%"
                     L"ls\33[0m\n "
                     L"\33[36m[NOTE]\33["
                     L"0m類別體中變數或常數符號只能宣告,賦值犯規了喵。",
                     errorLine, errCode ? errCode : L" ");
            break;
        }
        case ERR_CH_NO_END: {
            swprintf(errorMessageBuffer, ERROR_BUF_SIZE,
                     L"\33[31m[ERR]\33[0m字元缺少結尾喵(位於第%d行)\n "
                     L"\33[36m[NOTE]\33[0m這個字元缺結尾->%ls\n",
                     errorLine, errCode ? errCode : L" ");
            break;
        }
        case ERR_STR_NO_END: {
            swprintf(errorMessageBuffer, ERROR_BUF_SIZE,
                     L"\33[31m[ERR]\33[0m雖然這個字串太短了喵，但它沒結尾喵(位於第%d行)\n "
                     L"\33[36m[NOTE]\33[0m這個字串缺結尾->%ls\n",
                     errorLine, errCode ? errCode : L" ");
            break;
        }

        case ERR_VAL: {
            swprintf(errorMessageBuffer, ERROR_BUF_SIZE,
                     L"\33[31m[ERR]\33[0m常值(Literal)錯誤了喵(位於第%d行)\n "
                     L"\33[36m[NOTE]\33[0m這個常值寫錯了->%ls\n",
                     errorLine, errCode ? errCode : L" ");
            break;
        }

        case ERR_HUAKUOHAO_NOT_CLOSE: {
            swprintf(errorMessageBuffer, ERROR_BUF_SIZE,
                     L"\33[31m[ERR]大括號未正確封閉喵\33[0m！(位於第%"
                     L"d行)\n "
                     L"\33[36m[NOTE]\33["
                     L"0m這個大括號沒有對應的右大括號->%ls\n",
                     errorLine, errCode ? errCode : L" ");
            break;
        }

        case ERR_DEF_VAR: {
            swprintf(errorMessageBuffer, ERROR_BUF_SIZE,
                     L"\33[31m[ERR]對定義變數語法錯誤了🥵\33[0m(位於第%"
                     L"d行)\n\33[36m["
                     L"NOTE]\33[0mDefineVariable::= "
                     L"<\"var\"><\":\"><id><\":\"><kw|id>;\n     "
                     L"定義變數::= "
                     L"<\"定義變數\"><\":\"><識別碼><\",\"><"
                     L"\"類型是\"><\":\"><"
                     L"識別碼|關鍵字>;\n",
                     errorLine);
            break;
        }

        case ERR_DEF_CLASS_ACCESS: {
            swprintf(errorMessageBuffer, ERROR_BUF_SIZE,
                     L"\33[31m[ERR]定義類別時存取權限修飾詞使用錯誤了喵\33["
                     L"0m(位於第%d行)"
                     L"\n\33[36m[NOTE]\33[0m 宣告類別成員::= "
                     L"\"[public\"|\"private\"|\"protected\"|"
                     L"\"公有成員\"|"
                     L"\"私有成員\"|\"受保護成員\" <\":\"> ] "
                     L"定義函式|宣告變數\n",
                     errorLine);
            break;
        }

        case ERR_DEF_CLASS_DOUBLE_DEFINED_SYM: {
            swprintf(errorMessageBuffer, ERROR_BUF_SIZE,
                     L"\33[31m[ERR]定義類別時重複宣告符號，犯規了喵\33[0m("
                     L"位於第%d行)\n\33[36m["
                     L"NOTE]\33[0m 此符號被重複宣告-> %ls\n",
                     errorLine, errCode ? errCode : L" ");
            break;
        }

        case ERR_DEF_CLASS: {
            swprintf(errorMessageBuffer, ERROR_BUF_SIZE,
                     L"\33[31m[ERR]定義類別的語法有誤喵\33[0m(位於第%"
                     L"d行)\n\33[36m[NOTE]\33["
                     L"0m 定義類別::= <\"定義類別\"> "
                     L"<\":\"> <id> [<\",\"> "
                     L"<\"父類別是\"> <\":\"> <id>]  <\"->\">"
                     L"<\"{\"> ... <\"}\">\n"
                     L"DefineClass ::= class: [<id> ->] <id> {...}  #<id>->中的id為父類別名\n",
                     errorLine);
            break;
        }

        case ERR_FUN: {
            swprintf(errorMessageBuffer, ERROR_BUF_SIZE,
                     L"\33[31m[ERR]笨蛋！定義函式的語法犯規了喵\33[0m(位於第%"
                     L"d行)\n\33[36m["
                     L"NOTE]\33[0m DefineFunction::= "
                     L"<\"fun\"><\":\"><id><\"(\"><args><\")\">[<\":\"><"
                     L"id|kw>]<\"->\"><"
                     L"\"{\">."
                     L"..<\"}\">\n定義函式::= "
                     L"<\"定義函式\"><\"：\"><識別碼><\"(\"><參數><\")"
                     L"\">[<\",\"><"
                     L"\"返回類型是\"><\"：\"><資料類型>]|[<\",\"><"
                     L"\"無返回類型\">] <\"->\"> <"
                     L"\"{\">...<\"}\">\n"
                     L"\33[36m[NOTE]\33[0m函式體內不可定義函式喵\n",
                     errorLine);
            break;
        }

        case ERR_FUN_ARG: {
            swprintf(errorMessageBuffer, ERROR_BUF_SIZE,
                     L"\33[31m[ERR]對定義函式的參數的語法犯錯誤了\33["
                     L"0m(位於第%d行)\n\33["
                     L"36m[NOTE]\33[0m Argument::= "
                     L"<id><\":\"><id|kw>\n參數::= "
                     L"<識別碼><\":\"><識別碼|關鍵字>",
                     errorLine);
            break;
        }

        case ERR_TYPE: {
            swprintf(errorMessageBuffer, ERROR_BUF_SIZE,
                     L"\33[31m[ERR]對類型拼寫犯錯誤了\33[0m(擺在第%d行)"
                     L"\n\33[36m["
                     L"NOTE]\33[0m ArrayType::= "
                     L"<id|kw>[<\"[\"><\"]\">...]\n參數::= "
                     L"<識別碼|關鍵字>[<\"[\"><\"]\">...]",
                     errorLine);
            break;
        }

        case ERR_MAIN: {
            swprintf(errorMessageBuffer, ERROR_BUF_SIZE,
                     L"\33[31m[ERR]對主函式犯錯誤了\33[0m(位於第%d行)"
                     L"\n\33[36m[NOTE]"
                     L"\33[0m 主函式不能多載(Overload)！\n",
                     errorLine);
            break;
        }

        case ERR_COUNLD_NOT_FIND_PARENT: {
            swprintf(errorMessageBuffer, ERROR_BUF_SIZE,
                     L"\33[31m[ERR]找不到父類別喵\33[0m(位於第%d行)"
                     L"\n\33[36m[NOTE]\33["
                     L"0m 這個父類別找不到->%ls\n",
                     errorLine, errCode);
            break;
        }

        case ERR_UNKOWN_TYPE: {
            swprintf(errorMessageBuffer, ERROR_BUF_SIZE,
                     L"\33[31m[ERR]類型？不知道哦\33[0m(位於第%d行)"
                     L"\n\33[36m[NOTE]\33[0m "
                     L"似乎沒有這個類型喵->%ls\n",
                     errorLine, errCode);
            break;
        }

        case ERR_NO_MAIN: {
            swprintf(errorMessageBuffer, ERROR_BUF_SIZE, L"\33[31m[ERR]沒有主函式喵\33[0m\n");
            break;
        }

        case ERR_CANNOT_FIND_SYMBOL: {
            swprintf(errorMessageBuffer, ERROR_BUF_SIZE, L"\33[31m[ERR]找不到符號(%ls)喵\33[0m\n",
                     errCode ? errCode : L"(null)");
            break;
        }

        case ERR_EXP: {
            swprintf(errorMessageBuffer, ERROR_BUF_SIZE, L"\33[31m[ERR]運算式錯誤(%ls)了喵\33[0m\n",
                     errCode ? errCode : L"(null)");
            break;
        }

        case ERR_OUT_OF_VALUE: {
            swprintf(errorMessageBuffer, ERROR_BUF_SIZE,
                     L"\33[31m[ERR]啊♡~數值太......太大了......"
                     L"要溢出來了♡......(%ls)"
                     L"\33[0m\n",
                     errCode ? errCode : L"？？？？");
            break;
        }

        case ERR_GLOBAL_UNKOWN: {
            swprintf(errorMessageBuffer, ERROR_BUF_SIZE, L"\33[31m[ERR]未知的全域定義！\33[0m(位於第%d行)\n", errorLine);
            break;
        }

        case ERR_FUN_REPEATED: {
            swprintf(errorMessageBuffer, ERROR_BUF_SIZE, L"\33[31m[ERR]函式重複定義了喵\33[0m(位於第%d行)\n", errorLine);
            break;
        }

        case ERR_CLASS_REPEATED: {
            swprintf(errorMessageBuffer, ERROR_BUF_SIZE, L"\33[31m[ERR]類別重複定義了喵\33[0m(位於第%d行)\n", errorLine);
            break;
        }

        case ERR_RET: {
            swprintf(errorMessageBuffer, ERROR_BUF_SIZE,
                     L"\33[31m[ERR]笨蛋！返回語句語法錯誤了喵\33[0m(位於第%"
                     L"d行)\n\33[36m["
                     L"NOTE]\33[0m 返回::= ret:exp | 返回：exp\n",
                     errorLine);
            break;
        }
        case ERR_UNKNOWN_TYPE: {
            swprintf(errorMessageBuffer, ERROR_BUF_SIZE,
                     L"\33[31m[ERR]這是什麼類型喵？  "
                     L"%ls\33[0m(位於第%d行)\n",
                     errCode ? errCode : L" ", errorLine);
            break;
        }
        case ERR_RET_VAL: {
            swprintf(errorMessageBuffer, ERROR_BUF_SIZE, L"\33[31m[ERR]回傳值錯誤了喵\33[0m(位於第%d行)\n", errorLine);
            break;
        }
        case ERR_REPEAT: {
            swprintf(errorMessageBuffer, ERROR_BUF_SIZE,
                     L"\33[31m[ERR]迴圈語句錯誤了喵\33[0m(位於第%d行)"
                     L"\n\33[36m[NOTE]"
                     L"\33[0m 迴圈 ::= repeat-> 語句|區塊 [until(exp)]\n",
                     errorLine);
            break;
        }

        case ERR_IF: {
            swprintf(errorMessageBuffer, ERROR_BUF_SIZE,
                     L"\33[31m[ERR]對條件判斷語句犯錯誤了喵～\33["
                     L"0m(位於第%d行)\n\33["
                     L"36m[NOTE]\33[0m 條件判斷 ::= if: 運算式 -> "
                     L"語句|區塊\n",
                     errorLine);
            break;
        }

        case ERR_VAR_REPEATED: {
            swprintf(errorMessageBuffer, ERROR_BUF_SIZE,
                     L"\33[31m[ERR]變數不能重複定義的喵～笨蛋！\33["
                     L"0m(擺在第%d行)\n",
                     errorLine);
            break;
        }
        case ERROR_UNCOMPLETED_CLASS: {
            swprintf(errorMessageBuffer, ERROR_BUF_SIZE,
                     L"\33[31m[ERR]雜魚~你的類別互相包含了喵~("
                     L"如：類別A裡有類別B類型成員，而類別B裡又有類別A類型成員)"
                     L"如果不報錯，佔的記憶體就太......太大了。電腦會壞..."
                     L"...壞掉了...."
                     L".\33[0m(位於第%d行)\n",
                     errorLine);
            break;
        }
    }
    return;
}
#else
void setError(ErrorType e, int errorLine, const wchar_t* errCode) noexcept {
    initLocale();
    switch (e) {
        case ERR_NO_END: {
            swprintf(errorMessageBuffer, ERROR_BUF_SIZE,
                     L"\33[31m[ERR]\33[0m语句缺少分号结尾喵(位于第%"
                     L"d行)\n "
                     L"\33[36m[NOTE]\33[0m后面应该是分号喵->\33[4m%"
                     L"ls\33[0m\n "
                     L"\33[36m[NOTE]\33["
                     L"0m类体中变量或常量符号只能声明,赋值犯规了喵。",
                     errorLine, errCode ? errCode : L" ");
            break;
        }
        case ERR_CH_NO_END: {
            swprintf(errorMessageBuffer, ERROR_BUF_SIZE,
                     L"\33[31m[ERR]\33[0m字符缺少结尾喵(位于第%d行)\n "
                     L"\33[36m[NOTE]\33[0m这个字符缺结尾->%ls\n",
                     errorLine, errCode ? errCode : L" ");
            break;
        }
        case ERR_STR_NO_END: {
            swprintf(errorMessageBuffer, ERROR_BUF_SIZE,
                     L"\33[31m[ERR]\33[0m虽然这个字符串太短了喵，但它沒结尾喵(位于第%d行)\n "
                     L"\33[36m[NOTE]\33[0m这个字符串缺结尾->%ls\n",
                     errorLine, errCode ? errCode : L" ");
            break;
        }

        case ERR_VAL: {
            swprintf(errorMessageBuffer, ERROR_BUF_SIZE,
                     L"\33[31m[ERR]\33[0m字面量错误了喵(位于第%d行)\n "
                     L"\33[36m[NOTE]\33[0m这个字面量写错了->%ls\n",
                     errorLine, errCode ? errCode : L" ");
            break;
        }

        case ERR_HUAKUOHAO_NOT_CLOSE: {
            swprintf(errorMessageBuffer, ERROR_BUF_SIZE,
                     L"\33[31m[ERR]花括号未正确闭合喵\33[0m！(位于第%"
                     L"d行)\n "
                     L"\33[36m[NOTE]\33["
                     L"0m这个花括号没有对应的闭花括号->%ls\n",
                     errorLine, errCode ? errCode : L" ");
            break;
        }

        case ERR_DEF_VAR: {
            swprintf(errorMessageBuffer, ERROR_BUF_SIZE,
                     L"\33[31m[ERR]对定义变量语法错误了🥵\33[0m(位于第%"
                     L"d行)\n\33[36m["
                     L"NOTE]\33[0mDefineVariable::= "
                     L"<\"var\"><\":\"><id><\":\"><kw|id>;\n     "
                     L"定义变量::= "
                     L"<\"定义变量\"><\":\"><标识符><\",\"><"
                     L"\"类型是\"><\":\"><"
                     L"标识符|关键字>;\n",
                     errorLine);
            break;
        }

        case ERR_DEF_CLASS_ACCESS: {
            swprintf(errorMessageBuffer, ERROR_BUF_SIZE,
                     L"\33[31m[ERR]定义类时访问权限修饰符使用错误了喵\33["
                     L"0m(位于第%d行)"
                     L"\n\33[36m[NOTE]\33[0m 声明类成员::= "
                     L"\"[public\"|\"private\"|\"protected\"|"
                     L"\"公有成员\"|"
                     L"\"私有成员\"|\"受保护成员\" <\":\"> ] "
                     L"定义函数|声明变量\n",
                     errorLine);
            break;
        }

        case ERR_DEF_CLASS_DOUBLE_DEFINED_SYM: {
            swprintf(errorMessageBuffer, ERROR_BUF_SIZE,
                     L"\33[31m[ERR]定义类时重复声明符号，犯规了喵\33[0m("
                     L"位于第%d行)\n\33[36m["
                     L"NOTE]\33[0m 此符号被重复声明-> %ls\n",
                     errorLine, errCode ? errCode : L" ");
            break;
        }

        case ERR_DEF_CLASS: {
            swprintf(errorMessageBuffer, ERROR_BUF_SIZE,
                     L"\33[31m[ERR]定义类的语法有误喵\33[0m(位于第%"
                     L"d行)\n\33[36m[NOTE]\33["
                     L"0m 定义类::= <\"定义类\"> "
                     L"<\":\"> <id> [<\",\"> "
                     L"<\"父类是\"> <\":\"> <id>]  <\"->\">"
                     L"<\"{\"> ... <\"}\">\n"
                     L"DefineClass ::= class: [<id> ->] <id> {...}  #<id>->中的id为父类名\n",
                     errorLine);
            break;
        }

        case ERR_FUN: {
            swprintf(errorMessageBuffer, ERROR_BUF_SIZE,
                     L"\33[31m[ERR]笨蛋！定义函数的语法犯规了喵\33[0m(位于第%"
                     L"d行)\n\33[36m["
                     L"NOTE]\33[0m DefineFunction::= "
                     L"<\"fun\"><\":\"><id><\"(\"><args><\")\">[<\":\"><"
                     L"id|kw>]<\"->\"><"
                     L"\"{\">."
                     L"..<\"}\">\n定义函数::= "
                     L"<\"定义函数\"><\"：\"><标识符><\"(\"><参数><\")"
                     L"\">[<\",\"><"
                     L"\"返回类型是\"><\"：\"><数据类型>]|[<\",\"><"
                     L"\"无返回类型\">] <\"->\"> <"
                     L"\"{\">...<\"}\">"
                     L"\n\33[36m[NOTE]\33[0m函数体内不可定义函数喵\n",
                     errorLine);
            break;
        }

        case ERR_FUN_ARG: {
            swprintf(errorMessageBuffer, ERROR_BUF_SIZE,
                     L"\33[31m[ERR]对定义函数的参数的语法犯错误了\33["
                     L"0m(位于第%d行)\n\33["
                     L"36m[NOTE]\33[0m Argument::= "
                     L"<id><\":\"><id|kw>\n参数::= "
                     L"<标识符><\":\"><标识符|关键字>",
                     errorLine);
            break;
        }

        case ERR_TYPE: {
            swprintf(errorMessageBuffer, ERROR_BUF_SIZE,
                     L"\33[31m[ERR]对类型拼写犯错误了\33[0m(位于第%d行)"
                     L"\n\33[36m["
                     L"NOTE]\33[0m ArrayType::= "
                     L"<id|kw>[<\"[\"><\"]\">...]\n参数::= "
                     L"<标识符|关键字>[<\"[\"><\"]\">...]",
                     errorLine);
            break;
        }

        case ERR_MAIN: {
            swprintf(errorMessageBuffer, ERROR_BUF_SIZE,
                     L"\33[31m[ERR]对主函数犯错误了\33[0m(位于第%d行)"
                     L"\n\33[36m[NOTE]"
                     L"\33[0m 主函数不能重载！\n",
                     errorLine);
            break;
        }

        case ERR_COUNLD_NOT_FIND_PARENT: {
            swprintf(errorMessageBuffer, ERROR_BUF_SIZE,
                     L"\33[31m[ERR]找不到父类喵\33[0m(位于第%d行)"
                     L"\n\33[36m[NOTE]\33["
                     L"0m 这个父类找不到->%ls\n",
                     errorLine, errCode);
            break;
        }

        case ERR_UNKOWN_TYPE: {
            swprintf(errorMessageBuffer, ERROR_BUF_SIZE,
                     L"\33[31m[ERR]类型？不知道哦\33[0m(位于第%d行)"
                     L"\n\33[36m[NOTE]\33[0m "
                     L"似乎没有这个类型喵->%ls\n",
                     errorLine, errCode);
            break;
        }

        case ERR_NO_MAIN: {
            swprintf(errorMessageBuffer, ERROR_BUF_SIZE, L"\33[31m[ERR]没有主函数喵\33[0m\n");
            break;
        }

        case ERR_CANNOT_FIND_SYMBOL: {
            swprintf(errorMessageBuffer, ERROR_BUF_SIZE, L"\33[31m[ERR]找不到符号(%ls)喵\33[0m\n",
                     errCode ? errCode : L"(null)");
            break;
        }

        case ERR_EXP: {
            swprintf(errorMessageBuffer, ERROR_BUF_SIZE, L"\33[31m[ERR]表达式错误(%ls)了喵\33[0m\n",
                     errCode ? errCode : L"(null)");
            break;
        }

        case ERR_OUT_OF_VALUE: {
            swprintf(errorMessageBuffer, ERROR_BUF_SIZE,
                     L"\33[31m[ERR]啊♡~数值太......太大了......"
                     L"要溢出来了♡......(%ls)"
                     L"\33[0m\n",
                     errCode ? errCode : L"？？？？");
            break;
        }

        case ERR_GLOBAL_UNKOWN: {
            swprintf(errorMessageBuffer, ERROR_BUF_SIZE, L"\33[31m[ERR]未知的全局定义！\33[0m(位于第%d行)\n", errorLine);
            break;
        }

        case ERR_FUN_REPEATED: {
            swprintf(errorMessageBuffer, ERROR_BUF_SIZE, L"\33[31m[ERR]函数重复定义了喵\33[0m(位于第%d行)\n", errorLine);
            break;
        }

        case ERR_CLASS_REPEATED: {
            swprintf(errorMessageBuffer, ERROR_BUF_SIZE, L"\33[31m[ERR]类重复定义了喵\33[0m(位于第%d行)\n", errorLine);
            break;
        }

        case ERR_RET: {
            swprintf(errorMessageBuffer, ERROR_BUF_SIZE,
                     L"\33[31m[ERR]笨蛋！返回语句语法错误了喵\33[0m(位于第%"
                     L"d行)\n\33[36m["
                     L"NOTE]\33[0m 返回::= ret:exp | 返回：exp\n",
                     errorLine);
            break;
        }
        case ERR_UNKNOWN_TYPE: {
            swprintf(errorMessageBuffer, ERROR_BUF_SIZE,
                     L"\33[31m[ERR]这是什么类型喵？  "
                     L"%ls\33[0m(位于第%d行)\n",
                     errCode ? errCode : L" ", errorLine);
            break;
        }
        case ERR_NO_VAR: {
            swprintf(errorMessageBuffer, ERROR_BUF_SIZE,
                     L"\33[31m[ERR]这是什么符号喵？（好奇）  "
                     L"%ls\33[0m(位于第%d行)\n",
                     errCode ? errCode : L" ", errorLine);
            break;
        }
        case ERR_RET_VAL: {
            swprintf(errorMessageBuffer, ERROR_BUF_SIZE, L"\33[31m[ERR]返回值错误了喵\33[0m(位于第%d行)\n", errorLine);
            break;
        }
        case ERR_REPEAT: {
            swprintf(errorMessageBuffer, ERROR_BUF_SIZE,
                     L"\33[31m[ERR]循环语句错误了喵\33[0m(位于第%d行)"
                     L"\n\33[36m[NOTE]"
                     L"\33[0m 循环 ::= repeat-> 语句|块 [until(exp)]\n",
                     errorLine);
            break;
        }

        case ERR_IF: {
            swprintf(errorMessageBuffer, ERROR_BUF_SIZE,
                     L"\33[31m[ERR]对条件判断语句犯错误了喵～\33["
                     L"0m(位于第%d行)\n\33["
                     L"36m[NOTE]\33[0m 条件判断 ::= if: 表达式 -> "
                     L"语句|块\n",
                     errorLine);
            break;
        }

        case ERR_VAR_REPEATED: {
            swprintf(errorMessageBuffer, ERROR_BUF_SIZE,
                     L"\33[31m[ERR]变量不能重复定义的喵～笨蛋！\33["
                     L"0m(位于第%d行)\n",
                     errorLine);
            break;
        }
        case ERROR_UNCOMPLETED_CLASS: {
            swprintf(errorMessageBuffer, ERROR_BUF_SIZE,
                     L"\33[31m[ERR]杂鱼~你的类相互包含了喵~("
                     L"如：类A里有类B类型成员，而类B里又有类A类型成员)"
                     L"如果不报错，占的内存就太......太大了。电脑会坏..."
                     L"...坏掉了...."
                     L".\33[0m(位于第%d行)\n",
                     errorLine);
            break;
        }
        case ERROR_INC_OR_DEC_OP_VAR: {
            swprintf(errorMessageBuffer, ERROR_BUF_SIZE,
                     L"\33[31m[ERR]该类型的变量是不支持自增或自减的喵～笨蛋！\33["
                     L"0m(位于第%d行)\n",
                     errorLine);
            break;
        }
        case ERR_FOR: {
            swprintf(errorMessageBuffer, ERROR_BUF_SIZE,
                     L"\33[31m[ERR]for语句语法错误了喵～\33["
                     L"0m(位于第%d行)\n\33["
                     L"36m[NOTE]\33[0m for语句 ::= for: id(tmp):id(arr) -> "
                     L"语句|块\n",
                     errorLine);
            break;
        }
    }
    return;
}
#endif
#endif