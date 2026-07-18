#pragma once

#include "Parser.h"
#include "ObjectCode.h"
#include "Error.h"
#include "IR.h"
#include "Lexer.h"
#include "SymbolTable.h"

static void generateInstructionsFromAST(std::vector<Instruction>& instructions, int* inst_index, int* inst_size, ASTNode* node,
                                        ConstantPool* constantPool, std::vector<SymbolTable>& symbols, int procIndex, int* err);
                                        static int getVarSize(IR_DataType type, std::vector<IR_Class*>& class_table);

class PackedClassFunMem {
   public:
    PackedClassFunMem() : irFun(nullptr), cls(nullptr) {}
    enum { FUN_PUBLIC, FUN_PRIVATE, FUN_PROTECTED } accessPermission;
    IR_Function* irFun;
    IR_Class* cls;  // 不一定指向用户语句中的类，但该类一定包含该函数且是用户语句中的类的父类
};
PackedClassFunMem* findFunInClass(IR_Function& fun, IR_Class* cls, std::vector<IR_Class*>& classTable) {
#ifdef HX_DEBUG
    log(L"查找类的成员函数%ls", fun.name ? fun.name : L"(null)");
#endif
    if (cls == nullptr) {
        return nullptr;
    }
    PackedClassFunMem* packedClassFunMem = new PackedClassFunMem;
    for (int i = 0; i < cls->body.publicMembers.size(); i++) {
        if (cls->body.publicMembers.at(i).type == IR_CM_FUNCTION) {
            IR_Function* funInClass = cls->body.publicMembers.at(i).data.function;
            if (wcscmp(funInClass->name, fun.name) == 0) {
                if (funInClass->paramCount == fun.paramCount) {
                    for (int j = 0; j < fun.paramCount; j++) {
                        if (fun.params[j].type.kind == funInClass->params[j].type.kind) {
                            if (fun.params[j].type.kind == IR_DT_CUSTOM || fun.params[j].type.kind == IR_DT_CUSTOM_ARR) {
                                if (wcscmp(fun.params[j].type.customTypeName, funInClass->params[j].type.customTypeName) != 0) {
                                    continue;
                                }
                                packedClassFunMem->accessPermission = PackedClassFunMem::FUN_PUBLIC;
                                packedClassFunMem->cls = cls;
                                packedClassFunMem->irFun = funInClass;
                                return packedClassFunMem;
                            }
                        }
                    }
                }
            }
        }
    }
    for (int i = 0; i < cls->body.privateMembers.size(); i++) {
        if (cls->body.privateMembers.at(i).type == IR_CM_FUNCTION) {
            IR_Function* funInClass = cls->body.privateMembers.at(i).data.function;
            if (wcscmp(funInClass->name, fun.name) == 0) {
                if (funInClass->paramCount == fun.paramCount) {
                    for (int j = 0; j < fun.paramCount; j++) {
                        if (fun.params[j].type.kind == funInClass->params[j].type.kind) {
                            if (fun.params[j].type.kind == IR_DT_CUSTOM || fun.params[j].type.kind == IR_DT_CUSTOM_ARR) {
                                if (wcscmp(fun.params[j].type.customTypeName, funInClass->params[j].type.customTypeName) != 0) {
                                    continue;
                                }
                                packedClassFunMem->accessPermission = PackedClassFunMem::FUN_PRIVATE;
                                packedClassFunMem->cls = cls;
                                packedClassFunMem->irFun = funInClass;
                                return packedClassFunMem;
                            }
                        }
                    }
                }
            }
        }
    }
    for (int i = 0; i < cls->body.protectedMembers.size(); i++) {
        if (cls->body.protectedMembers.at(i).type == IR_CM_FUNCTION) {
            IR_Function* funInClass = cls->body.protectedMembers.at(i).data.function;
            if (wcscmp(funInClass->name, fun.name) == 0) {
                if (funInClass->paramCount == fun.paramCount) {
                    for (int j = 0; j < fun.paramCount; j++) {
                        if (fun.params[j].type.kind == funInClass->params[j].type.kind) {
                            if (fun.params[j].type.kind == IR_DT_CUSTOM || fun.params[j].type.kind == IR_DT_CUSTOM_ARR) {
                                if (wcscmp(fun.params[j].type.customTypeName, funInClass->params[j].type.customTypeName) != 0) {
                                    continue;
                                }
                                packedClassFunMem->accessPermission = PackedClassFunMem::FUN_PROTECTED;
                                packedClassFunMem->cls = cls;
                                packedClassFunMem->irFun = funInClass;
                                return packedClassFunMem;
                            }
                        }
                    }
                }
            }
        }
    }
    // 没找到就查找父类
    if (cls->fatherIndex == -1) {
        return nullptr;
    }
    return findFunInClass(fun, classTable.at(cls->fatherIndex), classTable);
}
static int generateClassFunctionStatement(
    int& index, FunCallPitchTable& pitchTable, ConstantPool* constantPool, int classIndex, int globalFunctionCount,
    std::vector<IR_Function*>& allFunctions, IR_Program* currentProgram, int allFunctionCount, IR_Function* function,
    SymbolTable& localeSymbolTable, std::vector<SymbolTable>& outsideScopes /*块内与块外的并集,localeSymbolTable包含其中*/,
    int localeScopeIndex, Procedure* proc, int procIndex, uint32_t& stackSize, uint32_t& localVarSize, int* err);

static int generateClassFunctionStatement(
    int& index, FunCallPitchTable& pitchTable, ConstantPool* constantPool, int classIndex, int globalFunctionCount,
    std::vector<IR_Function*>& allFunctions, IR_Program* currentProgram, int allFunctionCount, IR_Function* function,
    SymbolTable& localeSymbolTable, std::vector<SymbolTable>& outsideScopes /*块内与块外的并集,localeSymbolTable包含其中*/,
    int localeScopeIndex, Procedure* proc, int procIndex, uint32_t& stackSize, uint32_t& localVarSize, int* err) {
    if (!function || !proc || !err) return -1;
    Token& currentToken = function->bodyTokens[index];
    if (currentToken.type == TOK_END) return 0;
    // 弹出上一步JMP指令附带的占位用NOP
    if (proc->instructions.size() > 1 && proc->instructions.at(proc->instructions.size() - 1).opcode == OP_NOP) {
        proc->instructions.pop_back();
    }
    if (currentToken.type == TOK_OPR_LBRACE) {  // 块
#ifdef HX_DEBUG
        log(L"========分析块-----");
#endif
        index++;  // 跳过 {
        SymbolTable newLocalScope = {};
        newLocalScope.fun = outsideScopes.back().fun;
        outsideScopes.push_back(newLocalScope);
        uint32_t _localVarSize = 0U;
        int newLocaleScopeIndex = outsideScopes.size() - 1;
        while (function->bodyTokens[index].type != TOK_OPR_RBRACE) {
            generateClassFunctionStatement(index, pitchTable, constantPool, classIndex, globalFunctionCount, allFunctions, currentProgram, allFunctionCount, function,
                              outsideScopes.back(), outsideScopes, newLocaleScopeIndex, proc, procIndex, stackSize,
                              _localVarSize, err);
            if (*err) return *err;
            index++;
        }

#ifdef HX_DEBUG
        log(L"========离开块------");
#endif
    }
    // 返回返回值
    if (wcscmp(currentToken.value, L"ret") == 0 || wcscmp(currentToken.value, L"返回") == 0) {
#ifdef HX_DEBUG
        log(L"解析到返回语句\n");
#endif
        /* 语法：ret : <expression> ;
         */
        if (index + 1 >= function->body_token_count) {
            setError(ERR_RET, currentToken.line, NULL);
            *err = 255;
            delete (proc);
            return 255;
        }
        index++;  // 指向冒号
        if (function->bodyTokens[index].type == TOK_END) {
            if (function->returnType.kind != IR_DT_VOID && function->isReturnTypeKnown) {
                setError(ERR_RET_VAL, currentToken.line, NULL);
                *err = 255;
                delete (proc);
                return 255;
            }
            // 空返回
#ifdef HX_DEBUG
            log(L"生成空返回指令\n");
#endif
            Instruction newInst = {};
            newInst.opcode = OP_RET;
            proc->instructions.push_back(newInst);
            return 0;
        }
        //:
        if (function->bodyTokens[index].type != TOK_OPR_COLON) {
            setError(ERR_RET, currentToken.line, NULL);
            *err = 255;
            delete (proc);
            return 255;
        }
        index++;  // 指向表达式起始位置
        // wprintf(L"%ls", function->bodyTokens[index].value);
        ASTNode* expNode = parseExpression(function->bodyTokens, &index, function->body_token_count, pitchTable,
                                           &localeSymbolTable, outsideScopes, localeScopeIndex, err);
        if (*err != 0 || !expNode) {
            delete (proc);
            return 255;
        }
#ifdef HX_DEBUG
        log(L"生成返回值表达式指令...\n");
#endif
        // 生成表达式指令
        int inst_index = proc->instructions.size();
        int inst_size = proc->instructions.size();
        generateInstructionsFromAST(proc->instructions, &inst_index, &inst_size, expNode, constantPool, outsideScopes,
                                    procIndex, err);
        if (*err != 0) {
            freeAST(expNode);
            return 255;
        }
        freeAST(expNode);
        // 生成返回指令
        Instruction newInst = {};
        newInst.opcode = OP_RET;
        proc->instructions.push_back(newInst);
        index++;
        if (function->bodyTokens[index].type != TOK_END) {
            setError(ERR_RET, currentToken.line, NULL);
            *err = 255;
            freeAST(expNode);
            delete (proc);
            return 255;
        }
        // 向标准输出流写字符串
    } else if (wcscmp(currentToken.value, L"__hx_write_string__") == 0) {
        if (index + 1 >= function->body_token_count) {
            return 0;
        }
        index++;
        if ((function->bodyTokens[index].type != TOK_VAL) || (function->bodyTokens[index].mark != STR)) return 0;
        Instruction newInst = {};
        newInst.opcode = OP_PRINT_STRING;

        constantPool->constants = (Constant*)realloc(constantPool->constants, sizeof(Constant) * (constantPool->size + 1));
        if (!constantPool->constants) {
            *err = -1;
            return -1;
        }
        constantPool->constants[constantPool->size].type = CONST_STRING;
        constantPool->constants[constantPool->size].value.string_value =
            (wchar_t*)calloc(wcslen(function->bodyTokens[index].value) + 1, sizeof(wchar_t));
        if (!constantPool->constants[constantPool->size].value.string_value) {
            *err = -1;
            return -1;
        }
        wcscpy(constantPool->constants[constantPool->size].value.string_value, function->bodyTokens[index].value);
        constantPool->constants[constantPool->size].size =
            (uint16_t)wcslen(function->bodyTokens[index].value) * sizeof(uint16_t);
        constantPool->size += 1;
        newInst.params[0].type = PARAM_TYPE_INDEX;
        uint32_t strIndex = constantPool->size - 1;
        memcpy(newInst.params[0].value, &(strIndex), sizeof(uint32_t));
        newInst.params[0].size = sizeof(uint32_t);

        proc->instructions.push_back(newInst);
        if (index + 1 >= function->body_token_count) {
            return 0;
        }
        index++;
        if ((function->bodyTokens[index].type != TOK_END)) return 0;
    } else if (currentToken.type == TOK_ID) {  // 赋值或调用函数
        ASTNode* expNode = parseExpression(function->bodyTokens, &index, function->body_token_count, pitchTable,
                                           &localeSymbolTable, outsideScopes, localeScopeIndex, err);
        if (*err != 0 || !expNode) {
            delete (proc);
            return 255;
        }
        // 生成表达式指令
        int inst_index = proc->instructions.size();
        int inst_size = proc->instructions.size();
        generateInstructionsFromAST(proc->instructions, &inst_index, &inst_size, expNode, constantPool, outsideScopes,
                                    procIndex, err);
        if (expNode->kind == NODE_FUN_CALL) {
#ifdef HX_DEBUG
            log(L"分析表达式->调用函数");
#endif
            // 有返回值要手动弹出
            if (expNode->data.funCall.ret_type.kind != IR_DT_VOID) {
#ifdef HX_DEBUG
                log(L"手动弹出返回值");
#endif
                Instruction popInst = {};
                popInst.opcode = OP_POP;
                proc->instructions.push_back(popInst);
            }
        }
        Opcode lastOp = proc->instructions.back().opcode;
        if (lastOp == OP_STORE_VAR || lastOp == OP_STORE_ARRAY_ELEMENT) {
            Instruction popInst = {};
            popInst.opcode = OP_POP;
#ifdef HX_DEBUG
            log(L"手动弹出赋值");
#endif
            int storeInstIndex = proc->instructions.size() - 1;  // 拿到赋值指令的索引喵
            int popInstIndex = proc->instructions.size();        // OP_POP即将插入的索引喵
            proc->instructions.push_back(popInst);

            for (int i = 0; i < outsideScopes.size(); i++) {
                for (int j = 0; j < outsideScopes[i].vars.size(); j++) {
                    // 变量的指令列表最后一条刚好是刚刚的赋值指令
                    if (!outsideScopes[i].vars[j].instIndex.empty() &&
                            outsideScopes[i].vars[j].instIndex.back() == storeInstIndex) {
                        outsideScopes[i].vars[j].instIndex.push_back(popInstIndex);
                    }
                }
            }
        }
        if (*err != 0) {
            freeAST(expNode);
            return 255;
        }
        freeAST(expNode);
    } else if (wcscmp(currentToken.value, L"var") == 0) {  // var:id[:type][=exp];
        Symbol newVar = {};
        newVar.procIndex = procIndex;
        if (index + 1 >= function->body_token_count) {
            setError(ERR_DEF_VAR, currentToken.line, NULL);
            *err = 255;
            delete (proc);
            return 255;
        }
        index++;  // 指向冒号
        if (function->bodyTokens[index].type != TOK_OPR_COLON) {
            setError(ERR_DEF_VAR, currentToken.line, NULL);
            *err = 255;
            delete (proc);
            return 255;
        }
        if (index + 1 >= function->body_token_count) {
            setError(ERR_DEF_VAR, currentToken.line, NULL);
            *err = 255;
            delete (proc);
            return 255;
        }
        index++;  // 指向标识符
        if (function->bodyTokens[index].type != TOK_ID) {
            setError(ERR_DEF_VAR, currentToken.line, NULL);
            *err = 255;
            delete (proc);
            return 255;
        }
        newVar.name = (wchar_t*)calloc(wcslen(function->bodyTokens[index].value) + 1, sizeof(wchar_t));
        if (!(newVar.name)) {
            *err = -1;
            delete (proc);
            return -1;
        }
        wcscpy(newVar.name, function->bodyTokens[index].value);
#ifdef HX_DEBUG
        log(L"解析到局部变量“%ls”", newVar.name);
#endif
        // 检查变量唯一性
        if (getVarIndex(newVar.name, &localeSymbolTable) != -1) {
            *err = 255;
            setError(ERR_VAR_REPEATED, currentToken.line, NULL);
            delete (proc);
            return 255;
        }
        if (index + 1 >= function->body_token_count) {
            setError(ERR_DEF_VAR, currentToken.line, NULL);
            *err = 255;
            delete (proc);
            return 255;
        }
        index++;  // 指向结束标志或:或=
        if (function->bodyTokens[index].type == TOK_OPR_COLON) {
            newVar.isTypeKnown = true;
            if (index + 1 >= function->body_token_count) {
                setError(ERR_DEF_VAR, currentToken.line, NULL);
                *err = 255;
                delete (proc);
                return 255;
            }
            index++;  // 指向类型名
            if (function->bodyTokens[index].type != TOK_ID && function->bodyTokens[index].type != TOK_KW) {
                setError(ERR_DEF_VAR, currentToken.line, NULL);
                *err = 255;
                delete (proc);
                return 255;
            }
#ifdef HX_DEBUG
            log(L"解析到局部变量的类型名“%ls”", function->bodyTokens[index].value);
#endif
            // 提前越界检查，仅检查
            if (index + 1 >= function->body_token_count) {
                setError(ERR_DEF_VAR, currentToken.line, NULL);
                *err = 255;
                delete (proc);
                return 255;
            }
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
                        *err = 255;
                        delete (proc);
                        return 255;
                    }
                    index++;
                    if (function->bodyTokens[index + 1].type != TOK_OPR_RBRACKET &&
                            function->bodyTokens[index + 2].type != TOK_OPR_RBRACKET) {
                        setError(ERR_TYPE, currentToken.line, NULL);
                        *err = 255;
                        delete (proc);
                        return 255;
                    }
                    int arrSize = -1;
                    if (function->bodyTokens[index + 1].type == TOK_VAL) {
                        arrSize = wcstol(function->bodyTokens[index + 1].value, nullptr, 0);
                        newVar.type.arrayLength = arrSize;
#ifdef HX_DEBUG
                        log(L"数组长度：%d", newVar.type.arrayLength);
#endif
                        index++;
                    }
                    newVar.type.kind = IR_DT_INT_ARR;
                    newVar.size = 4;
                    index++;
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
                        *err = 255;
                        delete (proc);
                        return 255;
                    }
                    index++;
                    if (function->bodyTokens[index + 1].type != TOK_OPR_RBRACKET &&
                            function->bodyTokens[index + 2].type != TOK_OPR_RBRACKET) {
                        setError(ERR_TYPE, currentToken.line, NULL);
                        *err = 255;
                        delete (proc);
                        return 255;
                    }
                    int arrSize = -1;
                    if (function->bodyTokens[index + 1].type == TOK_VAL) {
                        arrSize = wcstol(function->bodyTokens[index + 1].value, nullptr, 0);
                        newVar.type.arrayLength = arrSize;
                        index++;
                    }
                    newVar.type.kind = IR_DT_FLOAT_ARR;
                    newVar.size = 4;
                    index++;
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
                        *err = 255;
                        delete (proc);
                        return 255;
                    }
                    index++;
                    if (function->bodyTokens[index + 1].type != TOK_OPR_RBRACKET &&
                            function->bodyTokens[index + 2].type != TOK_OPR_RBRACKET) {
                        setError(ERR_TYPE, currentToken.line, NULL);
                        *err = 255;
                        delete (proc);
                        return 255;
                    }
                    int arrSize = -1;
                    if (function->bodyTokens[index + 1].type == TOK_VAL) {
                        arrSize = wcstol(function->bodyTokens[index + 1].value, nullptr, 0);
                        newVar.type.arrayLength = arrSize;
                        index++;
                    }
                    newVar.type.kind = IR_DT_CHAR_ARR;
                    newVar.size = 4;
                    index++;
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
                        *err = 255;
                        delete (proc);
                        return 255;
                    }
                    index++;
                    if (function->bodyTokens[index + 1].type != TOK_OPR_RBRACKET &&
                            function->bodyTokens[index + 2].type != TOK_OPR_RBRACKET) {
                        setError(ERR_TYPE, currentToken.line, NULL);
                        *err = 255;
                        delete (proc);
                        return 255;
                    }
                    int arrSize = -1;
                    if (function->bodyTokens[index + 1].type == TOK_VAL) {
                        arrSize = wcstol(function->bodyTokens[index + 1].value, nullptr, 0);
                        newVar.type.arrayLength = arrSize;
                        index++;
                    }
                    newVar.type.kind = IR_DT_STRING_ARR;
                    index++;
                }
            }
            if (function->bodyTokens[index].type == TOK_ID) {
                newVar.type.kind = IR_DT_CUSTOM;
                if (getClassByName(function->bodyTokens[index].value, currentProgram) == NULL) {
                    setError(ERR_UNKNOWN_TYPE, function->bodyTokens[index].line, function->bodyTokens[index].value);
                    *err = 255;
                    return 255;
                }
                newVar.type.customTypeName = (wchar_t*)calloc(wcslen(function->bodyTokens[index].value) + 1, sizeof(wchar_t));
                if (!(newVar.type.customTypeName)) {
                    *err = -1;
                    delete (proc);
                    return -1;
                }
                wcscpy(newVar.type.customTypeName, function->bodyTokens[index].value);
                if (function->bodyTokens[index + 1].type == TOK_OPR_REFER) {
                    newVar.type.kind = IR_DT_CUSTOM_REFER;
                    index++;
                } else if (function->bodyTokens[index + 1].type == TOK_OPR_LBRACKET) {
                    if (index + 2 >= function->body_token_count) {
                        setError(ERR_TYPE, currentToken.line, NULL);
                        *err = 255;
                        delete (proc);
                        return 255;
                    }
                    index++;
                    if (function->bodyTokens[index + 1].type != TOK_OPR_RBRACKET &&
                            function->bodyTokens[index + 2].type != TOK_OPR_RBRACKET) {
                        setError(ERR_TYPE, currentToken.line, NULL);
                        *err = 255;
                        delete (proc);
                        return 255;
                    }
                    int arrSize = -1;
                    if (function->bodyTokens[index + 1].type == TOK_VAL) {
                        arrSize = wcstol(function->bodyTokens[index + 1].value, nullptr, 0);
                        newVar.type.arrayLength = arrSize;
                        index++;
                    }
                    newVar.type.kind = IR_DT_CUSTOM_ARR;
                    index++;
                }
            }
        }
        if (index + 1 >= function->body_token_count) {
            setError(ERR_DEF_VAR, currentToken.line, NULL);
            *err = 255;
            delete (proc);
            return 255;
        }
        index++;

        bool isInit = false;
        Instruction initInst = {};  // 初始化的指令
        int initInstBegin = proc->instructions.size();
        if (function->bodyTokens[index].type == TOK_OPR_SET) {
            isInit = true;
            if (newVar.isTypeKnown) {  //  <=> 已显示声明类型
            }
            newVar.isTypeKnown = true;
        }
        // 若为数组，需生成分配堆内存的指令
        if (newVar.isTypeKnown && newVar.type.arrayLength != 0) {
            // 分配堆内存的指令应在初始化指令之前
            if (newVar.type.kind == IR_DT_CHAR_ARR || newVar.type.kind == IR_DT_CUSTOM_ARR ||
                    newVar.type.kind == IR_DT_FLOAT_ARR || newVar.type.kind == IR_DT_INT_ARR ||
                    newVar.type.kind == IR_DT_STRING_ARR) {  // alloc + store ptr + store length
                int allocInstIndex = 0;
                if (isInit) {
                    proc->instructions.insert(proc->instructions.begin() + initInstBegin, 4, Instruction{OP_NOP});
                    allocInstIndex = initInstBegin;
                } else {
                    proc->instructions.resize(proc->instructions.size() + 4);
                    if (proc->instructions.size() >= 4) allocInstIndex = proc->instructions.size() - 4;
                }
                // 分配堆内存并将地址压栈
                Instruction& allocInst = proc->instructions.at(allocInstIndex);
                allocInst.opcode = OP_HEAP_ALLOC;
                uint32_t size = newVar.type.arrayLength;
                if (newVar.type.kind == IR_DT_CHAR_ARR) {
                    size = size * sizeof(uint16_t);
                } else if (newVar.type.kind == IR_DT_CUSTOM_ARR) {
                    size = size * 4;
                } else if (newVar.type.kind == IR_DT_FLOAT_ARR) {
                    size = size * sizeof(double);
                } else if (newVar.type.kind == IR_DT_INT_ARR) {
                    size = size * sizeof(int32_t);
                } else if (newVar.type.kind == IR_DT_STRING_ARR) {
                    size = size * 4;
                }
                memcpy(allocInst.params[0].value, &size, sizeof(uint32_t));

                Instruction& movAddrInst = proc->instructions.at(allocInstIndex + 1);
                movAddrInst.opcode = OP_STORE_VAR;
                movAddrInst.params[0].type = PARAM_TYPE_OFFEST;
                movAddrInst.params[1].type = PARAM_TYPE_SIZE;

                Instruction& pushLenInst = proc->instructions.at(allocInstIndex + 2);
                pushLenInst.opcode = OP_LOAD_CONST;
                pushLenInst.params[0].type = PARAM_TYPE_INT;
                pushLenInst.params[1];

                Instruction& movLenInst = proc->instructions.at(allocInstIndex + 3);
                movLenInst.opcode = OP_STORE_VAR;
                movLenInst.params[0].type = PARAM_TYPE_OFFEST;
                movLenInst.params[1].type = PARAM_TYPE_SIZE;
                movLenInst.params[0].size = sizeof(uint32_t);
                movLenInst.params[1].size = sizeof(uint32_t);
                movLenInst.params[0].offest = 8;  // 数组长度存储在变量的偏移量+8处
            }
        }
        // 关联指令
        if (initInstBegin < proc->instructions.size()) {
            for (int i = initInstBegin; i < proc->instructions.size(); i++) {
                newVar.instIndex.push_back(i);
            }
        }
        if (function->bodyTokens[index].type != TOK_END) {
            setError(ERR_DEF_VAR, currentToken.line, NULL);
            *err = 255;
            delete (proc);
            return 255;
        }
#ifdef HX_DEBUG
        log(L"将%ls推入局部符号表", newVar.name);
#endif
        localeSymbolTable.vars.push_back(newVar);
        uint32_t varSize = (uint32_t)getVarSize(newVar.type, currentProgram->classes);
        // if (stackSize + varSize > stackSize) stackSize += varSize;
        if (localVarSize + 1 > localVarSize) localVarSize++;

    } else if (wcscmp(currentToken.value, L"定义变量") == 0) {  // 定义变量: <id> [, 类型是:<kw>|<id>]];
        Symbol newVar = {};
        newVar.procIndex = procIndex;
        if (index + 1 >= function->body_token_count) {
            setError(ERR_DEF_VAR, currentToken.line, NULL);
            *err = 255;
            delete (proc);
            return 255;
        }
        index++;  // 指向冒号
        if (function->bodyTokens[index].type != TOK_OPR_COLON) {
            setError(ERR_DEF_VAR, currentToken.line, NULL);
            *err = 255;
            delete (proc);
            return 255;
        }
        if (index + 1 >= function->body_token_count) {
            setError(ERR_DEF_VAR, currentToken.line, NULL);
            *err = 255;
            delete (proc);
            return 255;
        }
        index++;  // 指向标识符
        if (function->bodyTokens[index].type != TOK_ID) {
            setError(ERR_DEF_VAR, currentToken.line, NULL);
            *err = 255;
            delete (proc);
            return 255;
        }
        newVar.name = (wchar_t*)calloc(wcslen(function->bodyTokens[index].value) + 1, sizeof(wchar_t));
        if (!(newVar.name)) {
            *err = -1;
            delete (proc);
            return -1;
        }
        wcscpy(newVar.name, function->bodyTokens[index].value);
#ifdef HX_DEBUG
        log(L"解析到局部变量“%ls”", newVar.name);
#endif
        // 检查变量唯一性
        if (getVarIndex(newVar.name, &localeSymbolTable) != -1) {
            *err = 255;
            setError(ERR_VAR_REPEATED, currentToken.line, NULL);
            delete (proc);
            return 255;
        }
        for (int i = 0; i < outsideScopes.size(); i++) {
            if (getVarIndex(newVar.name, &outsideScopes.at(i)) != -1) {
                *err = 255;
                setError(ERR_VAR_REPEATED, currentToken.line, NULL);
                delete (proc);
                return 255;
            }
        }
        if (index + 1 >= function->body_token_count) {
            setError(ERR_DEF_VAR, currentToken.line, NULL);
            *err = 255;
            delete (proc);
            return 255;
        }
        index++;  // 指向结束标志或,
        if (function->bodyTokens[index].type == TOK_OPR_COMMA) {
            if (index + 1 >= function->body_token_count) {
                setError(ERR_DEF_VAR, currentToken.line, NULL);
                *err = 255;
                delete (proc);
                return 255;
            }
            index++;  // 指向"类型是"或"初始化"
            if (wcscmp(L"类型是", function->bodyTokens[index].value) == 0) {
                newVar.isTypeKnown = true;
                if (index + 1 >= function->body_token_count) {
                    setError(ERR_DEF_VAR, currentToken.line, NULL);
                    *err = 255;
                    delete (proc);
                    return 255;
                }
                index++;  // 指向冒号
                if (function->bodyTokens[index].type != TOK_OPR_COLON) {
                    setError(ERR_DEF_VAR, currentToken.line, NULL);
                    *err = 255;
                    delete (proc);
                    return 255;
                }
                if (index + 1 >= function->body_token_count) {
                    setError(ERR_DEF_VAR, currentToken.line, NULL);
                    *err = 255;
                    delete (proc);
                    return 255;
                }
                index++;  // 指向类型名
                if (function->bodyTokens[index].type != TOK_ID && function->bodyTokens[index].type != TOK_KW) {
                    setError(ERR_DEF_VAR, currentToken.line, NULL);
                    *err = 255;
                    delete (proc);
                    return 255;
                }
#ifdef HX_DEBUG
                log(L"解析到局部变量的类型名“%ls”", function->bodyTokens[index].value);
#endif
                // 提前越界检查，仅检查
                if (index + 1 >= function->body_token_count) {
                    setError(ERR_DEF_VAR, currentToken.line, NULL);
                    *err = 255;
                    delete (proc);
                    return 255;
                }
                if (wcscmp(function->bodyTokens[index].value, L"int") == 0 ||
                        wcscmp(function->bodyTokens[index].value, L"整型") == 0) {
                    newVar.type.kind = IR_DT_INT;
                    newVar.size = 4;
                    // int&
                    if (function->bodyTokens[index + 1].type == TOK_OPR_REFER) {
                        newVar.type.kind = IR_DT_INT_REFER;
                        index++;
                        // int[]
                    } else if (function->bodyTokens[index + 1].type == TOK_OPR_LBRACKET) {
                        if (index + 2 >= function->body_token_count) {
                            setError(ERR_TYPE, currentToken.line, NULL);
                            *err = 255;
                            delete (proc);
                            return 255;
                        }
                        if (function->bodyTokens[index + 2].type != TOK_OPR_RBRACKET) {
                            setError(ERR_TYPE, currentToken.line, NULL);
                            *err = 255;
                            delete (proc);
                            return 255;
                        }
                        newVar.type.kind = IR_DT_INT_ARR;
                        index += 2;
                    }
                } else if (wcscmp(function->bodyTokens[index].value, L"float") == 0 ||
                           wcscmp(function->bodyTokens[index].value, L"浮点型") == 0) {
                    newVar.type.kind = IR_DT_FLOAT;
                    newVar.size = 8;
                    if (function->bodyTokens[index + 1].type == TOK_OPR_REFER) {
                        newVar.type.kind = IR_DT_FLOAT_REFER;
                        newVar.size = 4;
                        index++;
                    } else if (function->bodyTokens[index + 1].type == TOK_OPR_LBRACKET) {
                        if (index + 2 >= function->body_token_count) {
                            setError(ERR_TYPE, currentToken.line, NULL);
                            *err = 255;
                            delete (proc);
                            return 255;
                        }
                        if (function->bodyTokens[index + 2].type != TOK_OPR_RBRACKET) {
                            setError(ERR_TYPE, currentToken.line, NULL);
                            *err = 255;
                            delete (proc);
                            return 255;
                        }
                        newVar.type.kind = IR_DT_FLOAT_ARR;
                        newVar.size = 4;
                        index += 2;
                    }
                } else if (wcscmp(function->bodyTokens[index].value, L"char") == 0 ||
                           wcscmp(function->bodyTokens[index].value, L"字符型") == 0) {
                    newVar.size = 2;
                    newVar.type.kind = IR_DT_CHAR;
                    if (function->bodyTokens[index + 1].type == TOK_OPR_REFER) {
                        newVar.type.kind = IR_DT_CHAR_REFER;
                        newVar.size = 4;
                        index++;
                    } else if (function->bodyTokens[index + 1].type == TOK_OPR_LBRACKET) {
                        if (index + 2 >= function->body_token_count) {
                            setError(ERR_TYPE, currentToken.line, NULL);
                            *err = 255;
                            delete (proc);
                            return 255;
                        }
                        if (function->bodyTokens[index + 2].type != TOK_OPR_RBRACKET) {
                            setError(ERR_TYPE, currentToken.line, NULL);
                            *err = 255;
                            delete (proc);
                            return 255;
                        }
                        newVar.type.kind = IR_DT_CHAR_ARR;
                        newVar.size = 4;
                        index += 2;
                    }
                } else if (wcscmp(function->bodyTokens[index].value, L"str") == 0 ||
                           wcscmp(function->bodyTokens[index].value, L"字符串型") == 0) {
                    newVar.size = 4;
                    newVar.type.kind = IR_DT_STRING;
                    if (function->bodyTokens[index + 1].type == TOK_OPR_REFER) {
                        newVar.type.kind = IR_DT_STRING_REFER;
                        index++;
                    } else if (function->bodyTokens[index + 1].type == TOK_OPR_LBRACKET) {
                        if (index + 2 >= function->body_token_count) {
                            setError(ERR_TYPE, currentToken.line, NULL);
                            *err = 255;
                            delete (proc);
                            return 255;
                        }
                        if (function->bodyTokens[index + 2].type != TOK_OPR_RBRACKET) {
                            setError(ERR_TYPE, currentToken.line, NULL);
                            *err = 255;
                            delete (proc);
                            return 255;
                        }
                        newVar.type.kind = IR_DT_STRING_ARR;
                        index += 2;
                    }
                }
                if (function->bodyTokens[index].type == TOK_ID) {
                    newVar.type.kind = IR_DT_CUSTOM;
                    if (getClassByName(function->bodyTokens[index].value, currentProgram) == NULL) {
                        setError(ERR_UNKNOWN_TYPE, function->bodyTokens[index].line, function->bodyTokens[index].value);
                        *err = 255;
                        return 255;
                    }
                    newVar.type.customTypeName =
                        (wchar_t*)calloc(wcslen(function->bodyTokens[index].value) + 1, sizeof(wchar_t));
                    if (!(newVar.type.customTypeName)) {
                        *err = -1;
                        delete (proc);
                        return -1;
                    }
                    wcscpy(newVar.type.customTypeName, function->bodyTokens[index].value);
                    if (function->bodyTokens[index + 1].type == TOK_OPR_REFER) {
                        newVar.type.kind = IR_DT_CUSTOM_REFER;
                        index++;
                    } else if (function->bodyTokens[index + 1].type == TOK_OPR_LBRACKET) {
                        if (index + 2 >= function->body_token_count) {
                            setError(ERR_TYPE, currentToken.line, NULL);
                            *err = 255;
                            delete (proc);
                            return 255;
                        }
                        if (function->bodyTokens[index + 2].type != TOK_OPR_RBRACKET) {
                            setError(ERR_TYPE, currentToken.line, NULL);
                            *err = 255;
                            delete (proc);
                            return 255;
                        }
                        newVar.type.kind = IR_DT_CUSTOM_ARR;
                        index += 2;
                    }
                }
            }
        }
        if (index + 1 >= function->body_token_count) {
            setError(ERR_DEF_VAR, currentToken.line, NULL);
            *err = 255;
            delete (proc);
            return 255;
        }
        index++;

        bool isInit = false;
        Instruction initInst = {};  // 初始化的指令
        int initInstBegin = proc->instructions.size();
        if (function->bodyTokens[index].type == TOK_OPR_SET) {
            isInit = true;
            if (newVar.isTypeKnown) {  //  <=> 已显示声明类型
            }
            newVar.isTypeKnown = true;
        }
        // 若为数组，需生成分配堆内存的指令
        if (newVar.isTypeKnown && newVar.type.arrayLength != 0) {
            // 分配堆内存的指令应在初始化指令之前
            if (newVar.type.kind == IR_DT_CHAR_ARR || newVar.type.kind == IR_DT_CUSTOM_ARR ||
                    newVar.type.kind == IR_DT_FLOAT_ARR || newVar.type.kind == IR_DT_INT_ARR ||
                    newVar.type.kind == IR_DT_STRING_ARR) {  // alloc + store ptr + store length
                int allocInstIndex = 0;
                if (isInit) {
                    proc->instructions.insert(proc->instructions.begin() + initInstBegin, 4, Instruction{OP_NOP});
                    allocInstIndex = initInstBegin;
                } else {
                    proc->instructions.resize(proc->instructions.size() + 4);
                    if (proc->instructions.size() >= 4) allocInstIndex = proc->instructions.size() - 4;
                }
                // 分配堆内存并将地址压栈
                Instruction& allocInst = proc->instructions.at(allocInstIndex);
                allocInst.opcode = OP_HEAP_ALLOC;
                uint32_t size = newVar.type.arrayLength;
                if (newVar.type.kind == IR_DT_CHAR_ARR) {
                    size = size * sizeof(uint16_t);
                } else if (newVar.type.kind == IR_DT_CUSTOM_ARR) {
                    size = size * 4;
                } else if (newVar.type.kind == IR_DT_FLOAT_ARR) {
                    size = size * sizeof(double);
                } else if (newVar.type.kind == IR_DT_INT_ARR) {
                    size = size * sizeof(int32_t);
                } else if (newVar.type.kind == IR_DT_STRING_ARR) {
                    size = size * 4;
                }
                memcpy(allocInst.params[0].value, &size, sizeof(uint32_t));

                Instruction& movAddrInst = proc->instructions.at(allocInstIndex + 1);
                movAddrInst.opcode = OP_STORE_VAR;
                movAddrInst.params[0].type = PARAM_TYPE_OFFEST;
                movAddrInst.params[1].type = PARAM_TYPE_SIZE;

                Instruction& pushLenInst = proc->instructions.at(allocInstIndex + 2);
                pushLenInst.opcode = OP_LOAD_CONST;
                pushLenInst.params[0].type = PARAM_TYPE_INT;
                pushLenInst.params[1];

                Instruction& movLenInst = proc->instructions.at(allocInstIndex + 3);
                movLenInst.opcode = OP_STORE_VAR;
                movLenInst.params[0].type = PARAM_TYPE_OFFEST;
                movLenInst.params[1].type = PARAM_TYPE_SIZE;
                movLenInst.params[0].size = sizeof(uint32_t);
                movLenInst.params[1].size = sizeof(uint32_t);
                movLenInst.params[0].offest = 8;  // 数组长度存储在变量的偏移量+8处
            }
        }
        // 关联指令
        if (initInstBegin < proc->instructions.size()) {
            for (int i = initInstBegin; i < proc->instructions.size(); i++) {
                newVar.instIndex.push_back(i);
            }
        }

        if (function->bodyTokens[index].type == TOK_END) {
        } else {
            setError(ERR_DEF_VAR, currentToken.line, NULL);
            *err = 255;
            delete (proc);
            return 255;
        }
#ifdef HX_DEBUG
        log(L"将%ls推入局部符号表", newVar.name);
#endif
        localeSymbolTable.vars.push_back(newVar);
        uint32_t varSize = (uint32_t)getVarSize(newVar.type, currentProgram->classes);
        // if (stackSize + varSize > stackSize) stackSize += varSize;
        if (localVarSize + 1 > localVarSize) localVarSize++;

    } else if (wcscmp(currentToken.value, L"repeat") == 0) {  // 循环 ::= repeat-> 语句|块 [until:exp]
#ifdef HX_DEBUG
        log(L"循环");
#endif
        if (index + 1 >= function->body_token_count) {
            setError(ERR_REPEAT, currentToken.line, NULL);
            *err = 255;
            delete (proc);
            return 255;
        }
        index++;  // 指向->
        if (function->bodyTokens[index].type != TOK_OPR_POINT) {
            setError(ERR_REPEAT, currentToken.line, NULL);
            *err = 255;
            delete (proc);
            return 255;
        }
        if (index + 1 >= function->body_token_count) {
            setError(ERR_REPEAT, currentToken.line, NULL);
            *err = 255;
            delete (proc);
            return 255;
        }
        index++;  // 指向语句或块
        uint32_t jmpAddr = proc->instructions.size();
        generateClassFunctionStatement(index, pitchTable, constantPool, classIndex, globalFunctionCount, allFunctions, currentProgram, allFunctionCount, function,
                              outsideScopes.back(), outsideScopes, localeScopeIndex, proc, procIndex, stackSize,
                              localVarSize, err);
        if(*err != 0) {
            delete (proc);
            return *err;
        }
        if (index + 1 >= function->body_token_count) {
            setError(ERR_REPEAT, currentToken.line, NULL);
            *err = 255;
            delete (proc);
            return 255;
        }
        index++;  // 可能指向until
        // 条件
        if (wcscmp(function->bodyTokens[index].value, L"until") == 0) {
#ifdef HX_DEBUG
            log(L"带条件的循环");
#endif
            if (index + 1 >= function->body_token_count) {
                setError(ERR_REPEAT, currentToken.line, NULL);
                *err = 255;
                delete (proc);
                return 255;
            }
            index++;  // 指向冒号
            if (function->bodyTokens[index].type != TOK_OPR_COLON) {
                setError(ERR_IF, currentToken.line, NULL);
                *err = 255;
                delete (proc);
                return 255;
            }
            if (index + 1 >= function->body_token_count) {
                setError(ERR_REPEAT, currentToken.line, NULL);
                *err = 255;
                delete (proc);
                return 255;
            }
            index++;  // 指向表达式
            int expIndex = index;
            while (index + 1 < function->body_token_count) {
                index++;
                if (function->bodyTokens[index].type == TOK_END) break;
            }
            ASTNode* expNode = parseExpression(function->bodyTokens, &expIndex, index, pitchTable, &localeSymbolTable,
                                               outsideScopes, localeScopeIndex, err);
            if (*err != 0 || !expNode) {
                delete (proc);
                return 255;
            }
            int inst_index = proc->instructions.size();
            int inst_size = proc->instructions.size();
            generateInstructionsFromAST(proc->instructions, &inst_index, &inst_size, expNode, constantPool, outsideScopes,
                                        procIndex, err);
            if (*err != 0) {
                freeAST(expNode);
                return 255;
            }
            freeAST(expNode);
            // 不满足条件才跳转
            Instruction notInst = {};
            notInst.opcode = OP_NOT_LOGIC;
            proc->instructions.push_back(notInst);

            Instruction conditionJmpInst = {};
            conditionJmpInst.opcode = OP_JMP_CONDITION;
            conditionJmpInst.params[0].size = sizeof(uint32_t);
            memcpy(conditionJmpInst.params[0].value, &jmpAddr, conditionJmpInst.params[0].size);
            conditionJmpInst.params[0].type = PARAM_TYPE_INDEX;
            uint32_t falseJmpAddr = proc->instructions.size() + 1;
            conditionJmpInst.params[1].size = sizeof(uint32_t);
            memcpy(conditionJmpInst.params[1].value, &falseJmpAddr, conditionJmpInst.params[1].size);
            conditionJmpInst.params[1].type = PARAM_TYPE_INDEX;
            proc->instructions.push_back(conditionJmpInst);

            Instruction falseInst = {};
            falseInst.opcode = OP_NOP;
            proc->instructions.push_back(falseInst);
        } else {
            Instruction jmpInst = {};
            jmpInst.opcode = OP_JMP;
            jmpInst.params[0].size = sizeof(uint32_t);
            memcpy(jmpInst.params[0].value, &jmpAddr, jmpInst.params[0].size);
            jmpInst.params[0].type = PARAM_TYPE_INDEX;
            proc->instructions.push_back(jmpInst);
        }
    } else if (wcscmp(currentToken.value, L"循环") == 0) {  // 循环::= 循环->语句|块 直到:exp
#ifdef HX_DEBUG
        log(L"循环");
#endif
        if (index + 1 >= function->body_token_count) {
            setError(ERR_REPEAT, currentToken.line, NULL);
            *err = 255;
            delete (proc);
            return 255;
        }
        index++;  // 指向->
        if (function->bodyTokens[index].type != TOK_OPR_POINT) {
            setError(ERR_REPEAT, currentToken.line, NULL);
            *err = 255;
            delete (proc);
            return 255;
        }
        if (index + 1 >= function->body_token_count) {
            setError(ERR_REPEAT, currentToken.line, NULL);
            *err = 255;
            delete (proc);
            return 255;
        }
        index++;  // 指向语句或块
        uint32_t jmpAddr = proc->instructions.size();
        generateClassFunctionStatement(index, pitchTable, constantPool, classIndex, globalFunctionCount, allFunctions, currentProgram, allFunctionCount, function,
                              outsideScopes.back(), outsideScopes, localeScopeIndex, proc, procIndex, stackSize,
                              localVarSize, err);
        if(*err != 0) {
            delete (proc);
            return *err;
        }
        if (index + 1 >= function->body_token_count) {
            setError(ERR_REPEAT, currentToken.line, NULL);
            *err = 255;
            delete (proc);
            return 255;
        }
        index++;
        // 条件
        if (wcscmp(function->bodyTokens[index].value, L"直到") == 0) {
#ifdef HX_DEBUG
            log(L"带条件的循环");
#endif
            if (index + 1 >= function->body_token_count) {
                setError(ERR_REPEAT, currentToken.line, NULL);
                *err = 255;
                delete (proc);
                return 255;
            }
            index++;  // 指向冒号
            if (function->bodyTokens[index].type != TOK_OPR_COLON) {
                setError(ERR_IF, currentToken.line, NULL);
                *err = 255;
                delete (proc);
                return 255;
            }
            if (index + 1 >= function->body_token_count) {
                setError(ERR_REPEAT, currentToken.line, NULL);
                *err = 255;
                delete (proc);
                return 255;
            }
            index++;  // 指向表达式
            int expIndex = index;
            while (index + 1 < function->body_token_count) {
                index++;
                if (function->bodyTokens[index].type == TOK_END) break;
            }
            ASTNode* expNode = parseExpression(function->bodyTokens, &expIndex, index, pitchTable, &localeSymbolTable,
                                               outsideScopes, localeScopeIndex, err);
            if (*err != 0 || !expNode) {
                delete (proc);
                return 255;
            }
            int inst_index = proc->instructions.size();
            int inst_size = proc->instructions.size();
            generateInstructionsFromAST(proc->instructions, &inst_index, &inst_size, expNode, constantPool, outsideScopes,
                                        procIndex, err);
            if (*err != 0) {
                freeAST(expNode);
                return 255;
            }
            freeAST(expNode);

            // 不满足条件才跳转
            Instruction notInst = {};
            notInst.opcode = OP_NOT_LOGIC;
            proc->instructions.push_back(notInst);

            Instruction conditionJmpInst = {};
            conditionJmpInst.opcode = OP_JMP_CONDITION;
            conditionJmpInst.params[0].size = sizeof(uint32_t);
            memcpy(conditionJmpInst.params[0].value, &jmpAddr, conditionJmpInst.params[0].size);
            conditionJmpInst.params[0].type = PARAM_TYPE_INDEX;
            uint32_t falseJmpAddr = proc->instructions.size() + 1;
            conditionJmpInst.params[1].size = sizeof(uint32_t);
            memcpy(conditionJmpInst.params[1].value, &falseJmpAddr, conditionJmpInst.params[1].size);
            conditionJmpInst.params[1].type = PARAM_TYPE_INDEX;
            proc->instructions.push_back(conditionJmpInst);

            Instruction falseInst = {};
            falseInst.opcode = OP_NOP;
            proc->instructions.push_back(falseInst);
        } else {
            Instruction jmpInst = {};
            jmpInst.opcode = OP_JMP;
            jmpInst.params[0].size = sizeof(uint32_t);
            memcpy(jmpInst.params[0].value, &jmpAddr, jmpInst.params[0].size);
            jmpInst.params[0].type = PARAM_TYPE_INDEX;
            proc->instructions.push_back(jmpInst);
        }
        // 条件判断
    } else if (wcscmp(currentToken.value, L"if") == 0 ||
               wcscmp(currentToken.value, L"若") == 0) {  // ::= if|如果: <exp> -> <block|statement>
        if (index + 1 >= function->body_token_count) {
            setError(ERR_IF, currentToken.line, NULL);
            *err = 255;
            delete (proc);
            return 255;
        }
        index++;  // 指向冒号
        if (function->bodyTokens[index].type != TOK_OPR_COLON) {
            setError(ERR_IF, currentToken.line, NULL);
            *err = 255;
            delete (proc);
            return 255;
        }
        if (index + 1 >= function->body_token_count) {
            setError(ERR_IF, currentToken.line, NULL);
            *err = 255;
            delete (proc);
            return 255;
        }
        index++;  // 指向条件的表达式
        // 找到箭头位置作为向parseExpression提供的末尾
        int expIndex = index;
        while (index + 1 < function->body_token_count) {
            index++;
            if (function->bodyTokens[index].type == TOK_OPR_POINT) break;
        }
        // 分析表达式
        ASTNode* expNode = parseExpression(function->bodyTokens, &expIndex, index, pitchTable, &localeSymbolTable,
                                           outsideScopes, localeScopeIndex, err);
        if (*err != 0 || !expNode) {
            delete (proc);
            return 255;
        }
        int inst_index = proc->instructions.size();
        int inst_size = proc->instructions.size();
        generateInstructionsFromAST(proc->instructions, &inst_index, &inst_size, expNode, constantPool, outsideScopes,
                                    procIndex, err);
        if (*err != 0) {
            freeAST(expNode);
            return 255;
        }
        freeAST(expNode);
        // 分析语句
        if (function->bodyTokens[index].type != TOK_OPR_POINT) {
            setError(ERR_IF, currentToken.line, NULL);
            *err = 255;
            delete (proc);
            return 255;
        }
        if (index + 1 >= function->body_token_count) {
            setError(ERR_IF, currentToken.line, NULL);
            *err = 255;
            delete (proc);
            return 255;
        }
        index++;  // 指向语句或块

        // 生成OP_JMP_CONDITION指令,  ########后面要把第二个参数补回来#########
        // 跳转地址应指向JMP后的一个指令
        uint32_t jmpAddr = (uint32_t)(proc->instructions.size() + 1);
        Instruction jmpInst = {};
        jmpInst.opcode = OP_JMP_CONDITION;
        jmpInst.params[0].type = PARAM_TYPE_INDEX;
        memcpy(jmpInst.params[0].value, &jmpAddr, sizeof(uint32_t));
        proc->instructions.push_back(jmpInst);
        int jmpInstIndex = proc->instructions.size() - 1;
        // 生成块或语句的目标代码
        generateClassFunctionStatement(index, pitchTable, constantPool, classIndex, globalFunctionCount, allFunctions, currentProgram, allFunctionCount, function,
                              outsideScopes.back(), outsideScopes, localeScopeIndex, proc, procIndex, stackSize,
                              localVarSize, err);
        if(*err != 0) {
            delete (proc);
            return *err;
        }

        if (jmpAddr == (uint32_t)(proc->instructions.size())) {  // 用户写了空语句，那就塞个NOP
            Instruction nopInst = {};
            proc->instructions.push_back(nopInst);
        }
        bool hasNextElse =
            (index + 1 < function->body_token_count && (wcscmp(function->bodyTokens[index + 1].value, L"else") == 0 ||
                    wcscmp(function->bodyTokens[index + 1].value, L"否则") == 0));
        std::vector<int> endOfIfBlock;
        if (hasNextElse) {
            Instruction ifBodyJmp = {};
            ifBodyJmp.opcode = OP_JMP;
            proc->instructions.push_back(ifBodyJmp);
            endOfIfBlock.push_back(proc->instructions.size() - 1);
        }
        proc->instructions.at(jmpInstIndex).params[1].type = PARAM_TYPE_INDEX;
        uint32_t falseJmpAddr = (uint32_t)proc->instructions.size();

        Instruction nopInst = {};  // 塞NOP防越界
        nopInst.opcode = OP_NOP;
        proc->instructions.push_back(nopInst);
        memcpy(proc->instructions.at(jmpInstIndex).params[1].value, &falseJmpAddr, sizeof(uint32_t));
        while (index + 1 < function->body_token_count && (wcscmp(function->bodyTokens[index + 1].value, L"else") == 0 ||
                wcscmp(function->bodyTokens[index + 1].value, L"否则") == 0)) {
            index++;
            if (index + 1 >= function->body_token_count) {
                setError(ERR_IF, currentToken.line, NULL);
                *err = 255;
                delete (proc);
                return 255;
            }
            (index)++;  // 挪到 ->
            if (function->bodyTokens[index].type != TOK_OPR_POINT) {
                setError(ERR_IF, currentToken.line, NULL);
                *err = 255;
                delete (proc);
                return 255;
            }
            if (index + 1 >= function->body_token_count) {
                setError(ERR_IF, currentToken.line, NULL);
                *err = 255;
                delete (proc);
                return 255;
            }
            (index)++;  // 挪到语句或块

            generateClassFunctionStatement(index, pitchTable, constantPool, classIndex, globalFunctionCount, allFunctions, currentProgram, allFunctionCount, function,
                              outsideScopes.back(), outsideScopes, localeScopeIndex, proc, procIndex, stackSize,
                              localVarSize, err);
        if(*err != 0) {
            delete (proc);
            return *err;
        }
            hasNextElse =
                (index + 1 < function->body_token_count && (wcscmp(function->bodyTokens[index + 1].value, L"else") == 0 ||
                        wcscmp(function->bodyTokens[index + 1].value, L"否则") == 0));
            if (hasNextElse) {
                Instruction gotoInst = {};
                gotoInst.opcode = OP_JMP;
                proc->instructions.push_back(gotoInst);
                endOfIfBlock.push_back(proc->instructions.size() - 1);
            }
        }
        uint32_t endInstAddr = proc->instructions.size();
        proc->instructions.push_back(nopInst);

        for (int i = 0; i < endOfIfBlock.size(); i++) {
            int jumpInstIdx = endOfIfBlock.at(i);
            proc->instructions.at(jumpInstIdx).params[0].type = PARAM_TYPE_INDEX;
            proc->instructions.at(jumpInstIdx).params[0].size = (uint8_t)sizeof(uint32_t);
            memcpy(proc->instructions.at(jumpInstIdx).params[0].value, &endInstAddr, sizeof(uint32_t));
        }
    }
    if (wcscmp(currentToken.value, L"for") == 0) {  // for: tmp:arr -> {}
#ifdef HX_DEBUG
        log(L"for循环");
#endif
        if (index + 1 >= function->body_token_count) {
            setError(ERR_IF, currentToken.line, NULL);
            *err = 255;
            delete (proc);
            return 255;
        }
        (index)++;
        if (function->bodyTokens[index].type != TOK_OPR_COLON) {
            setError(ERR_FOR, currentToken.line, NULL);
            *err = 255;
            delete (proc);
            return 255;
        }
        if (index + 1 >= function->body_token_count) {
            setError(ERR_FOR, currentToken.line, NULL);
            *err = 255;
            delete (proc);
            return 255;
        }
        (index)++;  // id(tmp)
        if (function->bodyTokens[index].type != TOK_ID) {
            setError(ERR_FOR, currentToken.line, NULL);
            *err = 255;
            delete (proc);
            return 255;
        }
        Symbol tmpVar = {};  // for循环用户使用的中间变量
        tmpVar.name = (wchar_t*)calloc(wcslen(function->bodyTokens[index].value) + 1, sizeof(wchar_t));
        if (!tmpVar.name) {
            *err = -1;
            delete (proc);
            return -1;
        }
        wcscpy(tmpVar.name, function->bodyTokens[index].value);
        // 检查变量唯一性
        if (getVarIndex(tmpVar.name, &localeSymbolTable) != -1) {
            *err = 255;
            setError(ERR_VAR_REPEATED, currentToken.line, NULL);
            delete (proc);
            return 255;
        }
        tmpVar.procIndex = procIndex;

        if (index + 1 >= function->body_token_count) {
            setError(ERR_FOR, currentToken.line, NULL);
            *err = 255;
            delete (proc);
            return 255;
        }
        (index)++;  // 冒号
        if (function->bodyTokens[index].type != TOK_OPR_COLON) {
            setError(ERR_FOR, currentToken.line, NULL);
            *err = 255;
            delete (proc);
            return 255;
        }
        if (index + 1 >= function->body_token_count) {
            setError(ERR_FOR, currentToken.line, NULL);
            *err = 255;
            delete (proc);
            return 255;
        }
        index++;  // 数组
        if (function->bodyTokens[index].type != TOK_ID) {
            setError(ERR_FOR, currentToken.line, NULL);
            *err = 255;
            delete (proc);
            return 255;
        }
        int arrScopeIdx = -1;
        int arrVarIdx = -1;
        for (int i = localeScopeIndex; i >= 0; i--) {
            int arrIndex = getVarIndex(function->bodyTokens[index].value, &outsideScopes.at(i));
            if (arrIndex != -1) {
                arrScopeIdx = i;
                arrVarIdx = arrIndex;
                break;
            }
        }
        if (arrScopeIdx == -1) {
            setError(ERR_NO_VAR, function->bodyTokens[index].line, function->bodyTokens[index].value);
            *err = 255;
            delete (proc);
            return 255;
        }
        Symbol arrCopy = outsideScopes.at(arrScopeIdx).vars.at(arrVarIdx);
        Symbol* arr = &arrCopy;
        // 分析tmp的类型
        switch (arr->type.kind) {
        case IR_DT_INT_ARR:
            tmpVar.type.kind = IR_DT_INT;
            tmpVar.size = 4;
            break;
        case IR_DT_FLOAT_ARR:
            tmpVar.type.kind = IR_DT_FLOAT;
            tmpVar.size = 8;
            break;
        case IR_DT_CHAR_ARR:
            tmpVar.type.kind = IR_DT_CHAR;
            tmpVar.size = 2;
            break;
        case IR_DT_STRING_ARR:
            tmpVar.type.kind = IR_DT_STRING;
            tmpVar.size = 8;
            break;
        case IR_DT_CUSTOM_ARR:
            tmpVar.type.kind = IR_DT_CUSTOM;
            tmpVar.size = 8;
            tmpVar.type.customTypeName = (wchar_t*)calloc(wcslen(arr->type.customTypeName) + 1, sizeof(wchar_t));
            if (!tmpVar.type.customTypeName) {
                *err = -1;
                delete (proc);
                return -1;
            }
            wcscpy(tmpVar.type.customTypeName, arr->type.customTypeName);
            break;
        default:
            setError(ERR_FOR, function->bodyTokens[index].line, NULL);
            *err = 255;
            delete (proc);
            return 255;
        }
        tmpVar.isUsed = true;

        Symbol tmpIndexVar;  // 用于存索引
        tmpIndexVar.size = sizeof(uint32_t);
        tmpIndexVar.type = {IR_DT_INT};
        tmpIndexVar.procIndex = procIndex;
        tmpIndexVar.name = nullptr;
        tmpIndexVar.isUsed = true;  // 循环变量绝对被使用了
        outsideScopes.at(localeScopeIndex).vars.push_back(tmpIndexVar);
        int indexOfTmpIndexVar = (int)outsideScopes.at(localeScopeIndex).vars.size() - 1;  // 记录真正的循环变量索引
        if (indexOfTmpIndexVar < 0 || indexOfTmpIndexVar >= (int)localeSymbolTable.vars.size()) {
            setError(ERR_FOR, currentToken.line, NULL);
            *err = 255;
            delete (proc);
            return 255;
        }
        outsideScopes.at(arrScopeIdx).vars.at(arrVarIdx).isUsed = true;
        outsideScopes.at(localeScopeIndex).vars.at(indexOfTmpIndexVar).isUsed = true;
        // 初始化数组索引
#ifdef HX_DEBUG
        log(L"初始化数组索引");
#endif
        Instruction initIndexInst = {};
        initIndexInst.opcode = OP_LOAD_CONST;
        initIndexInst.params[0].type = PARAM_TYPE_INT;
        uint32_t initIndex = 0;
        memcpy(initIndexInst.params[1].value, &initIndex, sizeof(uint32_t));
        initIndexInst.params[1].size = sizeof(uint32_t);
        proc->instructions.push_back(initIndexInst);
        outsideScopes.at(localeScopeIndex).vars.at(indexOfTmpIndexVar).instIndex.push_back(proc->instructions.size() - 1);

        Instruction storeIndexInst = {};
        storeIndexInst.opcode = OP_STORE_VAR;
        storeIndexInst.params[0].type = PARAM_TYPE_OFFEST;
        uint32_t indexOffset = (uint32_t)(localeSymbolTable.vars.size() - 1);
        memcpy(storeIndexInst.params[0].value, &indexOffset, sizeof(uint32_t));
        storeIndexInst.params[0].size = sizeof(uint32_t);
        storeIndexInst.params[1].type = PARAM_TYPE_INT;
        *(uint32_t*)storeIndexInst.params[1].value = sizeof(uint32_t);
        storeIndexInst.params[1].size = sizeof(uint32_t);
        if (indexOfTmpIndexVar < 0 || indexOfTmpIndexVar >= (int)localeSymbolTable.vars.size()) {
            setError(ERR_FOR, currentToken.line, NULL);
            *err = 255;
            delete (proc);
            return 255;
        }
        localeSymbolTable.vars[indexOfTmpIndexVar].instIndex.push_back(proc->instructions.size());
        proc->instructions.push_back(storeIndexInst);
        outsideScopes.at(localeScopeIndex).vars.at(indexOfTmpIndexVar).instIndex.push_back(proc->instructions.size() - 1);
        // 初始化结束
        // 每次循环后跳转地址
        uint32_t jmpAddr = proc->instructions.size();
        // 正式循环
        // 1.加载索引
#ifdef HX_DEBUG
        log(L"加载索引");
#endif
        Instruction loadIndexInst = {};
        loadIndexInst.opcode = OP_LOAD_VAR;
        loadIndexInst.params[0].type = PARAM_TYPE_OFFEST;
        loadIndexInst.params[0].size = sizeof(uint32_t);
        loadIndexInst.params[1].type = PARAM_TYPE_INT;
        loadIndexInst.params[1].size = sizeof(uint32_t);
        memcpy(loadIndexInst.params[0].value, &indexOffset, sizeof(uint32_t));
        if (indexOfTmpIndexVar < 0 || indexOfTmpIndexVar >= (int)localeSymbolTable.vars.size()) {
            setError(ERR_FOR, currentToken.line, NULL);
            *err = 255;
            delete (proc);
            return 255;
        }
        proc->instructions.push_back(loadIndexInst);
        outsideScopes.at(localeScopeIndex).vars.at(indexOfTmpIndexVar).instIndex.push_back(proc->instructions.size() - 1);
        // 2.加载数组元素
#ifdef HX_DEBUG
        log(L"加载数组元素");
#endif
        Instruction loadElementInst = {};
        loadElementInst.opcode = OP_LOAD_ELEMENT_FROM_ARRAY;
        loadElementInst.params[0].type = PARAM_TYPE_OFFEST;
        loadElementInst.params[0].size = sizeof(uint32_t);

        uint32_t arrIndexVal = (uint32_t)arrVarIdx;
        memcpy(loadElementInst.params[0].value, &arrIndexVal, sizeof(uint32_t));

        switch (outsideScopes.at(arrScopeIdx).vars.at(arrVarIdx).type.kind) {
        case IR_DT_INT_ARR:
            loadElementInst.params[1].type = PARAM_TYPE_INT;
            loadElementInst.params[1].size = sizeof(uint32_t);
            break;
        case IR_DT_FLOAT_ARR:
            loadElementInst.params[1].type = PARAM_TYPE_FLOAT;
            loadElementInst.params[1].size = sizeof(uint32_t);
            break;
        case IR_DT_CHAR_ARR:
            loadElementInst.params[1].type = PARAM_TYPE_CHAR;
            loadElementInst.params[1].size = sizeof(uint32_t);
            break;
        case IR_DT_STRING_ARR:
            loadElementInst.params[1].type = PARAM_TYPE_STRING;
            loadElementInst.params[1].size = sizeof(uint32_t);
            break;
        case IR_DT_CUSTOM_ARR:
            loadElementInst.params[1].type = PARAM_TYPE_ADDRESS;
            loadElementInst.params[1].size = sizeof(uint32_t);
            break;
        }
        outsideScopes.at(arrScopeIdx).vars.at(arrVarIdx).instIndex.push_back(proc->instructions.size());
        proc->instructions.push_back(loadElementInst);
        // 3.存入tmp
#ifdef HX_DEBUG
        log(L"存入tmp");
#endif
        Instruction storeTmpInst = {};
        storeTmpInst.opcode = OP_STORE_VAR;
        storeTmpInst.params[0].type = PARAM_TYPE_OFFEST;
        storeTmpInst.params[0].size = sizeof(uint32_t);
        storeTmpInst.params[1].type = loadElementInst.params[1].type;
        storeTmpInst.params[1].size = loadElementInst.params[1].size;

        tmpVar.instIndex.push_back(proc->instructions.size());
        proc->instructions.push_back(storeTmpInst);
        // 4.执行循环体
#ifdef HX_DEBUG
        log(L"执行循环体");
#endif
        if (index + 1 >= function->body_token_count) {
            setError(ERR_FOR, currentToken.line, NULL);
            *err = 255;
            delete (proc);
            return 255;
        }
        (index)++;  // 指向->
        if (function->bodyTokens[index].type != TOK_OPR_POINT) {
            setError(ERR_FOR, currentToken.line, NULL);
            *err = 255;
            delete (proc);
            return 255;
        }
        if (index + 1 >= function->body_token_count) {
            setError(ERR_FOR, currentToken.line, NULL);
            *err = 255;
            delete (proc);
            return 255;
        }
        (index)++;
        generateClassFunctionStatement(index, pitchTable, constantPool, classIndex, globalFunctionCount, allFunctions, currentProgram, allFunctionCount, function,
                              outsideScopes.back(), outsideScopes, localeScopeIndex, proc, procIndex, stackSize,
                              localVarSize, err);
        outsideScopes.back().vars.push_back(tmpVar);
        if (*err != 0) {
            delete (proc);
            return 255;
        }
        // 5.索引加1
#ifdef HX_DEBUG
        log(L"索引自增");
#endif
        Instruction incIndexInst = {};
        incIndexInst.opcode = OP_INC;
        incIndexInst.params[0].type = PARAM_TYPE_OFFEST;
        incIndexInst.params[0].size = sizeof(uint32_t);
        incIndexInst.params[1].type = PARAM_TYPE_INT;
        if (indexOfTmpIndexVar < 0 || indexOfTmpIndexVar >= (int)outsideScopes.at(localeScopeIndex).vars.size()) {
            setError(ERR_FOR, currentToken.line, NULL);
            *err = 255;
            delete (proc);
            return 255;
        }
        outsideScopes.at(localeScopeIndex).vars[indexOfTmpIndexVar].instIndex.push_back(proc->instructions.size());
        proc->instructions.push_back(incIndexInst);
        // 6.判断索引是否小于数组长度
#ifdef HX_DEBUG
        log(L"判断索引是否小于数组长度");
#endif
        Instruction loadIndexInst2 = {};
        loadIndexInst2.opcode = OP_LOAD_VAR;
        loadIndexInst2.params[0].type = PARAM_TYPE_OFFEST;
        loadIndexInst2.params[0].size = sizeof(uint32_t);
        loadIndexInst2.params[1].type = PARAM_TYPE_INT;
        loadIndexInst2.params[1].size = sizeof(uint32_t);
        *(uint32_t*)loadIndexInst2.params[1].value = sizeof(uint32_t);
        uint32_t tmpIndexVal = (uint32_t)indexOfTmpIndexVar;
        memcpy(loadIndexInst2.params[0].value, &tmpIndexVal, sizeof(uint32_t));
        if (indexOfTmpIndexVar < 0 || indexOfTmpIndexVar >= (int)outsideScopes.at(localeScopeIndex).vars.size()) {
            setError(ERR_FOR, currentToken.line, NULL);
            *err = 255;
            delete (proc);
            return 255;
        }

        outsideScopes.at(localeScopeIndex).vars[indexOfTmpIndexVar].instIndex.push_back(proc->instructions.size());
        proc->instructions.push_back(loadIndexInst2);
        Instruction loadArrSizeInst = {};
        loadArrSizeInst.opcode = OP_LOAD_VAR;
        // 读取：数组在栈中偏移量+8， uint32_t类型的数组长度
        loadArrSizeInst.params[0].type = PARAM_TYPE_OFFEST;
        loadArrSizeInst.params[0].size = sizeof(uint32_t);
        loadArrSizeInst.params[1].type = PARAM_TYPE_INT;
        loadArrSizeInst.params[1].size = sizeof(uint32_t);
        loadArrSizeInst.params[0].offest = 8;
        *(uint32_t*)loadArrSizeInst.params[1].value = sizeof(uint32_t);
        uint32_t arrIndexVal2 = (uint32_t)arrVarIdx;
        memcpy(loadArrSizeInst.params[0].value, &arrIndexVal2, sizeof(uint32_t));

        outsideScopes.at(arrScopeIdx).vars.at(arrVarIdx).instIndex.push_back(proc->instructions.size());
        proc->instructions.push_back(loadArrSizeInst);
        Instruction cmpInst = {};
        cmpInst.opcode = OP_LT;
        outsideScopes.at(arrScopeIdx).vars.at(arrVarIdx).instIndex.push_back(proc->instructions.size());
        proc->instructions.push_back(cmpInst);
        // 7.条件跳转
        Instruction jmpInst = {};
        jmpInst.opcode = OP_JMP_CONDITION;
        jmpInst.params[0].type = PARAM_TYPE_INDEX;
        memcpy(jmpInst.params[0].value, &jmpAddr, sizeof(uint32_t));
        jmpInst.params[0].size = sizeof(uint32_t);
        uint32_t falseJmpAddr = proc->instructions.size() + 1;
        jmpInst.params[1].type = PARAM_TYPE_INDEX;
        jmpInst.params[1].size = sizeof(uint32_t);
        memcpy(jmpInst.params[1].value, &falseJmpAddr, sizeof(uint32_t));
        proc->instructions.push_back(jmpInst);
        Instruction nopInst = {};
        nopInst.opcode = OP_NOP;
        proc->instructions.push_back(nopInst);
    } else if (wcscmp(currentToken.value, L"遍历") == 0) {  // 遍历:arr，中间变量：tmp -> {}
#ifdef HX_DEBUG
        log(L"for循环");
#endif
        if (index + 1 >= function->body_token_count) {
            setError(ERR_IF, currentToken.line, NULL);
            *err = 255;
            delete (proc);
            return 255;
        }
        (index)++;
        if (function->bodyTokens[index].type != TOK_OPR_COLON) {
            setError(ERR_FOR, currentToken.line, NULL);
            *err = 255;
            delete (proc);
            return 255;
        }
        if (index + 1 >= function->body_token_count) {
            setError(ERR_FOR, currentToken.line, NULL);
            *err = 255;
            delete (proc);
            return 255;
        }
        (index)++;  // id(arr)
        if (function->bodyTokens[index].type != TOK_ID) {
            setError(ERR_FOR, currentToken.line, NULL);
            *err = 255;
            delete (proc);
            return 255;
        }

        int arrScopeIdx = -1;
        int arrVarIdx = -1;
        for (int i = localeScopeIndex; i >= 0; i--) {
            int arrIndex = getVarIndex(function->bodyTokens[index].value, &outsideScopes.at(i));
            if (arrIndex != -1) {
                arrScopeIdx = i;
                arrVarIdx = arrIndex;
                break;
            }
        }
        if (arrScopeIdx == -1) {
            setError(ERR_NO_VAR, function->bodyTokens[index].line, function->bodyTokens[index].value);
            *err = 255;
            delete (proc);
            return 255;
        }

        if (index + 1 >= function->body_token_count) {
            setError(ERR_FOR, currentToken.line, NULL);
            *err = 255;
            delete (proc);
            return 255;
        }
        (index)++;  // 逗号
        if (function->bodyTokens[index].type != TOK_OPR_COMMA) {
            setError(ERR_FOR, currentToken.line, NULL);
            *err = 255;
            delete (proc);
            return 255;
        }
        if (index + 1 >= function->body_token_count) {
            setError(ERR_FOR, currentToken.line, NULL);
            *err = 255;
            delete (proc);
            return 255;
        }
        index++;  // 关键字，中间变量
        if (function->bodyTokens[index].type != TOK_KW || wcscmp(function->bodyTokens[index].value, L"中间变量") != 0) {
            setError(ERR_FOR, currentToken.line, NULL);
            *err = 255;
            delete (proc);
            return 255;
        }
        if (index + 1 >= function->body_token_count) {
            setError(ERR_FOR, currentToken.line, NULL);
            *err = 255;
            delete (proc);
            return 255;
        }
        index++;  // 冒号
        if (function->bodyTokens[index].type != TOK_OPR_COLON) {
            setError(ERR_FOR, currentToken.line, NULL);
            *err = 255;
            delete (proc);
            return 255;
        }
        if (index + 1 >= function->body_token_count) {
            setError(ERR_FOR, currentToken.line, NULL);
            *err = 255;
            delete (proc);
            return 255;
        }
        index++;  // userTmpVar
        if (function->bodyTokens[index].type != TOK_ID) {
            setError(ERR_FOR, currentToken.line, NULL);
            *err = 255;
            delete (proc);
            return 255;
        }
        Symbol tmpVar = {};  // for循环用户使用的中间变量
        tmpVar.name = (wchar_t*)calloc(wcslen(function->bodyTokens[index].value) + 1, sizeof(wchar_t));
        if (!tmpVar.name) {
            *err = -1;
            delete (proc);
            return -1;
        }
        wcscpy(tmpVar.name, function->bodyTokens[index].value);
        // 检查变量唯一性
        if (getVarIndex(tmpVar.name, &localeSymbolTable) != -1) {
            *err = 255;
            setError(ERR_VAR_REPEATED, currentToken.line, NULL);
            delete (proc);
            return 255;
        }
        tmpVar.procIndex = procIndex;

        Symbol arrCopy = outsideScopes.at(arrScopeIdx).vars.at(arrVarIdx);
        Symbol* arr = &arrCopy;
        // 分析tmp的类型
        switch (arr->type.kind) {
        case IR_DT_INT_ARR:
            tmpVar.type.kind = IR_DT_INT;
            tmpVar.size = 4;
            break;
        case IR_DT_FLOAT_ARR:
            tmpVar.type.kind = IR_DT_FLOAT;
            tmpVar.size = 8;
            break;
        case IR_DT_CHAR_ARR:
            tmpVar.type.kind = IR_DT_CHAR;
            tmpVar.size = 2;
            break;
        case IR_DT_STRING_ARR:
            tmpVar.type.kind = IR_DT_STRING;
            tmpVar.size = 8;
            break;
        case IR_DT_CUSTOM_ARR:
            tmpVar.type.kind = IR_DT_CUSTOM;
            tmpVar.size = 8;
            tmpVar.type.customTypeName = (wchar_t*)calloc(wcslen(arr->type.customTypeName) + 1, sizeof(wchar_t));
            if (!tmpVar.type.customTypeName) {
                *err = -1;
                delete (proc);
                return -1;
            }
            wcscpy(tmpVar.type.customTypeName, arr->type.customTypeName);
            break;
        default:
            setError(ERR_FOR, function->bodyTokens[index].line, NULL);
            *err = 255;
            delete (proc);
            return 255;
        }
        tmpVar.isUsed = true;

        Symbol tmpIndexVar;  // 用于存索引
        tmpIndexVar.size = sizeof(uint32_t);
        tmpIndexVar.type = {IR_DT_INT};
        tmpIndexVar.procIndex = procIndex;
        tmpIndexVar.name = nullptr;
        tmpIndexVar.isUsed = true;  // 循环变量绝对被使用了
        outsideScopes.at(localeScopeIndex).vars.push_back(tmpIndexVar);
        int indexOfTmpIndexVar = (int)outsideScopes.at(localeScopeIndex).vars.size() - 1;  // 记录真正的循环变量索引
        if (indexOfTmpIndexVar < 0 || indexOfTmpIndexVar >= (int)outsideScopes.at(localeScopeIndex).vars.size()) {
            setError(ERR_FOR, currentToken.line, NULL);
            *err = 255;
            delete (proc);
            return 255;
        }
        outsideScopes.at(arrScopeIdx).vars.at(arrVarIdx).isUsed = true;
        outsideScopes.at(localeScopeIndex).vars.at(indexOfTmpIndexVar).isUsed = true;
        // 初始化数组索引
#ifdef HX_DEBUG
        log(L"初始化数组索引");
#endif
        Instruction initIndexInst = {};
        initIndexInst.opcode = OP_LOAD_CONST;
        initIndexInst.params[0].type = PARAM_TYPE_INT;
        uint32_t initIndex = 0;
        memcpy(initIndexInst.params[1].value, &initIndex, sizeof(uint32_t));
        initIndexInst.params[1].size = sizeof(uint32_t);
        proc->instructions.push_back(initIndexInst);
        outsideScopes.at(localeScopeIndex).vars.at(indexOfTmpIndexVar).instIndex.push_back(proc->instructions.size() - 1);

        Instruction storeIndexInst = {};
        storeIndexInst.opcode = OP_STORE_VAR;
        storeIndexInst.params[0].type = PARAM_TYPE_OFFEST;
        uint32_t indexOffset = (uint32_t)(localeSymbolTable.vars.size() - 1);
        memcpy(storeIndexInst.params[0].value, &indexOffset, sizeof(uint32_t));
        storeIndexInst.params[0].size = sizeof(uint32_t);
        storeIndexInst.params[1].type = PARAM_TYPE_INT;
        *(uint32_t*)storeIndexInst.params[1].value = sizeof(uint32_t);
        storeIndexInst.params[1].size = sizeof(uint32_t);
        if (indexOfTmpIndexVar < 0 || indexOfTmpIndexVar >= (int)localeSymbolTable.vars.size()) {
            setError(ERR_FOR, currentToken.line, NULL);
            *err = 255;
            delete (proc);
            return 255;
        }
        outsideScopes.at(localeScopeIndex).vars[indexOfTmpIndexVar].instIndex.push_back(proc->instructions.size());
        proc->instructions.push_back(storeIndexInst);
        outsideScopes.at(localeScopeIndex).vars.at(indexOfTmpIndexVar).instIndex.push_back(proc->instructions.size() - 1);
        // 初始化结束
        // 每次循环后跳转地址
        uint32_t jmpAddr = proc->instructions.size();
        // 正式循环
        // 1.加载索引
#ifdef HX_DEBUG
        log(L"加载索引");
#endif
        Instruction loadIndexInst = {};
        loadIndexInst.opcode = OP_LOAD_VAR;
        loadIndexInst.params[0].type = PARAM_TYPE_OFFEST;
        loadIndexInst.params[0].size = sizeof(uint32_t);
        loadIndexInst.params[1].type = PARAM_TYPE_INT;
        loadIndexInst.params[1].size = sizeof(uint32_t);
        memcpy(loadIndexInst.params[0].value, &indexOffset, sizeof(uint32_t));
        if (indexOfTmpIndexVar < 0 || indexOfTmpIndexVar >= (int)localeSymbolTable.vars.size()) {
            setError(ERR_FOR, currentToken.line, NULL);
            *err = 255;
            delete (proc);
            return 255;
        }
        proc->instructions.push_back(loadIndexInst);
        outsideScopes.at(localeScopeIndex).vars.at(indexOfTmpIndexVar).instIndex.push_back(proc->instructions.size() - 1);
        // 2.加载数组元素
#ifdef HX_DEBUG
        log(L"加载数组元素");
#endif
        Instruction loadElementInst = {};
        loadElementInst.opcode = OP_LOAD_ELEMENT_FROM_ARRAY;
        loadElementInst.params[0].type = PARAM_TYPE_OFFEST;
        loadElementInst.params[0].size = sizeof(uint32_t);

        uint32_t arrIndexVal = (uint32_t)arrVarIdx;
        memcpy(loadElementInst.params[0].value, &arrIndexVal, sizeof(uint32_t));

        switch (outsideScopes.at(arrScopeIdx).vars.at(arrVarIdx).type.kind) {
        case IR_DT_INT_ARR:
            loadElementInst.params[1].type = PARAM_TYPE_INT;
            loadElementInst.params[1].size = sizeof(uint32_t);
            break;
        case IR_DT_FLOAT_ARR:
            loadElementInst.params[1].type = PARAM_TYPE_FLOAT;
            loadElementInst.params[1].size = sizeof(uint32_t);
            break;
        case IR_DT_CHAR_ARR:
            loadElementInst.params[1].type = PARAM_TYPE_CHAR;
            loadElementInst.params[1].size = sizeof(uint32_t);
            break;
        case IR_DT_STRING_ARR:
            loadElementInst.params[1].type = PARAM_TYPE_STRING;
            loadElementInst.params[1].size = sizeof(uint32_t);
            break;
        case IR_DT_CUSTOM_ARR:
            loadElementInst.params[1].type = PARAM_TYPE_ADDRESS;
            loadElementInst.params[1].size = sizeof(uint32_t);
            break;
        }
        outsideScopes.at(arrScopeIdx).vars.at(arrVarIdx).instIndex.push_back(proc->instructions.size());
        proc->instructions.push_back(loadElementInst);
        // 3.存入tmp
#ifdef HX_DEBUG
        log(L"存入tmp");
#endif
        Instruction storeTmpInst = {};
        storeTmpInst.opcode = OP_STORE_VAR;
        storeTmpInst.params[0].type = PARAM_TYPE_OFFEST;
        storeTmpInst.params[0].size = sizeof(uint32_t);
        storeTmpInst.params[1].type = loadElementInst.params[1].type;
        storeTmpInst.params[1].size = loadElementInst.params[1].size;
        tmpVar.instIndex.push_back(proc->instructions.size());
        proc->instructions.push_back(storeTmpInst);
        // 4.执行循环体
#ifdef HX_DEBUG
        log(L"执行循环体");
#endif
        if (index + 1 >= function->body_token_count) {
            setError(ERR_FOR, currentToken.line, NULL);
            *err = 255;
            delete (proc);
            return 255;
        }
        (index)++;  // 指向->
        if (function->bodyTokens[index].type != TOK_OPR_POINT) {
            setError(ERR_FOR, currentToken.line, NULL);
            *err = 255;
            delete (proc);
            return 255;
        }
        if (index + 1 >= function->body_token_count) {
            setError(ERR_FOR, currentToken.line, NULL);
            *err = 255;
            delete (proc);
            return 255;
        }
        (index)++;
       generateClassFunctionStatement(index, pitchTable, constantPool, classIndex, globalFunctionCount, allFunctions, currentProgram, allFunctionCount, function,
                              outsideScopes.back(), outsideScopes, localeScopeIndex, proc, procIndex, stackSize,
                              localVarSize, err);
        outsideScopes.back().vars.push_back(tmpVar);  // userTmpVar属于新作用域
        if (*err != 0) {
            delete (proc);
            return 255;
        }
        // 5.索引加1
#ifdef HX_DEBUG
        log(L"索引自增");
#endif
        Instruction incIndexInst = {};
        incIndexInst.opcode = OP_INC;
        incIndexInst.params[0].type = PARAM_TYPE_OFFEST;
        incIndexInst.params[0].size = sizeof(uint32_t);
        incIndexInst.params[1].type = PARAM_TYPE_INT;
        if (indexOfTmpIndexVar < 0 || indexOfTmpIndexVar >= (int)outsideScopes.at(localeScopeIndex).vars.size()) {
            setError(ERR_FOR, currentToken.line, NULL);
            *err = 255;
            delete (proc);
            return 255;
        }
        outsideScopes.at(localeScopeIndex).vars[indexOfTmpIndexVar].instIndex.push_back(proc->instructions.size());
        proc->instructions.push_back(incIndexInst);
        // 6.判断索引是否小于数组长度
#ifdef HX_DEBUG
        log(L"判断索引是否小于数组长度");
#endif
        Instruction loadIndexInst2 = {};
        loadIndexInst2.opcode = OP_LOAD_VAR;
        loadIndexInst2.params[0].type = PARAM_TYPE_OFFEST;
        loadIndexInst2.params[0].size = sizeof(uint32_t);
        loadIndexInst2.params[1].type = PARAM_TYPE_INT;
        loadIndexInst2.params[1].size = sizeof(uint32_t);
        *(uint32_t*)loadIndexInst2.params[1].value = sizeof(uint32_t);
        uint32_t tmpIndexVal = (uint32_t)indexOfTmpIndexVar;
        memcpy(loadIndexInst2.params[0].value, &tmpIndexVal, sizeof(uint32_t));
        if (indexOfTmpIndexVar < 0 || indexOfTmpIndexVar >= (int)outsideScopes.at(localeScopeIndex).vars.size()) {
            setError(ERR_FOR, currentToken.line, NULL);
            *err = 255;
            delete (proc);
            return 255;
        }

        outsideScopes.at(localeScopeIndex).vars[indexOfTmpIndexVar].instIndex.push_back(proc->instructions.size());
        proc->instructions.push_back(loadIndexInst2);
        Instruction loadArrSizeInst = {};
        loadArrSizeInst.opcode = OP_LOAD_VAR;
        // 读取：数组在栈中偏移量+8， uint32_t类型的数组长度
        loadArrSizeInst.params[0].type = PARAM_TYPE_OFFEST;
        loadArrSizeInst.params[0].size = sizeof(uint32_t);
        loadArrSizeInst.params[1].type = PARAM_TYPE_INT;
        loadArrSizeInst.params[1].size = sizeof(uint32_t);
        loadArrSizeInst.params[0].offest = 8;
        *(uint32_t*)loadArrSizeInst.params[1].value = sizeof(uint32_t);
        uint32_t arrIndexVal2 = (uint32_t)arrVarIdx;
        memcpy(loadArrSizeInst.params[0].value, &arrIndexVal2, sizeof(uint32_t));

        outsideScopes.at(arrScopeIdx).vars.at(arrVarIdx).instIndex.push_back(proc->instructions.size());
        proc->instructions.push_back(loadArrSizeInst);
        Instruction cmpInst = {};
        cmpInst.opcode = OP_LT;
        outsideScopes.at(arrScopeIdx).vars.at(arrVarIdx).instIndex.push_back(proc->instructions.size());
        proc->instructions.push_back(cmpInst);
        // 7.条件跳转
        Instruction jmpInst = {};
        jmpInst.opcode = OP_JMP_CONDITION;
        jmpInst.params[0].type = PARAM_TYPE_INDEX;
        memcpy(jmpInst.params[0].value, &jmpAddr, sizeof(uint32_t));
        jmpInst.params[0].size = sizeof(uint32_t);
        uint32_t falseJmpAddr = proc->instructions.size() + 1;
        jmpInst.params[1].type = PARAM_TYPE_INDEX;
        jmpInst.params[1].size = sizeof(uint32_t);
        memcpy(jmpInst.params[1].value, &falseJmpAddr, sizeof(uint32_t));
        proc->instructions.push_back(jmpInst);
        Instruction nopInst = {};
        nopInst.opcode = OP_NOP;
        proc->instructions.push_back(nopInst);
    }
    return 0;
}