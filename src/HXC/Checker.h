#ifndef HXHLANG_SRC_HXC_CHECKER_H
#define HXHLANG_SRC_HXC_CHECKER_H
#include <wchar.h>

#include "IR.h"
#include "SymbolTable.h"

extern ASTNode* parseExpression(Token* exp, int* index, int size, FunCallPitchTable& pitchTable, SymbolTable* table,
                                std::vector<SymbolTable>& outsideTable, int localeScopeIndex, int* err) noexcept;
void freeAST(ASTNode* node) noexcept;
inline IR_Class* getClassByName(const wchar_t* name, IR_Program* currentIRProgram);
static int getVarIndex(const wchar_t* name, SymbolTable* table);
/**
 * 在分析到新函数时检查函数是否重复定义，包含对错误的处理
 * @return 错误为-255，-1为未声明，否则为声明在表中的索引
 */
int isFunctionRepeatDefine(IR_Function* fun, std::vector<IR_Function*>& table) {
    if (!fun) return -1;
    for (int i = 0; i < table.size(); i++) {
        if (!table[i]) continue;
        if (wcscmp(fun->name, table[i]->name) == 0) {
            if (fun->paramCount == table[i]->paramCount) {
                // 参数个数相同，继续检查参数类型
                bool allParamsMatch = true;
                for (int j = 0; j < fun->paramCount; j++) {
                    if (fun->params[j].type.kind != table[i]->params[j].type.kind) {
                        allParamsMatch = false;
                        break;
                    } else {
                        if (fun->params[j].type.kind == IR_DT_CUSTOM) {
                            // 自定义类型，继续检查名称
                            if (wcscmp(fun->params[j].type.customTypeName, table[i]->params[j].type.customTypeName) != 0) {
                                allParamsMatch = false;
                                break;
                            }
                        }
                    }
                }
                if (allParamsMatch) {
                    // 检查是不是声明
                    if (table[i]->bodyTokens == NULL || table[i]->body_token_count == 0) {
                        return i;
                    }
                    // 函数重复定义
                    setError(ERR_FUN_REPEATED, fun->bodyTokens[0].line, fun->name);
                    return -255;
                }
            }
        }
    }
    return -1;
}

/**
 * 检查类是否重复定义
 * @return 255为重复定义，0为未重复定义
 */
int isClassRepeatDefine(IR_Class* cls, IR_Class** table, int table_size) {
    if (!cls || !table) return -1;
    for (int i = 0; i < table_size; i++) {
        if (!table[i]) continue;
        if (wcscmp(cls->name, table[i]->name) == 0) {
            // 类重复定义
            setError(ERR_CLASS_REPEATED, cls->line, cls->name);
            return 255;
        }
    }
    return 0;
}
/*类型推导*/
int deduceFunctionReturnTypes(IR_Program* program) {
#ifdef HX_DEBUG
    log(L"进行函数返回类型推导.....");
#endif
    if (!program) return -1;
    for (int i = 0; i < program->functions.size(); i++) {
        IR_Function* fun = program->functions[i];
        // 已经知道类型
        if (fun->isReturnTypeKnown || fun->body_token_count == 0) continue;
        // 简单记录变量
        SymbolTable tempLocalScope;
        tempLocalScope.fun = program->functions;
        if (fun->paramCount > 0 && fun->params != NULL) {
#ifdef HX_DEBUG
            log(L"返回类型推导.->填入参数");
#endif
            for (int i = 0; i < fun->paramCount; i++) {
                Symbol param;
                param.name = wcsdup(fun->params[i].name);
                param.type = fun->params[i].type;
                tempLocalScope.vars.push_back(param);
            }
        }
        std::vector<SymbolTable> tempOutsideScopes;
        tempOutsideScopes.push_back(tempLocalScope);

        SymbolTable* tempLocalScopePtr = &(tempOutsideScopes.back());
        FunCallPitchTable feckPitchTable;
        int err = 0;
        unsigned int blockNum = 0U;
        // 遍历一遍，查找ret和简单记录变量
        for (int index = 1; index < fun->body_token_count - 1; index++) {
            Token& currentToken = fun->bodyTokens[index];

            if (currentToken.type == TOK_OPR_LBRACE) {
                SymbolTable newTempLocalScope = {};
                newTempLocalScope.fun = program->functions;
                tempOutsideScopes.push_back(newTempLocalScope);
                tempLocalScopePtr = &(tempOutsideScopes.back());
                blockNum++;
            }
            if (currentToken.type == TOK_OPR_RBRACE) {
                blockNum--;
                tempLocalScopePtr = &(tempOutsideScopes.at(blockNum));
            }
            if (wcscmp(currentToken.value, L"var") == 0) {  // var:id[:type][=exp];
                Symbol newVar = {};
                if (index + 1 >= fun->body_token_count) {
                    setError(ERR_DEF_VAR, currentToken.line, NULL);
                    return 255;
                }
                index++;  // 指向冒号
                if (fun->bodyTokens[index].type != TOK_OPR_COLON) {
                    setError(ERR_DEF_VAR, currentToken.line, NULL);
                    return 255;
                }
                if (index + 1 >= fun->body_token_count) {
                    setError(ERR_DEF_VAR, currentToken.line, NULL);
                    return 255;
                }
                index++;  // 指向标识符
                if (fun->bodyTokens[index].type != TOK_ID) {
                    setError(ERR_DEF_VAR, currentToken.line, NULL);
                    return 255;
                }
                newVar.name = (wchar_t*)calloc(wcslen(fun->bodyTokens[index].value) + 1, sizeof(wchar_t));
                if (!(newVar.name)) {
                    return -1;
                }
                wcscpy(newVar.name, fun->bodyTokens[index].value);
                // 检查变量唯一性
                if (getVarIndex(newVar.name, tempLocalScopePtr) != -1) {
                    setError(ERR_VAR_REPEATED, currentToken.line, NULL);
                    return 255;
                }
                if (index + 1 >= fun->body_token_count) {
                    setError(ERR_DEF_VAR, currentToken.line, NULL);
                    return 255;
                }
                index++;  // 指向结束标志或:或=
                if (fun->bodyTokens[index].type == TOK_OPR_COLON) {
                    newVar.isTypeKnown = true;
                    if (index + 1 >= fun->body_token_count) {
                        setError(ERR_DEF_VAR, currentToken.line, NULL);
                        return 255;
                    }
                    index++;  // 指向类型名
                    if (fun->bodyTokens[index].type != TOK_ID && fun->bodyTokens[index].type != TOK_KW) {
                        setError(ERR_DEF_VAR, currentToken.line, NULL);
                        return 255;
                    }
                }
                // 提前越界检查，仅检查
                if (index + 1 >= fun->body_token_count) {
                    setError(ERR_DEF_VAR, currentToken.line, NULL);
                    return 255;
                }
                IR_Function* function = fun;
                IR_Program* currentProgram = program;
                if (wcscmp(function->bodyTokens[index].value, L"int") == 0 ||
                    wcscmp(function->bodyTokens[index].value, L"整型") == 0) {
                    newVar.type.kind = IR_DT_INT;
                    newVar.size = 4;
                    // int&
                    if (function->bodyTokens[index + 1].type == TOK_OPR_REFER) {
                        newVar.type.kind = IR_DT_INT_REFER;
                        newVar.size = 4;
                        index++;
                        // int[]
                    } else if (function->bodyTokens[index + 1].type == TOK_OPR_LBRACKET) {
                        if (index + 2 >= function->body_token_count) {
                            setError(ERR_TYPE, currentToken.line, NULL);
                            return 255;
                        }
                        index++;
                        if (!(function->bodyTokens[index + 1].type == TOK_OPR_RBRACKET ||
                              function->bodyTokens[index + 2].type == TOK_OPR_RBRACKET)) {
#ifdef HX_DEBUG
                            log("index+1: %ls,   index+2: %ls", function->bodyTokens[index + 1].value,
                                function->bodyTokens[index + 2].value);
#endif
                            setError(ERR_TYPE, currentToken.line, NULL);
                            return 255;
                        }
                        int arrSize = -1;
                        if (function->bodyTokens[index + 1].type == TOK_VAL) {
                            arrSize = wcstol(function->bodyTokens[index + 1].value, nullptr, 0);
                            newVar.type.arrayLength = arrSize;
                        }
                        newVar.type.kind = IR_DT_INT_ARR;
                        newVar.size = 4;
                        index += 2;
                    }
                } else if (wcscmp(function->bodyTokens[index].value, L"float") == 0 ||
                           wcscmp(function->bodyTokens[index].value, L"浮点型") == 0) {
                    newVar.type.kind = IR_DT_FLOAT;
                    newVar.size = 8;
                    if (function->bodyTokens[index + 1].type == TOK_OPR_REFER) {
                        newVar.type.kind = IR_DT_FLOAT_REFER;
                        newVar.size = 4;  // 实为void*大小
                        index++;
                    } else if (function->bodyTokens[index + 1].type == TOK_OPR_LBRACKET) {
                        if (index + 2 >= function->body_token_count) {
                            setError(ERR_TYPE, currentToken.line, NULL);
                            return 255;
                        }
                        index++;
                        if (function->bodyTokens[index + 1].type != TOK_OPR_RBRACKET &&
                            function->bodyTokens[index + 2].type != TOK_OPR_RBRACKET) {
                            setError(ERR_TYPE, currentToken.line, NULL);
                            return 255;
                        }
                        int arrSize = -1;
                        if (function->bodyTokens[index + 1].type == TOK_VAL) {
                            arrSize = wcstol(function->bodyTokens[index + 1].value, nullptr, 0);
                            newVar.type.arrayLength = arrSize;
                        }
                        newVar.type.kind = IR_DT_FLOAT_ARR;
                        newVar.size = 4;
                        index += 2;
                    }
                } else if (wcscmp(function->bodyTokens[index].value, L"char") == 0 ||
                           wcscmp(function->bodyTokens[index].value, L"字符型") == 0) {
                    newVar.type.kind = IR_DT_CHAR;
                    newVar.size = 2;
                    if (function->bodyTokens[index + 1].type == TOK_OPR_REFER) {
                        newVar.size = 4;
                        newVar.type.kind = IR_DT_CHAR_REFER;
                        index++;
                    } else if (function->bodyTokens[index + 1].type == TOK_OPR_LBRACKET) {
                        if (index + 2 >= function->body_token_count) {
                            setError(ERR_TYPE, currentToken.line, NULL);
                            return 255;
                        }
                        index++;
                        if (function->bodyTokens[index + 1].type != TOK_OPR_RBRACKET &&
                            function->bodyTokens[index + 2].type != TOK_OPR_RBRACKET) {
                            setError(ERR_TYPE, currentToken.line, NULL);
                            return 255;
                        }
                        int arrSize = -1;
                        if (function->bodyTokens[index + 1].type == TOK_VAL) {
                            arrSize = wcstol(function->bodyTokens[index + 1].value, nullptr, 0);
                            newVar.type.arrayLength = arrSize;
                        }
                        newVar.type.kind = IR_DT_CHAR_ARR;
                        newVar.size = 4;
                        index += 2;
                    }
                } else if (wcscmp(function->bodyTokens[index].value, L"str") == 0 ||
                           wcscmp(function->bodyTokens[index].value, L"字符串型") == 0) {
                    newVar.type.kind = IR_DT_STRING;
                    newVar.size = 4;
                    if (function->bodyTokens[index + 1].type == TOK_OPR_REFER) {
                        newVar.type.kind = IR_DT_STRING_REFER;
                        index++;
                    } else if (function->bodyTokens[index + 1].type == TOK_OPR_LBRACKET) {
                        if (index + 2 >= function->body_token_count) {
                            setError(ERR_TYPE, currentToken.line, NULL);
                            return 255;
                        }
                        index++;
                        if (function->bodyTokens[index + 1].type != TOK_OPR_RBRACKET &&
                            function->bodyTokens[index + 2].type != TOK_OPR_RBRACKET) {
                            setError(ERR_TYPE, currentToken.line, NULL);
                            return 255;
                        }
                        int arrSize = -1;
                        if (function->bodyTokens[index + 1].type == TOK_VAL) {
                            arrSize = wcstol(function->bodyTokens[index + 1].value, nullptr, 0);
                            newVar.type.arrayLength = arrSize;
                        }
                        newVar.type.kind = IR_DT_STRING_ARR;
                        index += 2;
                    }
                }
                if (function->bodyTokens[index].type == TOK_ID) {
                    newVar.type.kind = IR_DT_CUSTOM;
                    if (getClassByName(function->bodyTokens[index].value, currentProgram) == NULL) {
                        setError(ERR_UNKNOWN_TYPE, function->bodyTokens[index].line, function->bodyTokens[index].value);
                        return 255;
                    }
                    newVar.type.customTypeName =
                        (wchar_t*)calloc(wcslen(function->bodyTokens[index].value) + 1, sizeof(wchar_t));
                    if (!(newVar.type.customTypeName)) {
                        return -1;
                    }
                    wcscpy(newVar.type.customTypeName, function->bodyTokens[index].value);
                    if (function->bodyTokens[index + 1].type == TOK_OPR_REFER) {
                        newVar.type.kind = IR_DT_CUSTOM_REFER;
                        index++;
                    } else if (function->bodyTokens[index + 1].type == TOK_OPR_LBRACKET) {
                        if (index + 2 >= function->body_token_count) {
                            setError(ERR_TYPE, currentToken.line, NULL);
                            return 255;
                        }
                        index++;
                        if (function->bodyTokens[index + 1].type != TOK_OPR_RBRACKET &&
                            function->bodyTokens[index + 2].type != TOK_OPR_RBRACKET) {
                            setError(ERR_TYPE, currentToken.line, NULL);
                            return 255;
                        }
                        int arrSize = -1;
                        if (function->bodyTokens[index + 1].type == TOK_VAL) {
                            arrSize = wcstol(function->bodyTokens[index + 1].value, nullptr, 0);
                            newVar.type.arrayLength = arrSize;
                        }
                        newVar.type.kind = IR_DT_CUSTOM_ARR;
                        index += 2;
                    }
                }
                tempLocalScopePtr->vars.push_back(newVar);
            } else if (wcscmp(currentToken.value, L"ret") == 0 || wcscmp(currentToken.value, L"返回") == 0) {
                if (index + 1 >= fun->body_token_count) {
                    setError(ERR_RET, currentToken.line, NULL);
                    return 255;
                }
                index++;  // 指向冒号
                if (fun->bodyTokens[index].type == TOK_END) {
                    // 无返回值
                    fun->isReturnTypeKnown = true;
                    fun->returnType.kind = IR_DT_VOID;
                    return 0;
                }
                //:
                if (fun->bodyTokens[index].type != TOK_OPR_COLON) {
                    setError(ERR_RET, currentToken.line, NULL);
                    return 255;
                }
                index++;  // 指向表达式起始位置
                ASTNode* expNode = parseExpression(fun->bodyTokens, &index, fun->body_token_count, feckPitchTable,
                                                   &tempLocalScope, tempOutsideScopes, blockNum, &err);
                if (err != 0 || !expNode) {
                    return 255;
                }
                fun->isReturnTypeKnown = true;
                fun->returnType = expNode->resultType;
                index++;
                if (fun->bodyTokens[index].type != TOK_END) {
                    setError(ERR_RET, currentToken.line, NULL);
                    freeAST(expNode);
                    return 255;
                }
                freeAST(expNode);
            }
        }
        // 扫完了还是没找到ret，默认是void
        if (!fun->isReturnTypeKnown) {
            fun->returnType.kind = IR_DT_VOID;
            fun->isReturnTypeKnown = true;
        }
#ifdef HX_DEBUG
        switch (fun->returnType.kind) {
            case IR_DT_VOID:
                log(L"类型推导：%ls -> %ls", fun->name, L"void");
                break;
            case IR_DT_INT:
                log(L"类型推导：%ls -> %ls", fun->name, L"int");
                break;
            case IR_DT_INT_ARR:
                log(L"类型推导：%ls -> %ls", fun->name, L"int[]");
                break;
            case IR_DT_INT_REFER:
                log(L"类型推导：%ls -> %ls", fun->name, L"int&");
                break;
            case IR_DT_CHAR:
                log(L"类型推导：%ls -> %ls", fun->name, L"char");
                break;
            case IR_DT_CHAR_ARR:
                log(L"类型推导：%ls -> %ls", fun->name, L"char[]");
                break;
            case IR_DT_CHAR_REFER:
                log(L"类型推导：%ls -> %ls", fun->name, L"char&");
                break;
            case IR_DT_FLOAT:
                log(L"类型推导：%ls -> %ls", fun->name, L"float");
                break;
            case IR_DT_FLOAT_ARR:
                log(L"类型推导：%ls -> %ls", fun->name, L"float[]");
                break;
            case IR_DT_FLOAT_REFER:
                log(L"类型推导：%ls -> %ls", fun->name, L"float&");
                break;
            case IR_DT_STRING:
                log(L"类型推导：%ls -> %ls", fun->name, L"str");
                break;
            case IR_DT_STRING_ARR:
                log(L"类型推导：%ls -> %ls", fun->name, L"str[]");
                break;
            case IR_DT_STRING_REFER:
                log(L"类型推导：%ls -> %ls", fun->name, L"str&");
                break;
            case IR_DT_CUSTOM:
                log(L"类型推导：%ls -> %ls", fun->name, fun->returnType.customTypeName);
                break;
            case IR_DT_CUSTOM_ARR:
                log(L"类型推导：%ls -> %ls[]", fun->name, fun->returnType.customTypeName);
                break;
            case IR_DT_CUSTOM_REFER:
                log(L"类型推导：%ls -> %ls&", fun->name, fun->returnType.customTypeName);
                break;
        }
#endif
    }
#ifdef HX_DEBUG
    log(L"进行函数返回类型推导->完成\n");
#endif
    return 0;
}
#endif