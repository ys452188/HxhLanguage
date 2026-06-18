#ifndef HXHLANG_SRC_HXC_IR_H
#define HXHLANG_SRC_HXC_IR_H
#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>
#include <wchar.h>

#include "Error.h"
#include "Lexer.h"
#include "SymbolTable.h"
class FunCallPitch;
#include "Checker.h"
#ifdef HX_DEBUG
void showFunctionInfo(IR_Function* function);
#endif
/**
 * 解析函数定义
 * @param tokens 词法分析得到的Token流
 * @param index 当前解析到的Token索引，解析完成后会更新为下一个未解析的Token索引
 * @param err 错误码指针，发生错误时会设置为相应错误码
 */
IR_Function* parseFunction(Tokens* tokens, int* index, int* err);
/**
 * 解析类定义
 * @param tokens 词法分析得到的Token流
 * @param index 当前解析到的Token索引，解析完成后会更新为下一个未解析的Token索引
 * @param err 错误码指针，发生错误时会设置为相应错误码
 */
IR_Class* parseClass(Tokens* tokens, int* index, int* err);
/** 生成中间表示
 * @param tokens 词法分析得到的Token流
 * @param err 错误码指针，发生错误时会设置为相应错误码
 */
IR_Program* generateIR(Tokens* tokens, int* err);  // 生成中间表示
/** 解析全局或类中的变量定义(不能赋初值)
 * @param tokens 词法分析得到的Token流
 * @param index 当前解析到的Token索引，解析完成后会更新为下一个未解析的Token索引
 * @param err 错误码指针，发生错误时会设置为相应错误码
 */
IR_Variable* parseGlobalOrClassVariable(Tokens* tokens, int* index, int* err);
/*释放IR*/
void freeIRProgram(IR_Program** program);
bool checkEachClass(IR_Class* cls, IR_Program* currentIRProgram, std::vector<IR_Class*> lastClasses);
static int getClassSize(IR_Class* cls, IR_Program*);
//----------------------------------------------------------------------------------------------
IR_Program* generateIR(Tokens* tokens, int* err) {
    if (!tokens) {
        if (err) *err = -1;
        return NULL;
    }
    if (tokens->count == 0) {
        *err = 225;
        setError(ERR_NO_MAIN, 0, NULL);
        return NULL;
    }
    IR_Program* program = (IR_Program*)calloc(1, sizeof(IR_Program));
    if (!program) {
        if (err) *err = -1;
        return NULL;
    }
    int index = 0;
    while (index < tokens->count) {
        int old_index = index; /* 防护：记录进入循环时的索引，防止解析器未推进导致死循环
                                */
        // 解析全局变量、函数或类定义
        if (tokens->tokens[index].type == TOK_KW) {
            if (wcscmp(tokens->tokens[index].value, L"函数") == 0 || wcscmp(tokens->tokens[index].value, L"fun") == 0) {
#ifdef HX_DEBUG
                log(L"解析到一个函数定义");
#endif
                IR_Function* func = parseFunction(tokens, &index, err);
                if (!func) {
                    // 解析函数出错
                    free(program);
                    return NULL;
                }
                int index = isFunctionRepeatDefine(func, program->functions);
                if (index == -255) {
                    // 函数重复定义错误已处理
                    freeIRProgram(&program);
                    *err = 255;
                    return NULL;
                } else if (index == -1) {
                    // 函数未声明，继续添加
                    program->functions.push_back(func);
                } else {
                    // 函数已声明，进行定义
                    if (program->functions[index]->name) free(program->functions[index]->name);
                    if (program->functions[index]->params) {
                        for (int i = 0; i < program->functions[index]->paramCount; i++) {
                            if (program->functions[index]->params[i].name) free(program->functions[index]->params[i].name);
                        }
                        free(program->functions[index]->params);
                    }
                    if (program->functions[index]->returnType.customTypeName)
                        free(program->functions[index]->returnType.customTypeName);
                    free(program->functions[index]);
                    program->functions[index] = func;
                    continue;
                }
            } else if (wcscmp(tokens->tokens[index].value, L"定义类") == 0 ||
                       wcscmp(tokens->tokens[index].value, L"cls") == 0) {
#ifdef HX_DEBUG
                log(L"解析到一个类定义");
#endif
                IR_Class* cls = parseClass(tokens, &index, err);
                if (!cls) {
                    // 解析类出错
                    freeIRProgram(&program);
                    return NULL;
                }
                if (isClassRepeatDefine(cls, program->classes, program->class_count) == 255) {
                    // 类重复定义错误已处理
                    freeIRProgram(&program);
                    *err = 255;
                    return NULL;
                }
                program->class_count++;
                program->classes = (IR_Class**)realloc(program->classes, program->class_count * sizeof(IR_Class*));
                if (!program->classes) {
                    if (err) *err = -1;
                    free(cls);
                    free(program);
                    return NULL;
                }
                program->classes[program->class_count - 1] = cls;
            } else if (wcscmp(tokens->tokens[index].value, L"var") == 0 ||
                       wcscmp(tokens->tokens[index].value, L"定义变量") == 0 ||
                       wcscmp(tokens->tokens[index].value, L"con") == 0) {
#ifdef HX_DEBUG
                log(L"解析到一个全局变量定义");
#endif
                IR_Variable* var = parseGlobalOrClassVariable(tokens, &index, err);
                if (!var || *err) {
                    free(program);
                    return NULL;
                }
                program->global_variable_count++;
                program->global_variables =
                    (IR_Variable**)realloc(program->global_variables, program->global_variable_count * sizeof(IR_Variable*));
                if (!program->global_variables) {
                    if (err) *err = -1;
                    free(var);
                    free(program);
                    return NULL;
                }
                program->global_variables[program->global_variable_count - 1] = var;
#ifdef HX_DEBUG
                log(L"已解析全局变量(\"%ls\")", var->name);
                log(L"是否只读? ：%d", var->isOnlyRead);
#endif
            } else {
                // 未知的全局定义
                setError(ERR_GLOBAL_UNKOWN, tokens->tokens[index].line, tokens->tokens[index].value);
                if (err) *err = 255;
                free(program);
                return NULL;
            }
        } else {
            // 未知的全局定义
            setError(ERR_GLOBAL_UNKOWN, tokens->tokens[index].line, tokens->tokens[index].value);
            if (err) *err = 255;
            free(program);
            return NULL;
        }
        /* 如果处理完一个单元后索引没有前进，说明解析器遇到无法前进的状态，避免死循环
         */
        if (index == old_index) {
#ifdef HX_DEBUG
            fwprintf(stderr,
                     L"[DBG] index=%d type=%d line=%d "
                     L"valuePtr=%p\n",
                     index, tokens->tokens[index].type, tokens->tokens[index].line, (void*)tokens->tokens[index].value);
#endif
            setError(ERR_FUN, tokens->tokens[index].line, tokens->tokens[index].value ? tokens->tokens[index].value : L" ");
            if (err) *err = 255;
            free(program);
            return NULL;
        }
    }
    // 设置类大小、检查是否包含
    for (int i = 0; i < program->class_count; i++) {
        std::vector<IR_Class*> checkedClasses;
        if (checkEachClass(program->classes[i], program, checkedClasses)) {
            setError(ERROR_UNCOMPLETED_CLASS, program->classes[i]->line, NULL);
            *err = 255;
            free(program);
            return NULL;
        }
        program->classes[i]->size = getClassSize(program->classes[i], program);
    }
    *err = deduceFunctionReturnTypes(program);
    if (*err) {
        free(program);
        return NULL;
    }
    return program;
}
void freeIRProgram(IR_Program** program) {
    if (!program || !*program) return;
    if (*program) {
        // 释放程序中的所有函数
        if ((*program)->functions.size() != 0) {
            for (int i = 0; i < (*program)->functions.size(); i++) {
                IR_Function* func = (*program)->functions[i];
                if (func) {
                    if (func->name) {
                        free(func->name);
                        func->name = NULL;
                    }
                    if (func->params) {
                        for (int j = 0; j < func->paramCount; j++) {
                            if (func->params[j].name) {
                                free(func->params[j].name);
                                func->params[j].name = NULL;
                            }
                        }
                        free(func->params);
                        func->params = NULL;
                    }
                    if (func->bodyTokens) {
                        free(func->bodyTokens);
                        func->bodyTokens = NULL;
                    }
                    free(func);
                    func = NULL;
                }
            }
        }
        // 释放程序中的所有类
        if ((*program)->classes) {
            for (int i = 0; i < (*program)->class_count; i++) {
                IR_Class* cls = (*program)->classes[i];
                if (cls) {
                    if (cls->name) {
                        free(cls->name);
                        cls->name = NULL;
                    }
                    if (cls->parent_name) {
                        free(cls->parent_name);
                        cls->parent_name = NULL;
                    }
                    // 释放类体成员
                    // 释放公有成员
                    if (cls->body.publicMembers) {
                        for (int j = 0; j < cls->body.public_member_count; j++) {
                            IR_ClassMember member = cls->body.publicMembers[j];
                            if (member.type == IR_CM_VARIABLE) {
                                if (member.data.variable) {
                                    if (member.data.variable->name) {
                                        free(member.data.variable->name);
                                        member.data.variable->name = NULL;
                                    }
                                    free(member.data.variable);
                                    member.data.variable = NULL;
                                }
                            } else if (member.type == IR_CM_FUNCTION) {
                                IR_Function* func = member.data.function;
                                if (func) {
                                    if (func->name) {
                                        free(func->name);
                                        func->name = NULL;
                                    }
                                    if (func->params) {
                                        for (int k = 0; k < func->paramCount; k++) {
                                            if (func->params[k].name) {
                                                free(func->params[k].name);
                                                func->params[k].name = NULL;
                                            }
                                        }
                                        free(func->params);
                                        func->params = NULL;
                                    }
                                    if (func->bodyTokens) {
                                        free(func->bodyTokens);
                                        func->bodyTokens = NULL;
                                    }
                                    free(func);
                                    func = NULL;
                                }
                            }
                        }
                        free(cls->body.publicMembers);
                        cls->body.publicMembers = NULL;
                    }
                    // 释放私有成员
                    if (cls->body.privateMembers) {
                        for (int j = 0; j < cls->body.private_member_count; j++) {
                            IR_ClassMember member = cls->body.privateMembers[j];
                            if (member.type == IR_CM_VARIABLE) {
                                if (member.data.variable) {
                                    if (member.data.variable->name) {
                                        free(member.data.variable->name);
                                        member.data.variable->name = NULL;
                                    }
                                    free(member.data.variable);
                                    member.data.variable = NULL;
                                }
                            } else if (member.type == IR_CM_FUNCTION) {
                                IR_Function* func = member.data.function;
                                if (func) {
                                    if (func->name) {
                                        free(func->name);
                                        func->name = NULL;
                                    }
                                    if (func->params) {
                                        for (int k = 0; k < func->paramCount; k++) {
                                            if (func->params[k].name) {
                                                free(func->params[k].name);
                                                func->params[k].name = NULL;
                                            }
                                        }
                                        free(func->params);
                                        func->params = NULL;
                                    }
                                    if (func->bodyTokens) {
                                        free(func->bodyTokens);
                                        func->bodyTokens = NULL;
                                    }
                                    free(func);
                                    func = NULL;
                                }
                            }
                        }
                        free(cls->body.privateMembers);
                        cls->body.privateMembers = NULL;
                    }
                }
                if (cls->body.protectedMembers) {
                    for (int j = 0; j < cls->body.protected_member_count; j++) {
                        IR_ClassMember member = cls->body.protectedMembers[j];
                        if (member.type == IR_CM_VARIABLE) {
                            if (member.data.variable) {
                                if (member.data.variable->name) {
                                    free(member.data.variable->name);
                                    member.data.variable->name = NULL;
                                }
                                free(member.data.variable);
                                member.data.variable = NULL;
                            }
                        } else if (member.type == IR_CM_FUNCTION) {
                            IR_Function* func = member.data.function;
                            if (func) {
                                if (func->name) {
                                    free(func->name);
                                    func->name = NULL;
                                }
                                if (func->params) {
                                    for (int k = 0; k < func->paramCount; k++) {
                                        if (func->params[k].name) {
                                            free(func->params[k].name);
                                            func->params[k].name = NULL;
                                        }
                                    }
                                    free(func->params);
                                    func->params = NULL;
                                }
                                if (func->bodyTokens) {
                                    free(func->bodyTokens);
                                    func->bodyTokens = NULL;
                                }
                                free(func);
                                func = NULL;
                            }
                        }
                    }
                    free(cls->body.protectedMembers);
                    cls->body.protectedMembers = NULL;
                }
            }
        }
        // 释放程序中的所有全局变量
        if ((*program)->global_variables) {
            for (int i = 0; i < (*program)->global_variable_count; i++) {
                IR_Variable* var = (*program)->global_variables[i];
                if (var) {
                    if (var->name) {
                        free(var->name);
                        var->name = NULL;
                    }
                    free(var);
                    var = NULL;
                }
            }
            free((*program)->global_variables);
            (*program)->global_variables = NULL;
        }
        free(*program);
    }
    program = NULL;
}
// 根据字符串解析数据类型
static IR_DataType parseDataTypeByString(wchar_t* typeStr) noexcept {
    IR_DataType dt;
    if (wcscmp(typeStr, L"int") == 0) {
        dt.kind = IR_DT_INT;
    } else if (wcscmp(typeStr, L"float") == 0) {
        dt.kind = IR_DT_FLOAT;
    } else if (wcscmp(typeStr, L"string") == 0) {
        dt.kind = IR_DT_STRING;
    } else if (wcscmp(typeStr, L"char") == 0) {
        dt.kind = IR_DT_CHAR;
    } else if (wcscmp(typeStr, L"bool") == 0) {
        dt.kind = IR_DT_BOOL;
    } else if (wcscmp(typeStr, L"无返回类型") == 0) {
        dt.kind = IR_DT_VOID;
    } else if (wcscmp(typeStr, L"void") == 0) {
        dt.kind = IR_DT_VOID;
    } else if (wcscmp(typeStr, L"无参数") == 0) {
        dt.kind = IR_DT_VOID;
    } else if (wcscmp(typeStr, L"整型") == 0) {
        dt.kind = IR_DT_INT;
    } else if (wcscmp(typeStr, L"浮点型") == 0) {
        dt.kind = IR_DT_FLOAT;
    } else if (wcscmp(typeStr, L"字符串") == 0) {
        dt.kind = IR_DT_STRING;
    } else if (wcscmp(typeStr, L"字符型") == 0) {
        dt.kind = IR_DT_CHAR;
    } else if (wcscmp(typeStr, L"布尔型") == 0) {
        dt.kind = IR_DT_BOOL;
    } else {
        dt.kind = IR_DT_CUSTOM;
        dt.customTypeName = (wchar_t*)calloc(wcslen(typeStr) + 1, sizeof(wchar_t));
        if (dt.customTypeName) {
            wcscpy(dt.customTypeName, typeStr);
        }
    }
    return dt;
}
IR_DataType parseDataType(Tokens* tokens, int* index, int size, int* err) {
    IR_DataType dt = {};
    if (!tokens || !index || size < 1) {
        *err = -1;
        return dt;
    }
    if (tokens->tokens[*index].type != TOK_ID) {
        return dt;
    }
    dt = parseDataTypeByString(tokens->tokens[*index].value);
    if ((*index + 1) < tokens->count) {
        (*index)++;
        // 若为 []则为数组类型
        if (tokens->tokens[*index].type == TOK_OPR_LBRACKET) {
            if (dt.kind == IR_DT_INT) {
                dt.kind = IR_DT_INT_ARR;
            } else if (dt.kind == IR_DT_FLOAT) {
                dt.kind = IR_DT_FLOAT_ARR;
            } else if (dt.kind == IR_DT_STRING) {
                dt.kind = IR_DT_STRING_ARR;
            } else if (dt.kind == IR_DT_CHAR) {
                dt.kind = IR_DT_CHAR_ARR;
            } else if (dt.kind == IR_DT_BOOL) {
                dt.kind = IR_DT_BOOL_ARR;
            } else if (dt.kind == IR_DT_CUSTOM) {
                dt.kind = IR_DT_CUSTOM_ARR;
            }
            (*index)++;
            if ((*index + 1) < tokens->count && tokens->tokens[*index].type == TOK_OPR_RBRACKET) {
                //(*index)++;
            } else {
                *err = 255;  // 语法错误
                setError(ERR_TYPE, tokens->tokens[*index].line, NULL);
                return dt;
            }
        }
        // 若为 &则为引用类型
        else if (tokens->tokens[*index].type == TOK_OPR_REFER) {
            if (dt.kind == IR_DT_INT) {
                dt.kind = IR_DT_INT_REFER;
            } else if (dt.kind == IR_DT_FLOAT) {
                dt.kind = IR_DT_FLOAT_REFER;
            } else if (dt.kind == IR_DT_STRING) {
                dt.kind = IR_DT_STRING_REFER;
            } else if (dt.kind == IR_DT_CHAR) {
                dt.kind = IR_DT_CHAR_REFER;
            } else if (dt.kind == IR_DT_BOOL) {
                dt.kind = IR_DT_BOOL_REFER;
            } else if (dt.kind == IR_DT_CUSTOM) {
                dt.kind = IR_DT_CUSTOM_REFER;
            }
            //(*index)++;
        } else {
            // 不是数组或引用类型，回退索引
            (*index)--;
        }
    }
    return dt;
}
// 定义变量::= 变量:标识符[,它的类型是:数据类型];
static IR_Variable* parseGlobalOrClassVariable_CN(Tokens* tokens, int* index, int* err, bool isOnlyRead) {
    if (!tokens || !index || !err) {
        if (err) *err = -1;
        return NULL;
    }
    if ((*index + 1) >= tokens->count) {
        *err = 255;  // 语法错误
        setError(ERR_DEF_VAR, tokens->tokens[*index].line, NULL);
        return NULL;
    }
    (*index)++;
    if (tokens->tokens[*index].type != TOK_OPR_COLON) {
        *err = 255;  // 语法错误
        setError(ERR_DEF_VAR, tokens->tokens[*index].line, NULL);
        return NULL;
    }
    if ((*index + 1) >= tokens->count) {
        *err = 255;  // 语法错误
        setError(ERR_DEF_VAR, tokens->tokens[*index].line, NULL);
        return NULL;
    }
    (*index)++;
    if (tokens->tokens[*index].type != TOK_ID) {
        *err = 255;  // 语法错误
        setError(ERR_DEF_VAR, tokens->tokens[*index].line, NULL);
        return NULL;
    }
    IR_Variable* var = (IR_Variable*)calloc(1, sizeof(IR_Variable));
    if (!var) {
        *err = -1;
        return NULL;
    }
    var->isTypeKnown = false;
    var->isOnlyRead = isOnlyRead;
    var->name = (wchar_t*)calloc(wcslen(tokens->tokens[*index].value) + 1, sizeof(wchar_t));
    if (!var->name) {
        *err = -1;
        free(var);
        return NULL;
    }
    wcscpy(var->name, tokens->tokens[*index].value);
#ifdef HX_DEBUG
    log(L"已解析变量名(\"%ls\")", var->name);
#endif
    // 类型 ,它的类型是:
    if ((*index + 1) >= tokens->count) {
        *err = 255;  // 语法错误
        setError(ERR_DEF_VAR, tokens->tokens[*index].line, NULL);
        free(var->name);
        free(var);
        return NULL;
    }
    (*index)++;
    if (tokens->tokens[*index].type == TOK_OPR_COMMA) {
        if ((*index + 1) >= tokens->count) {
            *err = 255;  // 语法错误
            setError(ERR_DEF_VAR, tokens->tokens[*index].line, NULL);
            free(var->name);
            free(var);
            return NULL;
        }
        (*index)++;
        // 它的类型是
        if (tokens->tokens[*index].type != TOK_KW) {
            *err = 255;  // 语法错误
            setError(ERR_DEF_VAR, tokens->tokens[*index].line, NULL);
            free(var->name);
            free(var);
            return NULL;
        }
        if (wcscmp(tokens->tokens[*index].value, L"类型是") != 0) {
            *err = 255;  // 语法错误
            setError(ERR_DEF_VAR, tokens->tokens[*index].line, NULL);
            free(var->name);
            free(var);
            return NULL;
        }
        if ((*index + 1) >= tokens->count) {
            *err = 255;  // 语法错误
            setError(ERR_DEF_VAR, tokens->tokens[*index].line, NULL);
            free(var->name);
            free(var);
            return NULL;
        }
        (*index)++;
        // :
        if (tokens->tokens[*index].type != TOK_OPR_COLON) {
            *err = 255;  // 语法错误
            setError(ERR_DEF_VAR, tokens->tokens[*index].line, NULL);
            free(var->name);
            free(var);
            return NULL;
        }
        if ((*index + 1) >= tokens->count) {
            *err = 255;  // 语法错误
            setError(ERR_DEF_VAR, tokens->tokens[*index].line, NULL);
            free(var->name);
            free(var);
            return NULL;
        }
        (*index)++;
        // 数据类型
        if (tokens->tokens[*index].type != TOK_ID && tokens->tokens[*index].type != TOK_KW) {
            *err = 255;  // 语法错误
            setError(ERR_DEF_VAR, tokens->tokens[*index].line, NULL);
            free(var->name);
            free(var);
            return NULL;
        }
        var->isTypeKnown = true;
        var->type = parseDataType(tokens, index, tokens->count, err);
        if (*err != 0) {
            free(var->name);
            free(var);
            return NULL;
        }
        if (var->type.kind == IR_DT_CUSTOM) {
            var->type.customTypeName = (wchar_t*)calloc(wcslen(tokens->tokens[*index].value) + 1, sizeof(wchar_t));
            if (!var->type.customTypeName) {
                *err = -1;
                free(var->name);
                free(var);
                return NULL;
            }
            wcscpy(var->type.customTypeName, tokens->tokens[*index].value);
        }
        if ((*index + 1) >= tokens->count) {
            *err = 255;  // 语法错误
            setError(ERR_DEF_VAR, tokens->tokens[*index].line, NULL);
            if (var->type.kind == IR_DT_CUSTOM) {
                if (var->type.customTypeName) free(var->type.customTypeName);
            }
            free(var->name);
            free(var);
            return NULL;
        }
        (*index)++;
    }
    // end
    if (tokens->tokens[*index].type != TOK_END) {
        *err = 255;
        setError(ERR_DEF_VAR, tokens->tokens[*index].line, NULL);
        if (var->type.kind == IR_DT_CUSTOM) {
            if (var->type.customTypeName) free(var->type.customTypeName);
        }
        free(var->name);
        free(var);
        return NULL;
    }
    (*index)++;
    return var;
}
// 定义变量::= var:标识符[:数据类型];
static IR_Variable* parseGlobalOrClassVariable_EN(Tokens* tokens, int* index, int* err, bool isOnlyRead) {
    if (!tokens || !index || !err) {
        if (err) *err = -1;
        return NULL;
    }
    if (*index + 1 >= tokens->count) {
        *err = 255;  // 语法错误
        setError(ERR_DEF_VAR, tokens->tokens[*index].line, NULL);
        return NULL;
    }
    (*index)++;
    if (tokens->tokens[*index].type != TOK_OPR_COLON) {
        *err = 255;  // 语法错误
        setError(ERR_DEF_VAR, tokens->tokens[*index].line, NULL);
        return NULL;
    }
    if (*index + 1 >= tokens->count) {
        *err = 255;  // 语法错误
        setError(ERR_DEF_VAR, tokens->tokens[*index].line, NULL);
        return NULL;
    }
    (*index)++;
    if (tokens->tokens[*index].type != TOK_ID) {
        *err = 255;  // 语法错误
        setError(ERR_DEF_VAR, tokens->tokens[*index].line, NULL);
        return NULL;
    }
    IR_Variable* var = (IR_Variable*)calloc(1, sizeof(IR_Variable));
    var->isOnlyRead = isOnlyRead;
    if (!var) {
        *err = -1;
        return NULL;
    }
    var->name = (wchar_t*)calloc(wcslen(tokens->tokens[*index].value) + 1, sizeof(wchar_t));
    if (!var->name) {
        *err = -1;
        free(var);
        return NULL;
    }
    wcscpy(var->name, tokens->tokens[*index].value);
#ifdef HX_DEBUG
    log(L"已解析变量名(\"%ls\")", var->name);
#endif
    // 类型
    if (*index + 1 >= tokens->count) {
        *err = 255;  // 语法错误
        setError(ERR_DEF_VAR, tokens->tokens[*index].line, NULL);
        free(var->name);
        free(var);
        return NULL;
    }
    (*index)++;
    var->isTypeKnown = false;
    if (tokens->tokens[*index].type == TOK_OPR_COLON) {
        var->isTypeKnown = true;
        if (*index + 1 >= tokens->count) {
            *err = 255;  // 语法错误
            setError(ERR_DEF_VAR, tokens->tokens[*index].line, NULL);
            free(var->name);
            free(var);
            return NULL;
        }
        (*index)++;
        if (tokens->tokens[*index].type != TOK_ID && tokens->tokens[*index].type != TOK_KW) {
            *err = 255;  // 语法错误
            setError(ERR_DEF_VAR, tokens->tokens[*index].line, NULL);
            free(var->name);
            free(var);
            return NULL;
        }
        var->type = parseDataType(tokens, index, tokens->count, err);
        if (*err != 0) {
            free(var->name);
            free(var);
            return NULL;
        }
        if (var->type.kind == IR_DT_CUSTOM) {
            var->type.customTypeName = (wchar_t*)calloc(wcslen(tokens->tokens[*index].value) + 1, sizeof(wchar_t));
            if (!var->type.customTypeName) {
                *err = -1;
                free(var->name);
                free(var);
                return NULL;
            }
            wcscpy(var->type.customTypeName, tokens->tokens[*index].value);
        }
        if ((*index + 1) >= tokens->count) {
            *err = 255;
            setError(ERR_DEF_VAR, tokens->tokens[*index].line, NULL);
            if (var->type.kind == IR_DT_CUSTOM) {
                if (var->type.customTypeName) free(var->type.customTypeName);
            }
            free(var->name);
            free(var);
            return NULL;
        }
        (*index)++;
    }
    // end
    if (tokens->tokens[*index].type != TOK_END) {
        *err = 255;
        setError(ERR_DEF_VAR, tokens->tokens[*index].line, NULL);
        if (var->type.kind == IR_DT_CUSTOM) {
            if (var->type.customTypeName) free(var->type.customTypeName);
        }
        free(var->name);
        free(var);
        return NULL;
    }
    (*index)++;
    return var;
}
IR_Variable* parseGlobalOrClassVariable(Tokens* tokens, int* index, int* err) {
    if (!tokens || !index || !err) {
        if (err) *err = -1;
        return NULL;
    }
    if (wcscmp(tokens->tokens[*index].value, L"var") == 0) {
        return parseGlobalOrClassVariable_EN(tokens, index, err, false);
    } else if (wcscmp(tokens->tokens[*index].value, L"定义变量") == 0) {
        return parseGlobalOrClassVariable_CN(tokens, index, err, false);
    } else if (wcscmp(tokens->tokens[*index].value, L"con") == 0) {
        return parseGlobalOrClassVariable_EN(tokens, index, err, true);
    } else if (wcscmp(tokens->tokens[*index].value, L"定义常量") == 0) {
        return parseGlobalOrClassVariable_CN(tokens, index, err, true);
    }
    return NULL;
}
// 解析函数参数列表
// 参数::= 标识符：数据类型
static IR_FunctionParam* parseFunctionParams(Tokens* tokens, int* index, int* paramCount, int* err) {
    *paramCount = 0;
    IR_FunctionParam* params = NULL;
    if (tokens->tokens[*index].type == TOK_OPR_RQUOTE) {
        return NULL;
    }
    while (*index < tokens->count && tokens->tokens[*index].type != TOK_OPR_RQUOTE) {
        if (wcscmp(tokens->tokens[*index].value, L"无参数") == 0 || wcscmp(tokens->tokens[*index].value, L"void") == 0) {
            (*index)++;
            break;
        }
        if (tokens->tokens[*index].type == TOK_OPR_RQUOTE) {
            break;
        }
        if (tokens->tokens[*index].type != TOK_ID) {
#ifdef HX_DEBUG
            fwprintf(logStream, L"[DBG]分析参数: index=%d type=%d line=%d\n", *index, tokens->tokens[*index].type,
                     tokens->tokens[*index].line);
            int start = *index - 3;
            if (start < 0) start = 0;
            int end = *index + 3;
            if (end >= tokens->count) end = tokens->count - 1;
            for (int k = start; k <= end; k++) {
                fwprintf(logStream, L"\tidx=%d type=%d line=%d value=\"%ls\"\n", k, tokens->tokens[k].type,
                         tokens->tokens[k].line, tokens->tokens[k].value ? tokens->tokens[k].value : L"(null)");
            }
#endif
            *err = 255;  // 语法错误
            setError(ERR_FUN_ARG, tokens->tokens[*index].line, NULL);
            return NULL;
        }
        (*paramCount)++;
        params = (IR_FunctionParam*)realloc(params, (*paramCount) * sizeof(IR_FunctionParam));
        if (!params) {
            *err = -1;
            return NULL;
        }
        params[(*paramCount) - 1].name = (wchar_t*)calloc(wcslen(tokens->tokens[*index].value) + 1, sizeof(wchar_t));
        if (!params[(*paramCount) - 1].name) {
            *err = -1;
            free(params);
            return NULL;
        }
        wcscpy(params[(*paramCount) - 1].name, tokens->tokens[*index].value);
#ifdef HX_DEBUG
        log(L"解析到参数名：%ls", params[(*paramCount) - 1].name);
#endif
        if ((*index + 1) >= tokens->count) {
            *err = 255;  // 语法错误
            setError(ERR_FUN_ARG, tokens->tokens[*index].line, NULL);
            // 释放已分配的内存
            for (int i = 0; i < (*paramCount); i++) {
                free(params[i].name);
            }
            free(params);
            return NULL;
        }
        (*index)++;
        // 解析冒号
        if (tokens->tokens[*index].type != TOK_OPR_COLON) {
#ifdef HX_DEBUG
            log(L"解析冒号发现错误");
#endif
            *err = 255;  // 语法错误
            setError(ERR_FUN_ARG, tokens->tokens[*index].line, NULL);
            // 释放已分配的内存
            for (int i = 0; i < (*paramCount); i++) {
                free(params[i].name);
            }
            free(params);
            return NULL;
        }
        if ((*index + 1) >= tokens->count) {
            *err = 255;  // 语法错误
            setError(ERR_FUN_ARG, tokens->tokens[*index].line, NULL);
            // 释放已分配的内存
            for (int i = 0; i < (*paramCount); i++) {
                free(params[i].name);
            }
            free(params);
            return NULL;
        }
        (*index)++;
        // 解析数据类型
        if (tokens->tokens[*index].type != TOK_ID && tokens->tokens[*index].type != TOK_KW) {
#ifdef HX_DEBUG
            log(L"解析到数据类型发现错误");
#endif
            *err = 255;  // 语法错误
            setError(ERR_FUN_ARG, tokens->tokens[*index].line, NULL);
            // 释放已分配的内存
            for (int i = 0; i < (*paramCount); i++) {
                free(params[i].name);
            }
            free(params);
            return NULL;
        }
        params[(*paramCount) - 1].type = parseDataType(tokens, index, tokens->count, err);
        if (*err != 0) {
#ifdef HX_DEBUG
            log(L"数据类型转为IR_DataType发现错误");
#endif
            // 释放已分配的内存
            for (int i = 0; i < (*paramCount); i++) {
                free(params[i].name);
            }
            free(params);
            return NULL;
        }

        // 下一参数或结束
        if ((*index + 1) >= tokens->count) {
            *err = 255;  // 语法错误
            setError(ERR_FUN, tokens->tokens[*index].line, NULL);
            // 释放已分配的内存
            for (int i = 0; i < (*paramCount); i++) {
                free(params[i].name);
            }
            free(params);
            return NULL;
        }
        (*index)++;
        if (tokens->tokens[*index].type == TOK_OPR_COMMA) {
            (*index)++;
        } else if (tokens->tokens[*index].type == TOK_OPR_RQUOTE) {
            break;
        } else {
            *err = 255;  // 语法错误
            setError(ERR_FUN_ARG, tokens->tokens[*index].line, NULL);
            // 释放已分配的内存
            for (int i = 0; i < (*paramCount); i++) {
                free(params[i].name);
            }
            free(params);
            return NULL;
        }
    }
    return params;
}
// 函数定义::= 函数:标识符(参数列表)[,返回类型:数据类型] -> {函数体}
static IR_Function* parseFunction_CN(Tokens* tokens, int* index, int* err) {
    if ((*index + 1) >= tokens->count) {
        *err = 255;  // 语法错误
        setError(ERR_FUN, tokens->tokens[*index].line, NULL);
        return NULL;
    }
    IR_Function* function = (IR_Function*)calloc(1, sizeof(IR_Function));
    if (!function) {
        *err = -1;
        return NULL;
    }
    (*index)++;

    // 解析函数名
    //:|：
    if (tokens->tokens[*index].type != TOK_OPR_COLON) {
        setError(ERR_FUN, tokens->tokens[*index].line, NULL);
        *err = 255;  // 语法错误
        free(function);
        return NULL;
    }
    if ((*index + 1) >= tokens->count) {
        setError(ERR_FUN, tokens->tokens[*index].line, NULL);
        *err = 255;  // 语法错误
        free(function);
        return NULL;
    }
    (*index)++;
    // ID
    if (tokens->tokens[*index].type != TOK_ID) {
        setError(ERR_FUN, tokens->tokens[*index].line, NULL);
        *err = 255;  // 语法错误
        free(function);
        return NULL;
    }
    function->name = (wchar_t*)calloc(wcslen(tokens->tokens[*index].value) + 1, sizeof(wchar_t));
    if (!function->name) {
        *err = -1;
        free(function);
        return NULL;
    }
    wcscpy(function->name, tokens->tokens[*index].value);
#ifdef HX_DEBUG
    log(L"解析到函数名：%ls", function->name);
#endif

    if ((*index + 1) >= tokens->count) {
        setError(ERR_FUN, tokens->tokens[*index].line, NULL);
        *err = 255;  // 语法错误
        free(function->name);
        free(function);
        return NULL;
    }
    (*index)++;
    // (
    if (tokens->tokens[*index].type != TOK_OPR_LQUOTE) {
        setError(ERR_FUN, tokens->tokens[*index].line, NULL);
        *err = 255;  // 语法错误
        free(function->name);
        free(function);
        return NULL;
    }
    if ((*index + 1) >= tokens->count) {
        setError(ERR_FUN, tokens->tokens[*index].line, NULL);
        *err = 255;  // 语法错误
        free(function->name);
        free(function);
        return NULL;
    }
    (*index)++;
    // 解析参数列表
    function->params = parseFunctionParams(tokens, index, &(function->paramCount), err);
    if (*err != 0) {
        free(function->name);
        free(function);
        return NULL;
    }
    // 分析返回类型或复制函数体
    if ((*index + 1) >= tokens->count) {
        setError(ERR_FUN, tokens->tokens[*index].line, NULL);
        *err = 255;  // 语法错误
        // 释放已分配的内存
        for (int i = 0; i < function->paramCount; i++) {
            free(function->params[i].name);
        }
        free(function->params);
        free(function->name);
        free(function);
        return NULL;
    }
    (*index)++;
    function->isReturnTypeKnown = false;
    if (tokens->tokens[*index].type == TOK_OPR_COMMA) {
        // ,
        if ((*index + 1) >= tokens->count) {
            setError(ERR_FUN, tokens->tokens[*index].line, NULL);
            *err = 255;  // 语法错误
            // 释放已分配的内存
            for (int i = 0; i < function->paramCount; i++) {
                free(function->params[i].name);
            }
            free(function->params);
            free(function->name);
            free(function);
            return NULL;
        }
        (*index)++;
        // 返回类型是
        if (wcscmp(tokens->tokens[*index].value, L"返回类型是") == 0) {
            // 冒号
            if ((*index + 1) >= tokens->count) {
                setError(ERR_FUN, tokens->tokens[*index].line, NULL);
                *err = 255;  // 语法错误
                // 释放已分配的内存
                for (int i = 0; i < function->paramCount; i++) {
                    free(function->params[i].name);
                }
                free(function->params);
                free(function->name);
                free(function);
                return NULL;
            }
            (*index)++;
            if (tokens->tokens[*index].type != TOK_OPR_COLON) {
                setError(ERR_FUN, tokens->tokens[*index].line, NULL);
                *err = 255;  // 语法错误
                // 释放已分配的内存
                for (int i = 0; i < function->paramCount; i++) {
                    free(function->params[i].name);
                }
                free(function->params);
                free(function->name);
                free(function);
                return NULL;
            }
            // 数据类型
            if ((*index + 1) >= tokens->count) {
                setError(ERR_FUN, tokens->tokens[*index].line, NULL);
                *err = 255;  // 语法错误
                // 释放已分配的内存
                for (int i = 0; i < function->paramCount; i++) {
                    free(function->params[i].name);
                }
                free(function->params);
                free(function->name);
                free(function);
                return NULL;
            }
            (*index)++;
            if (tokens->tokens[*index].type != TOK_ID) {
                setError(ERR_FUN, tokens->tokens[*index].line, NULL);
                *err = 255;  // 语法错误
                // 释放已分配的内存
                for (int i = 0; i < function->paramCount; i++) {
                    free(function->params[i].name);
                }
                free(function->params);
                free(function->name);
                free(function);
                return NULL;
            }
#ifdef HX_DEBUG
            log(L"解析到函数返回类型：%ls", tokens->tokens[*index].value);
#endif
            function->returnType = parseDataType(tokens, index, tokens->count, err);
            if (*err) {
                // 释放已分配的内存
                for (int i = 0; i < function->paramCount; i++) {
                    free(function->params[i].name);
                }
                free(function->params);
                free(function->name);
                free(function);
                return NULL;
            }
            function->isReturnTypeKnown = true;
            if ((*index + 1) >= tokens->count) {
                setError(ERR_FUN, tokens->tokens[*index].line, NULL);
                *err = 255;  // 语法错误
                // 释放已分配的内存
                for (int i = 0; i < function->paramCount; i++) {
                    free(function->params[i].name);
                }
                free(function->params);
                free(function->name);
                free(function);
                return NULL;
            }
            (*index)++;
            // 它没有返回类型
        } else if (wcscmp(tokens->tokens[*index].value, L"无返回类型") == 0) {
            function->isReturnTypeKnown = true;
            function->returnType.kind = IR_DT_VOID;
            if ((*index + 1) >= tokens->count) {
                setError(ERR_FUN, tokens->tokens[*index].line, NULL);
                *err = 255;  // 语法错误
                // 释放已分配的内存
                for (int i = 0; i < function->paramCount; i++) {
                    free(function->params[i].name);
                }
                free(function->params);
                free(function->name);
                free(function);
                return NULL;
            }
            (*index)++;
        } else {
            setError(ERR_FUN, tokens->tokens[*index].line, NULL);
            *err = 255;  // 语法错误
            // 释放已分配的内存
            for (int i = 0; i < function->paramCount; i++) {
                free(function->params[i].name);
            }
            free(function->params);
            free(function->name);
            free(function);
            return NULL;
        }
    }
    // 函数体
    if (tokens->tokens[*index].type == TOK_END) {
        (*index)++;
        function->body_token_count = 0;
        function->bodyTokens = NULL;
        return function;
    }
    if (tokens->tokens[*index].type != TOK_OPR_POINT) {
        setError(ERR_FUN, tokens->tokens[*index].line, NULL);
        *err = 255;
        // 释放已分配的内存
        for (int i = 0; i < function->paramCount; i++) {
            free(function->params[i].name);
        }
        free(function->params);
        free(function->name);
        free(function);
        return NULL;
    }
    if ((*index + 1) >= tokens->count) {
        setError(ERR_FUN, tokens->tokens[*index].line, NULL);
        *err = 255;  // 语法错误
        // 释放已分配的内存
        for (int i = 0; i < function->paramCount; i++) {
            free(function->params[i].name);
        }
        free(function->params);
        free(function->name);
        free(function);
        return NULL;
    }
    (*index)++;
    if (tokens->tokens[*index].type != TOK_OPR_LBRACE) {
        setError(ERR_FUN, tokens->tokens[*index].line, NULL);
        *err = 255;
        // 释放已分配的内存
        for (int i = 0; i < function->paramCount; i++) {
            free(function->params[i].name);
        }
        free(function->params);
        free(function->name);
        free(function);
        return NULL;
    }
    // 复制函数体Token流
    int body_start_index = *index;
    int brace_count = 1;
    while (*index + 1 < tokens->count && brace_count > 0) {
        (*index)++;
        if (tokens->tokens[*index].type == TOK_OPR_LBRACE) {
            brace_count++;
        } else if (tokens->tokens[*index].type == TOK_OPR_RBRACE) {
            brace_count--;
        }

        if (brace_count == 0) {
            break;
        }
    }
    int end = *index;
    if (brace_count != 0) {
        setError(ERR_HUAKUOHAO_NOT_CLOSE, tokens->tokens[*index].line, tokens->tokens[body_start_index].value);
        *err = 255;  // 语法错误
        // 释放已分配的内存
        for (int i = 0; i < function->paramCount; i++) {
            free(function->params[i].name);
        }
        free(function->params);
        free(function->name);
        free(function);
        return NULL;
    }
    function->body_token_count = end - body_start_index + 1;
    function->bodyTokens = (Token*)calloc(function->body_token_count, sizeof(Token));
    if (!function->bodyTokens) {
        *err = -1;
        // 释放已分配的内存
        for (int i = 0; i < function->paramCount; i++) {
            free(function->params[i].name);
        }
        free(function->params);
        free(function->name);
        free(function);
        return NULL;
    }
    for (int i = 0; i < function->body_token_count; i++) {
        function->bodyTokens[i] = tokens->tokens[body_start_index + i];
    }
    (*index)++;
#ifdef HX_DEBUG
    showFunctionInfo(function);
#endif
    return function;
}
// 函数定义::= function:标识符(参数列表)[:数据类型]->{函数体}
static IR_Function* parseFunction_EN(Tokens* tokens, int* index, int* err) {
    if (!tokens || !index) {
        if (err) *err = -1;
        return NULL;
    }
    if ((*index + 1) >= tokens->count) {
        *err = 255;  // 语法错误
        setError(ERR_FUN, tokens->tokens[*index].line, NULL);
        return NULL;
    }
    // 解析函数名
    (*index)++;
    if (tokens->tokens[*index].type != TOK_OPR_COLON) {
        setError(ERR_FUN, tokens->tokens[*index].line, NULL);
        *err = 255;  // 语法错误
        return NULL;
    }
    if ((*index + 1) >= tokens->count) {
        setError(ERR_FUN, tokens->tokens[*index].line, NULL);
        *err = 255;  // 语法错误
        return NULL;
    }
    (*index)++;
    if (tokens->tokens[*index].type != TOK_ID) {
        setError(ERR_FUN, tokens->tokens[*index].line, NULL);
        *err = 255;  // 语法错误
        return NULL;
    }
    IR_Function* function = (IR_Function*)calloc(1, sizeof(IR_Function));
    if (!function) {
        *err = -1;
        return NULL;
    }
    function->name = (wchar_t*)calloc(wcslen(tokens->tokens[*index].value) + 1, sizeof(wchar_t));
    if (!function->name) {
        *err = -1;
        free(function);
        return NULL;
    }
    wcscpy(function->name, tokens->tokens[*index].value);
#ifdef HX_DEBUG
    log(L"解析到函数名：%ls", function->name);
#endif
    // 参数列表
    if ((*index + 1) >= tokens->count) {
        setError(ERR_FUN, tokens->tokens[*index].line, NULL);
        *err = 255;  // 语法错误
        free(function->name);
        free(function);
        return NULL;
    }
    (*index)++;
    if (tokens->tokens[*index].type != TOK_OPR_LQUOTE) {
        setError(ERR_FUN, tokens->tokens[*index].line, NULL);
        *err = 255;  // 语法错误
        free(function->name);
        free(function);
        return NULL;
    }
    if ((*index + 1) >= tokens->count) {
        setError(ERR_FUN, tokens->tokens[*index].line, NULL);
        *err = 255;  // 语法错误
        free(function->name);
        free(function);
        return NULL;
    }
    (*index)++;
    int paramCount = 0;
#ifdef HX_DEBUG
    log(L"解析参数列表.....");
#endif
    function->params = parseFunctionParams(tokens, index, &paramCount, err);
    if (*err != 0) {
        free(function->name);
        free(function);
        return NULL;
    }
    function->paramCount = paramCount;
#ifdef HX_DEBUG
    log(L"解析到函数体");
#endif
    // 返回类型或函数体
    if ((*index + 1) >= tokens->count) {
        setError(ERR_FUN, tokens->tokens[*index].line, NULL);
        *err = 255;  // 语法错误
        // 释放已分配的内存
        for (int i = 0; i < function->paramCount; i++) {
            free(function->params[i].name);
        }
        free(function->params);
        free(function->name);
        free(function);
        return NULL;
    }
    (*index)++;
    function->isReturnTypeKnown = false;
    if (tokens->tokens[*index].type == TOK_OPR_COLON) {
        // :
        if ((*index + 1) >= tokens->count) {
            setError(ERR_FUN, tokens->tokens[*index].line, NULL);
            *err = 255;  // 语法错误
            // 释放已分配的内存
            for (int i = 0; i < function->paramCount; i++) {
                free(function->params[i].name);
            }
            free(function->params);
            free(function->name);
            free(function);
            return NULL;
        }
        (*index)++;
        // 数据类型
        if (tokens->tokens[*index].type != TOK_ID) {
            setError(ERR_FUN, tokens->tokens[*index].line, NULL);
            *err = 255;  // 语法错误
            // 释放已分配的内存
            for (int i = 0; i < function->paramCount; i++) {
                free(function->params[i].name);
            }
            free(function->params);
            free(function->name);
            free(function);
            return NULL;
        }
#ifdef HX_DEBUG
        log(L"解析到函数返回类型：%ls", tokens->tokens[*index].value);
#endif
        function->returnType = parseDataType(tokens, index, tokens->count, err);
        if (*err) {
            // 释放已分配的内存
            for (int i = 0; i < function->paramCount; i++) {
                free(function->params[i].name);
            }
            free(function->params);
            free(function->name);
            free(function);
            return NULL;
        }
        function->isReturnTypeKnown = true;
        if ((*index + 1) >= tokens->count) {
            setError(ERR_FUN, tokens->tokens[*index].line, NULL);
            *err = 255;  // 语法错误
            // 释放已分配的内存
            for (int i = 0; i < function->paramCount; i++) {
                free(function->params[i].name);
            }
            free(function->params);
            free(function->name);
            free(function);
            return NULL;
        }
        (*index)++;
    }
    // 函数体 | 声明
    if (tokens->tokens[*index].type == TOK_END) {
        // 函数声明，无函数体
        (*index)++;
        function->body_token_count = 0;
        function->bodyTokens = NULL;
        return function;
    }
    // ->
    if (tokens->tokens[*index].type != TOK_OPR_POINT) {
        setError(ERR_FUN, tokens->tokens[*index].line, NULL);
        *err = 255;  // 语法错误
        // 释放已分配的内存
        for (int i = 0; i < function->paramCount; i++) {
            free(function->params[i].name);
        }
        free(function->params);
        free(function->name);
        free(function);
        return NULL;
    }
    if (*index + 1 >= tokens->count) {
        setError(ERR_FUN, tokens->tokens[*index].line, NULL);
        *err = 255;  // 语法错误
        // 释放已分配的内存
        for (int i = 0; i < function->paramCount; i++) {
            free(function->params[i].name);
        }
        free(function->params);
        free(function->name);
        free(function);
        return NULL;
    }
    (*index)++;
    if (tokens->tokens[*index].type != TOK_OPR_LBRACE) {
        setError(ERR_FUN, tokens->tokens[*index].line, NULL);
        *err = 255;  // 语法错误
        // 释放已分配的内存
        for (int i = 0; i < function->paramCount; i++) {
            free(function->params[i].name);
        }
        free(function->params);
        free(function->name);
        free(function);
        return NULL;
    }
    // 复制函数体Token流
    int body_start_index = *index;
    int brace_count = 1;
    while (*index + 1 < tokens->count && brace_count > 0) {
        (*index)++;
        if (tokens->tokens[*index].type == TOK_OPR_LBRACE) {
            brace_count++;
        } else if (tokens->tokens[*index].type == TOK_OPR_RBRACE) {
            brace_count--;
        }

        if (brace_count == 0) {
            break;
        }
    }
    int end = *index;
    if (brace_count != 0) {
        setError(ERR_HUAKUOHAO_NOT_CLOSE, tokens->tokens[*index].line, tokens->tokens[body_start_index].value);
        *err = 255;  // 语法错误
        // 释放已分配的内存
        for (int i = 0; i < function->paramCount; i++) {
            free(function->params[i].name);
        }
        free(function->params);
        free(function->name);
        free(function);
        return NULL;
    }
    function->body_token_count = end - body_start_index + 1;
    function->bodyTokens = (Token*)calloc(function->body_token_count, sizeof(Token));
    if (!function->bodyTokens) {
        *err = -1;
        // 释放已分配的内存
        for (int i = 0; i < function->paramCount; i++) {
            free(function->params[i].name);
        }
        free(function->params);
        free(function->name);
        free(function);
        return NULL;
    }
    for (int i = 0; i < function->body_token_count; i++) {
        function->bodyTokens[i] = tokens->tokens[body_start_index + i];
    }
    (*index)++;
#ifdef HX_DEBUG
    showFunctionInfo(function);
#endif
    return function;
}

// 定义函数::= fun:标识符(参数列表)->数据类型 { 函数体 }
// 定义函数::= 定义函数:标识符(参数列表)
// [，它的返回类型是：数据类型]|[，它没有返回类型] { 函数体 }
IR_Function* parseFunction(Tokens* tokens, int* index, int* err) {
    if (!tokens || !index) {
        if (err) *err = -1;
        return NULL;
    }
    IR_Function* function = NULL;
    if (wcscmp(tokens->tokens[*index].value, L"fun") == 0) {
        function = parseFunction_EN(tokens, index, err);
        if (*err != 0) {
            return NULL;
        }
    } else if (wcscmp(tokens->tokens[*index].value, L"函数") == 0) {
        function = parseFunction_CN(tokens, index, err);
        if (*err != 0) {
            return NULL;
        }
    } else {
        *err = 255;  // 语法错误
        return NULL;
    }

    return function;
}
/** 显示函数信息
 * @param function 要显示信息的函数
 */
#ifdef HX_DEBUG
void showFunctionInfo(IR_Function* function) {
    if (!function) return;
    initLocale();
    fwprintf(logStream, L"函数名: %ls\n", function->name);
    fwprintf(logStream, L"是否已知返回类型: %ls\n", function->isReturnTypeKnown ? L"是" : L"否");
    if (function->isReturnTypeKnown) {
        fwprintf(logStream, L"返回类型: ");
        switch (function->returnType.kind) {
        case IR_DT_INT:
            fwprintf(logStream, L"int\n");
            break;
        case IR_DT_FLOAT:
            fwprintf(logStream, L"float\n");
            break;
        case IR_DT_STRING:
            fwprintf(logStream, L"string\n");
            break;
        case IR_DT_CHAR:
            fwprintf(logStream, L"char\n");
            break;
        case IR_DT_BOOL:
            fwprintf(logStream, L"bool\n");
            break;
        case IR_DT_VOID:
            fwprintf(logStream, L"void\n");
            break;
        case IR_DT_CUSTOM:
            fwprintf(logStream, L"custom(%ls)\n", function->returnType.customTypeName);
            break;
        case IR_DT_INT_ARR:
            fwprintf(logStream, L"int array\n");
            break;
        case IR_DT_FLOAT_ARR:
            fwprintf(logStream, L"float array\n");
            break;
        case IR_DT_STRING_ARR:
            fwprintf(logStream, L"string array\n");
            break;
        case IR_DT_CHAR_ARR:
            fwprintf(logStream, L"char array\n");
            break;
        case IR_DT_BOOL_ARR:
            fwprintf(logStream, L"bool array\n");
            break;
        case IR_DT_INT_REFER:
            fwprintf(logStream, L"int reference\n");
            break;
        case IR_DT_FLOAT_REFER:
            fwprintf(logStream, L"float reference\n");
            break;
        case IR_DT_STRING_REFER:
            fwprintf(logStream, L"string reference\n");
            break;
        case IR_DT_CHAR_REFER:
            fwprintf(logStream, L"char reference\n");
            break;
        case IR_DT_BOOL_REFER:
            fwprintf(logStream, L"bool reference\n");
            break;
        }
    }
    fwprintf(logStream, L"参数个数: %d\n", function->paramCount);
    for (int i = 0; i < function->paramCount; i++) {
        fwprintf(logStream, L"\t参数%d: 名字=%ls, 类型=", i + 1, function->params[i].name);
        switch (function->params[i].type.kind) {
        case IR_DT_INT:
            fwprintf(logStream, L"int\n");
            break;
        case IR_DT_FLOAT:
            fwprintf(logStream, L"float\n");
            break;
        case IR_DT_STRING:
            fwprintf(logStream, L"string\n");
            break;
        case IR_DT_CHAR:
            fwprintf(logStream, L"char\n");
            break;
        case IR_DT_BOOL:
            fwprintf(logStream, L"bool\n");
            break;
        case IR_DT_VOID:
            fwprintf(logStream, L"void\n");
            break;
        case IR_DT_CUSTOM:
            fwprintf(logStream, L"custom(%ls)\n", function->returnType.customTypeName);
            break;
        case IR_DT_INT_ARR:
            fwprintf(logStream, L"int array\n");
            break;
        case IR_DT_FLOAT_ARR:
            fwprintf(logStream, L"float array\n");
            break;
        case IR_DT_STRING_ARR:
            fwprintf(logStream, L"string array\n");
            break;
        case IR_DT_CHAR_ARR:
            fwprintf(logStream, L"char array\n");
            break;
        case IR_DT_BOOL_ARR:
            fwprintf(logStream, L"bool array\n");
            break;
        case IR_DT_INT_REFER:
            fwprintf(logStream, L"int reference\n");
            break;
        case IR_DT_FLOAT_REFER:
            fwprintf(logStream, L"float reference\n");
            break;
        case IR_DT_STRING_REFER:
            fwprintf(logStream, L"string reference\n");
            break;
        case IR_DT_CHAR_REFER:
            fwprintf(logStream, L"char reference\n");
            break;
        case IR_DT_BOOL_REFER:
            fwprintf(logStream, L"bool reference\n");
            break;
        }
    }
}
void showIRClassInfo(IR_Class* cls) {
    if (!cls) return;
    initLocale();
    fwprintf(logStream, L"类名: %ls\n", cls->name);
    fwprintf(logStream, L"父类: %ls\n", cls->parent_name ? cls->parent_name : L"无");
    fwprintf(logStream, L"类体信息:\n");
    fwprintf(logStream, L"\t私有成员个数: %d\n", cls->body.private_member_count);
    for (int i = 0; i < cls->body.private_member_count; i++) {
        IR_ClassMember* member = &cls->body.privateMembers[i];
        if (member->type == IR_CM_VARIABLE) {
            fwprintf(logStream, L"\t\t变量: 名字=%ls, 类型=", member->data.variable->name);
            switch (member->data.variable->type.kind) {
            case IR_DT_INT:
                fwprintf(logStream, L"int\n");
                break;
            case IR_DT_FLOAT:
                fwprintf(logStream, L"float\n");
                break;
            case IR_DT_STRING:
                fwprintf(logStream, L"string\n");
                break;
            case IR_DT_CHAR:
                fwprintf(logStream, L"char\n");
                break;
            case IR_DT_BOOL:
                fwprintf(logStream, L"bool\n");
                break;
            case IR_DT_VOID:
                fwprintf(logStream, L"void\n");
                break;
            case IR_DT_CUSTOM:
                fwprintf(logStream, L"custom(%ls)\n", member->data.variable->type.customTypeName);
                break;
            }
        } else if (member->type == IR_CM_FUNCTION) {
            fwprintf(logStream, L"\t\t函数:\n");
            showFunctionInfo(member->data.function);
        }
    }
    fwprintf(logStream, L"\t公有成员个数: %d\n", cls->body.public_member_count);
    for (int i = 0; i < cls->body.public_member_count; i++) {
        IR_ClassMember* member = &cls->body.publicMembers[i];
        if (member->type == IR_CM_VARIABLE) {
            fwprintf(logStream, L"\t\t变量: 名字=%ls, 类型=", member->data.variable->name);
            switch (member->data.variable->type.kind) {
            case IR_DT_INT:
                fwprintf(logStream, L"int\n");
                break;
            case IR_DT_FLOAT:
                fwprintf(logStream, L"float\n");
                break;
            case IR_DT_STRING:
                fwprintf(logStream, L"string\n");
                break;
            case IR_DT_CHAR:
                fwprintf(logStream, L"char\n");
                break;
            case IR_DT_BOOL:
                fwprintf(logStream, L"bool\n");
                break;
            case IR_DT_VOID:
                fwprintf(logStream, L"void\n");
                break;
            case IR_DT_CUSTOM:
                fwprintf(logStream, L"custom(%ls)\n", member->data.variable->type.customTypeName);
                break;
            }
        } else if (member->type == IR_CM_FUNCTION) {
            fwprintf(logStream, L"\t\t函数:\n");
            showFunctionInfo(member->data.function);
        }
    }
    fwprintf(logStream, L"\t受保护成员个数: %d\n", cls->body.protected_member_count);
    for (int i = 0; i < cls->body.protected_member_count; i++) {
        IR_ClassMember* member = &cls->body.protectedMembers[i];
        if (member->type == IR_CM_VARIABLE) {
            fwprintf(logStream, L"\t\t变量: 名字=%ls, 类型=", member->data.variable->name);
            switch (member->data.variable->type.kind) {
            case IR_DT_INT:
                fwprintf(logStream, L"int\n");
                break;
            case IR_DT_FLOAT:
                fwprintf(logStream, L"float\n");
                break;
            case IR_DT_STRING:
                fwprintf(logStream, L"string\n");
                break;
            case IR_DT_CHAR:
                fwprintf(logStream, L"char\n");
                break;
            case IR_DT_BOOL:
                fwprintf(logStream, L"bool\n");
                break;
            case IR_DT_VOID:
                fwprintf(logStream, L"void\n");
                break;
            case IR_DT_CUSTOM:
                fwprintf(logStream, L"custom(%ls)\n", member->data.variable->type.customTypeName);
                break;
            }
        } else if (member->type == IR_CM_FUNCTION) {
            fwprintf(logStream, L"\t\t函数:\n");
            showFunctionInfo(member->data.function);
        }
    }
}
void showIRProgramInfo(IR_Program* program) {
    if (!program) return;
    initLocale();
    fwprintf(logStream, L"======= 中间表示信息 =======\n");
    fwprintf(logStream, L"全局变量个数: %d\n", program->global_variable_count);
    for (int i = 0; i < program->global_variable_count; i++) {
        IR_Variable* var = program->global_variables[i];
        fwprintf(logStream, L"\t变量%d: 名字=%ls, 类型=", i + 1, var->name);
        switch (var->type.kind) {
        case IR_DT_INT:
            fwprintf(logStream, L"int\n");
            break;
        case IR_DT_FLOAT:
            fwprintf(logStream, L"float\n");
            break;
        case IR_DT_STRING:
            fwprintf(logStream, L"string\n");
            break;
        case IR_DT_CHAR:
            fwprintf(logStream, L"char\n");
            break;
        case IR_DT_BOOL:
            fwprintf(logStream, L"bool\n");
            break;
        case IR_DT_VOID:
            fwprintf(logStream, L"void\n");
            break;
        case IR_DT_CUSTOM:
            fwprintf(logStream, L"custom(%ls)\n", var->type.customTypeName);
            break;
        }
    }
    fwprintf(logStream, L"函数个数: %d\n", program->functions.size());
    for (int i = 0; i < program->functions.size(); i++) {
        fwprintf(logStream, L"---- 函数%d ----\n", i + 1);
        showFunctionInfo(program->functions[i]);
    }
    fwprintf(logStream, L"类个数: %d\n", program->class_count);
    for (int i = 0; i < program->class_count; i++) {
        fwprintf(logStream, L"---- 类%d ----\n", i + 1);
        showIRClassInfo(program->classes[i]);
    }
    return;
}
#endif

/**辅助函数，解析类体
 * @param tokens 词法分析结果
 * @param start_index 类体开始索引
 * @param end_index 类体结束索引
 * @param err 错误码输出指针
 * @return 解析得到的类体结构体(不是指针)
 */
static IR_ClassBody parseClassBody(Tokens* tokens, int start_index, int end_index, int* err) {
    if (!tokens || err == NULL) {
        if (err) *err = -1;
        IR_ClassBody body = {0};
        return body;
    }
    IR_ClassBody body = {0};
    int index = start_index;
    enum { PARSE_PRIVATE_MEMBERS, PARSE_PUBLIC_MEMBERS, PARSE_PROTECTED_MEMBERS } state = PARSE_PUBLIC_MEMBERS;
    // 成员::=[访问修饰符:] 定义函数|定义变量
    while (index <= end_index) {
        if (wcscmp(tokens->tokens[index].value, L"公有成员") == 0 || wcscmp(tokens->tokens[index].value, L"public") == 0) {
            if (index >= end_index) {
                setError(ERR_DEF_CLASS_ACCESS, tokens->tokens[index].line, NULL);
                *err = 255;
                return body;
            }
            index++;
            if (tokens->tokens[index].type != TOK_OPR_COLON) {
                setError(ERR_DEF_CLASS_ACCESS, tokens->tokens[index].line, NULL);
                *err = 255;
                return body;
            }
            index++;
            state = PARSE_PUBLIC_MEMBERS;
            continue;
        } else if (wcscmp(tokens->tokens[index].value, L"受保护成员") == 0 ||
                   wcscmp(tokens->tokens[index].value, L"protected") == 0) {
            if (index >= end_index) {
                setError(ERR_DEF_CLASS_ACCESS, tokens->tokens[index].line, NULL);
                *err = 255;
                return body;
            }
            index++;
            if (tokens->tokens[index].type != TOK_OPR_COLON) {
                setError(ERR_DEF_CLASS_ACCESS, tokens->tokens[index].line, NULL);
                *err = 255;
                return body;
            }
            state = PARSE_PROTECTED_MEMBERS;
            index++;
            continue;
        } else if (wcscmp(tokens->tokens[index].value, L"私有成员") == 0 ||
                   wcscmp(tokens->tokens[index].value, L"private") == 0) {
            if (index >= end_index) {
                setError(ERR_DEF_CLASS_ACCESS, tokens->tokens[index].line, NULL);
                *err = 255;
                return body;
            }
            index++;
            if (tokens->tokens[index].type != TOK_OPR_COLON) {
                setError(ERR_DEF_CLASS_ACCESS, tokens->tokens[index].line, NULL);
                *err = 255;
                return body;
            }
            state = PARSE_PRIVATE_MEMBERS;
            index++;
            continue;
        }
        if (wcscmp(tokens->tokens[index].value, L"定义函数") == 0 || wcscmp(tokens->tokens[index].value, L"fun") == 0) {
            IR_Function* func = parseFunction(tokens, &index, err);
            if (func == NULL || *err != 0) {
                return body;
            }
            if (state == PARSE_PRIVATE_MEMBERS) {
                body.private_member_count++;
                body.privateMembers =
                    (IR_ClassMember*)realloc(body.privateMembers, sizeof(IR_ClassMember) * body.private_member_count);
                if (!body.privateMembers) {
                    *err = -1;
                    return body;
                }
                body.privateMembers[body.private_member_count - 1].type = IR_CM_FUNCTION;
                body.privateMembers[body.private_member_count - 1].data.function = func;
            } else if (state == PARSE_PUBLIC_MEMBERS) {
                body.public_member_count++;
                body.publicMembers =
                    (IR_ClassMember*)realloc(body.publicMembers, sizeof(IR_ClassMember) * body.public_member_count);
                if (!body.publicMembers) {
                    *err = -1;
                    return body;
                }
                body.publicMembers[body.public_member_count - 1].type = IR_CM_FUNCTION;
                body.publicMembers[body.public_member_count - 1].data.function = func;
            } else if (state == PARSE_PROTECTED_MEMBERS) {
                body.protected_member_count++;
                body.protectedMembers =
                    (IR_ClassMember*)realloc(body.protectedMembers, sizeof(IR_ClassMember) * body.protected_member_count);
                if (!body.protectedMembers) {
                    *err = -1;
                    return body;
                }
                body.protectedMembers[body.protected_member_count - 1].type = IR_CM_FUNCTION;
                body.protectedMembers[body.protected_member_count - 1].data.function = func;
            }
#ifdef HX_DEBUG
            wchar_t access[4] = {0};
            if (state == PARSE_PRIVATE_MEMBERS) {
                wcscpy(access, L"私有");
            } else if (state == PARSE_PUBLIC_MEMBERS) {
                wcscpy(access, L"公有");
            } else if (state == PARSE_PROTECTED_MEMBERS) {
                wcscpy(access, L"受保护");
            }
            log(L"解析到类的函数成员：%ls  (%ls)", func->name, access);
#endif
            // index++;
        } else if (wcscmp(tokens->tokens[index].value, L"var") == 0 || wcscmp(tokens->tokens[index].value, L"定义变量") == 0 ||
                   wcscmp(tokens->tokens[index].value, L"con") == 0 || wcscmp(tokens->tokens[index].value, L"定义常量") == 0) {
            IR_Variable* var = parseGlobalOrClassVariable(tokens, &index, err);
            if (var == NULL || *err != 0) {
                return body;
            }
            body.private_member_count++;
            body.privateMembers =
                (IR_ClassMember*)realloc(body.privateMembers, sizeof(IR_ClassMember) * body.private_member_count);
            body.privateMembers[body.private_member_count - 1].type = IR_CM_VARIABLE;
            body.privateMembers[body.private_member_count - 1].data.variable = var;
#ifdef HX_DEBUG
            wchar_t access[4] = {0};
            if (state == PARSE_PRIVATE_MEMBERS) {
                wcscpy(access, L"私有");
            } else if (state == PARSE_PUBLIC_MEMBERS) {
                wcscpy(access, L"公有");
            } else if (state == PARSE_PROTECTED_MEMBERS) {
                wcscpy(access, L"受保护");
            }
            log(L"解析到类的变量成员：%ls (%ls)", var->name, access);
#endif
            // index++;
        }
    }
    return body;
}
// 定义类::= class:[标识符->] 标识符 { 类体 }
/** 解析类定义(英文关键字)
 * @param tokens 词法分析结果
 * @param index 当前解析位置
 * @param err 错误码输出指针
 * @return 解析得到的类结构体指针
 */
static IR_Class* parseClass_EN(Tokens* tokens, int* index, int* err) {
    if (!tokens || !index || err == NULL) {
        if (err) *err = -1;
        return NULL;
    }
    if ((*index + 1) >= tokens->count) {
        *err = 255;  // 语法错误
        setError(ERR_DEF_CLASS, tokens->tokens[*index].line, NULL);
        return NULL;
    }
    (*index)++;
    if (tokens->tokens[*index].type != TOK_OPR_COLON) {
        setError(ERR_DEF_CLASS, tokens->tokens[*index].line, NULL);
        *err = 255;  // 语法错误
        return NULL;
    }
    if ((*index + 1) >= tokens->count) {
        setError(ERR_DEF_CLASS, tokens->tokens[*index].line, NULL);
        *err = 255;  // 语法错误
        return NULL;
    }
    (*index)++;
    // 类名或父类名
    if (tokens->tokens[*index].type != TOK_ID) {
        setError(ERR_DEF_CLASS, tokens->tokens[*index].line, NULL);
        *err = 255;  // 语法错误
        return NULL;
    }
    int tmpIndex = *index;
    if ((*index + 1) >= tokens->count) {
        setError(ERR_DEF_CLASS, tokens->tokens[*index].line, NULL);
        *err = 255;  // 语法错误
        return NULL;
    }
    (*index)++;

    IR_Class* _class = (IR_Class*)calloc(1, sizeof(IR_Class));
    if (!_class) {
        *err = -1;
        return NULL;
    }
    if (tokens->tokens[*index].type == TOK_OPR_POINT) {
        if ((*index + 1) >= tokens->count) {
            setError(ERR_DEF_CLASS, tokens->tokens[*index].line, NULL);
            *err = 255;
            return NULL;
        }
        (*index)++;
        if (tokens->tokens[*index].type != TOK_ID) {
            setError(ERR_DEF_CLASS, tokens->tokens[*index].line, NULL);
            *err = 255;
            return NULL;
        }
        _class->name = (wchar_t*)calloc(wcslen(tokens->tokens[*index].value) + 1, sizeof(wchar_t));
        if (!_class->name) {
            *err = -1;
            free(_class);
            return NULL;
        }
        wcscpy(_class->name, tokens->tokens[*index].value);
#ifdef HX_DEBUG
        log(L"解析到类名：%ls", _class->name);
#endif
        _class->parent_name = (wchar_t*)calloc(wcslen(tokens->tokens[tmpIndex].value) + 1, sizeof(wchar_t));
        if (!_class->parent_name) {
            *err = -1;
            free(_class->name);
            free(_class);
            return NULL;
        }
        wcscpy(_class->parent_name, tokens->tokens[tmpIndex].value);
#ifdef HX_DEBUG
        log(L"解析到父类名：%ls", _class->parent_name);
#endif
        if ((*index + 1) >= tokens->count) {
            setError(ERR_DEF_CLASS, tokens->tokens[*index].line, NULL);
            *err = 255;  // 语法错误
            free(_class->parent_name);
            free(_class->name);
            free(_class);
            return NULL;
        }
        (*index)++;
    } else {
        _class->name = (wchar_t*)calloc(wcslen(tokens->tokens[tmpIndex].value) + 1, sizeof(wchar_t));
        if (!_class->name) {
            *err = -1;
            free(_class);
            return NULL;
        }
        wcscpy(_class->name, tokens->tokens[tmpIndex].value);
#ifdef HX_DEBUG
        log(L"解析到类名：%ls", _class->name);
#endif
    }
    // 类体
    if (tokens->tokens[*index].type != TOK_OPR_LBRACE) {
        setError(ERR_DEF_CLASS, tokens->tokens[*index].line, NULL);
        *err = 255;  // 语法错误
        if (_class->parent_name) free(_class->parent_name);
        free(_class->name);
        free(_class);
        return NULL;
    }
    // 确定类体范围
    int body_start_index = *index;
    int brace_count = 1;
    while (*index + 1 < tokens->count && brace_count > 0) {
        (*index)++;
        if (tokens->tokens[*index].type == TOK_OPR_LBRACE) {
            brace_count++;
        } else if (tokens->tokens[*index].type == TOK_OPR_RBRACE) {
            brace_count--;
        }

        if (brace_count == 0) {
            break;
        }
    }
    if (brace_count != 0) {
        setError(ERR_HUAKUOHAO_NOT_CLOSE, tokens->tokens[*index].line, tokens->tokens[body_start_index].value);
        *err = 255;  // 语法错误
        if (_class->parent_name) free(_class->parent_name);
        free(_class->name);
        free(_class);
        return NULL;
    }
    // 解析类体
    _class->body = parseClassBody(tokens, body_start_index + 1, *index - 1, err);
    if (*err != 0) {
        if (_class->parent_name) free(_class->parent_name);
        free(_class->name);
        free(_class);
        return NULL;
    }
    (*index)++;
    return _class;
}
// 定义类：<id> [, 父类是：<id>]-> {}
static IR_Class* parseClass_CN(Tokens* tokens, int* index, int* err) {
    if (!tokens || !index || err == NULL) {
        if (err) *err = -1;
        return NULL;
    }
    if ((*index + 1) >= tokens->count) {
        *err = 255;  // 语法错误
        setError(ERR_DEF_CLASS, tokens->tokens[*index].line, NULL);
        return NULL;
    }
    (*index)++;
    if (tokens->tokens[*index].type != TOK_OPR_COLON) {
        setError(ERR_DEF_CLASS, tokens->tokens[*index].line, NULL);
        *err = 255;  // 语法错误
        return NULL;
    }
    if ((*index + 1) >= tokens->count) {
        setError(ERR_DEF_CLASS, tokens->tokens[*index].line, NULL);
        *err = 255;  // 语法错误
        return NULL;
    }
    (*index)++;
    // 类名
    if (tokens->tokens[*index].type != TOK_ID) {
        setError(ERR_DEF_CLASS, tokens->tokens[*index].line, NULL);
        *err = 255;  // 语法错误
        return NULL;
    }
    IR_Class* _class = (IR_Class*)calloc(1, sizeof(IR_Class));
    if (!_class) {
        *err = -1;
        return NULL;
    }
    _class->name = (wchar_t*)calloc(wcslen(tokens->tokens[*index].value) + 1, sizeof(wchar_t));
    if (!_class->name) {
        *err = -1;
        free(_class);
        return NULL;
    }
    wcscpy(_class->name, tokens->tokens[*index].value);
#ifdef HX_DEBUG
    log(L"解析到类名：%ls", _class->name);
#endif
    // 继承或类体
    if ((*index + 1) >= tokens->count) {
        setError(ERR_DEF_CLASS, tokens->tokens[*index].line, NULL);
        *err = 255;  // 语法错误
        free(_class->name);
        free(_class);
        return NULL;
    }
    (*index)++;
    if (tokens->tokens[*index].type == TOK_OPR_COMMA) {
        //,
        if ((*index + 1) >= tokens->count) {
            setError(ERR_DEF_CLASS, tokens->tokens[*index].line, NULL);
            *err = 255;  // 语法错误
            free(_class->name);
            free(_class);
            return NULL;
        }
        (*index)++;
        if (wcscmp(tokens->tokens[*index].value, L"父类是") != 0) {
            setError(ERR_DEF_CLASS, tokens->tokens[*index].line, NULL);
            *err = 255;  // 语法错误
            free(_class->name);
            free(_class);
            return NULL;
        }
        if ((*index + 1) >= tokens->count) {
            setError(ERR_DEF_CLASS, tokens->tokens[*index].line, NULL);
            *err = 255;  // 语法错误
            free(_class->name);
            free(_class);
            return NULL;
        }
        (*index)++;
        if (tokens->tokens[*index].type != TOK_OPR_COLON) {
            setError(ERR_DEF_CLASS, tokens->tokens[*index].line, NULL);
            *err = 255;
            free(_class->name);
            free(_class);
            return NULL;
        }
        if ((*index + 1) >= tokens->count) {
            setError(ERR_DEF_CLASS, tokens->tokens[*index].line, NULL);
            *err = 255;
            free(_class->name);
            free(_class);
            return NULL;
        }
        (*index)++;
        // 父类名
        if (tokens->tokens[*index].type != TOK_ID) {
            setError(ERR_DEF_CLASS, tokens->tokens[*index].line, NULL);
            *err = 255;  // 语法错误
            free(_class->name);
            free(_class);
            return NULL;
        }
        _class->parent_name = (wchar_t*)calloc(wcslen(tokens->tokens[*index].value) + 1, sizeof(wchar_t));
        if (!_class->parent_name) {
            *err = -1;
            free(_class->name);
            free(_class);
            return NULL;
        }
        wcscpy(_class->parent_name, tokens->tokens[*index].value);
#ifdef HX_DEBUG
        log(L"解析到父类名：%ls", _class->parent_name);
#endif
        if ((*index + 1) >= tokens->count) {
            setError(ERR_DEF_CLASS, tokens->tokens[*index].line, NULL);
            *err = 255;  // 语法错误
            free(_class->parent_name);
            free(_class->name);
            free(_class);
            return NULL;
        }
        (*index)++;
    }
    if (tokens->tokens[*index].type != TOK_OPR_POINT) {
        setError(ERR_DEF_CLASS, tokens->tokens[*index].line, NULL);
        *err = 255;  // 语法错误
        if (_class->parent_name) free(_class->parent_name);
        free(_class->name);
        free(_class);
        return NULL;
    }
    if ((*index + 1) >= tokens->count) {
        setError(ERR_DEF_CLASS, tokens->tokens[*index].line, NULL);
        *err = 255;  // 语法错误
        free(_class->parent_name);
        free(_class->name);
        free(_class);
        return NULL;
    }
    (*index)++;
    // 类体
    if (tokens->tokens[*index].type != TOK_OPR_LBRACE) {
        setError(ERR_DEF_CLASS, tokens->tokens[*index].line, NULL);
        *err = 255;  // 语法错误
        if (_class->parent_name) free(_class->parent_name);
        free(_class->name);
        free(_class);
        return NULL;
    }
    // 确定类体范围
    int body_start_index = *index;
    int brace_count = 1;
    while (*index + 1 < tokens->count && brace_count > 0) {
        (*index)++;
        if (tokens->tokens[*index].type == TOK_OPR_LBRACE) {
            brace_count++;
        } else if (tokens->tokens[*index].type == TOK_OPR_RBRACE) {
            brace_count--;
        }

        if (brace_count == 0) {
            break;
        }
    }
    if (brace_count != 0) {
        setError(ERR_HUAKUOHAO_NOT_CLOSE, tokens->tokens[*index].line, tokens->tokens[body_start_index].value);
        *err = 255;  // 语法错误
        if (_class->parent_name) free(_class->parent_name);
        free(_class->name);
        free(_class);
        return NULL;
    }
    // 解析类体
    _class->body = parseClassBody(tokens, body_start_index + 1, *index - 1, err);
    if (*err != 0) {
        if (_class->parent_name) free(_class->parent_name);
        free(_class->name);
        free(_class);
        return NULL;
    }
    (*index)++;
    return _class;
}
IR_Class* parseClass(Tokens* tokens, int* index, int* err) {
    if (!tokens || !index || err == NULL) {
        if (err) *err = -1;
        return NULL;
    }
    IR_Class* _class = NULL;
    if (wcscmp(tokens->tokens[*index].value, L"cls") == 0) {
#ifdef HX_DEBUG
        log(L"开始解析类定义(英文)...");
#endif
        int line = tokens->tokens[*index].line;
        _class = parseClass_EN(tokens, index, err);
        if (*err != 0 || _class == NULL) {
            return NULL;
        }
        _class->line = line;
    } else if (wcscmp(tokens->tokens[*index].value, L"定义类") == 0) {
#ifdef HX_DEBUG
        log(L"开始解析类定义(中文)...");
#endif
        int line = tokens->tokens[*index].line;
        _class = parseClass_CN(tokens, index, err);
        if (*err != 0 || _class == NULL) {
            return NULL;
        }
        _class->line = line;
    }
    return _class;
}
inline static int getDataSize(IR_DataType type) {
    switch (type.kind) {
    case IR_DT_INT:
        return sizeof(int32_t);
    case IR_DT_FLOAT:
        return sizeof(float);
    case IR_DT_CHAR:
        return sizeof(uint16_t);
    case IR_DT_BOOL:
        return 1;
    case IR_DT_STRING:
        return sizeof(uint16_t*);  // 用uint16_t规范wchar_t*指针大小
    }
}
inline IR_Class* getClassByName(const wchar_t* name, IR_Program* currentIRProgram) {
    if (!name || !currentIRProgram) return NULL;
    for (int i = 0; i < currentIRProgram->class_count; i++) {
        if (wcscmp(currentIRProgram->classes[i]->name, name) == 0) {
            return currentIRProgram->classes[i];
        }
    }
    return NULL;
}
// 检查是否存在相互包含
bool checkEachClass(IR_Class* cls, IR_Program* currentIRProgram, std::vector<IR_Class*> lastClasses) {
    if (!cls) return false;
    lastClasses.push_back(cls);
    for (int i = 0; i < cls->body.private_member_count; i++) {
        IR_ClassMember* member = &cls->body.privateMembers[i];
        if (member->type == IR_CM_VARIABLE) {
            if (member->data.variable->type.kind == IR_DT_CUSTOM) {
                for (int i = 0; i < lastClasses.size(); i++) {
                    if (wcscmp(member->data.variable->type.customTypeName, lastClasses.at(i)->name) == 0) {
                        return true;
                    }
                }
                IR_Class* customClass = getClassByName(member->data.variable->type.customTypeName, currentIRProgram);
                if (customClass) {
                    if (checkEachClass(customClass, currentIRProgram, lastClasses)) return true;
                }
            }
        }
    }
    for (int i = 0; i < cls->body.public_member_count; i++) {
        IR_ClassMember* member = &cls->body.publicMembers[i];
        if (member->type == IR_CM_VARIABLE) {
            if (member->data.variable->type.kind == IR_DT_CUSTOM) {
                for (int i = 0; i < lastClasses.size(); i++) {
                    if (wcscmp(member->data.variable->type.customTypeName, lastClasses.at(i)->name) == 0) {
                        return true;
                    }
                }
                IR_Class* customClass = getClassByName(member->data.variable->type.customTypeName, currentIRProgram);
                if (customClass) {
                    if (checkEachClass(customClass, currentIRProgram, lastClasses)) return true;
                }
            }
        }
    }
    for (int i = 0; i < cls->body.protected_member_count; i++) {
        IR_ClassMember* member = &cls->body.protectedMembers[i];
        if (member->type == IR_CM_VARIABLE) {
            if (member->data.variable->type.kind == IR_DT_CUSTOM) {
                for (int i = 0; i < lastClasses.size(); i++) {
                    if (wcscmp(member->data.variable->type.customTypeName, lastClasses.at(i)->name) == 0) {
                        return true;
                    }
                }
                IR_Class* customClass = getClassByName(member->data.variable->type.customTypeName, currentIRProgram);
                if (customClass) {
                    if (checkEachClass(customClass, currentIRProgram, lastClasses)) return true;
                }
            }
        }
    }
    lastClasses.pop_back();
    return false;
}
inline int getClassSize(IR_Class* cls, IR_Program* currentIRProgram) {
    if (!cls) return 0;
    int size = 0;
    // 计算私有成员变量大小
    for (int i = 0; i < cls->body.private_member_count; i++) {
        IR_ClassMember* member = &cls->body.privateMembers[i];
        if (member->type == IR_CM_VARIABLE) {
            if (member->data.variable->type.kind == IR_DT_CUSTOM) {
                // 自定义类型，递归计算大小
                IR_Class* customClass = getClassByName(member->data.variable->type.customTypeName, currentIRProgram);
                if (customClass) {
                    size += getClassSize(customClass, currentIRProgram);
                }
            } else {
                size += getDataSize(member->data.variable->type);
            }
        }
    }
    // 计算公有成员变量大小
    for (int i = 0; i < cls->body.public_member_count; i++) {
        IR_ClassMember* member = &cls->body.publicMembers[i];
        if (member->type == IR_CM_VARIABLE) {
            if (member->data.variable->type.kind == IR_DT_CUSTOM) {
                // 自定义类型，递归计算大小
                IR_Class* custom_cls = getClassByName(member->data.variable->type.customTypeName, currentIRProgram);
                if (custom_cls) {
                    size += getClassSize(custom_cls, currentIRProgram);
                }
            } else {
                size += getDataSize(member->data.variable->type);
            }
        }
    }
    // 计算受保护成员变量大小
    for (int i = 0; i < cls->body.protected_member_count; i++) {
        IR_ClassMember* member = &cls->body.protectedMembers[i];
        if (member->type == IR_CM_VARIABLE) {
            if (member->type == IR_CM_VARIABLE) {
                if (member->data.variable->type.kind == IR_DT_CUSTOM) {
                    // 自定义类型，递归计算大小
                    IR_Class* custom_cls = getClassByName(member->data.variable->type.customTypeName, currentIRProgram);
                    if (custom_cls) {
                        size += getClassSize(custom_cls, currentIRProgram);
                    }
                } else {
                    size += getDataSize(member->data.variable->type);
                }
            }
        }
    }
    return size;
}
#endif