#ifndef HXHLANG_SRC_HXC_GENERATOR_H
#define HXHLANG_SRC_HXC_GENERATOR_H
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <wchar.h>

#include <vector>

#include "Error.h"
#include "IR.h"
#include "Lexer.h"
#include "SymbolTable.h"
typedef uint16_t wchar;
#include "ObjectCode.h"
#include "Parser.h"

/*
 * 生成目标代码
 * @param program: 中间表示
 * @param err: 错误码指针，发生错误时会设置为相应错误码
 * @return 生成的目标代码对象指针，发生错误时返回NULL
 */
extern ObjectCode* generateObjectCode(IR_Program* program, int* err);
extern void freeObjectCode(ObjectCode** obj);
static void generateInstructionsFromAST(std::vector<Instruction>& instructions, int* inst_index, int* inst_size, ASTNode* node,
                                        ConstantPool* constantPool, std::vector<SymbolTable>& symbols, int procIndex, int* err);
/*
 * 生成函数的目标代码
 * @param function: 中间表示的函数
 * @param err: 错误码指针，发生错误时会设置为相应错误码
 * @return 生成的过程对象指针，发生错误时返回NULL
 */
static Procedure* generateFunction(int procIndex, IR_Function* function, IR_Program* program, FunCallPitchTable& pitchTable,
                                   ConstantPool* constantPool, std::vector<IR_Function*>& all_functions, int all_function_count,
                                   std::vector<SymbolTable>& symbols, int* err);
/**
 * 生成定义变量的目标代码
 */
static Procedure* generateVariable();
static int getMainFunctionIndex(IR_Program* program) {
    if (!program) return -1;
    int index = -1;
    for (int i = 0; i < program->functions.size(); i++) {
        if ((program->functions[i] && program->functions[i]->name && wcscmp(program->functions[i]->name, L"main") == 0) ||
            (program->functions[i] && program->functions[i]->name && wcscmp(program->functions[i]->name, L"主函数") == 0)) {
            if (index != -1) {
                // 已经找到过main函数，说明有重定义
                index = -255;
                setError(ERR_MAIN, program->functions[i]->bodyTokens[0].line, program->functions[i]->name);
                break;
            }
            index = i;
        }
    }
    return index;
}
#ifdef HX_DEBUG
static void listObjectCode_Proc(Procedure* proc) {
    if (!proc) return;
    fwprintf(logStream, L"[DEB] 过程指令数: %u\n", proc->instructionSize);
    for (uint32_t i = 0; i < proc->instructions.size(); i++) {
        Instruction& ins = proc->instructions[i];
        if (ins.isNotUsed) {
            fwprintf(logStream, L"\33[31m(无用)\33[0m");
        }
        switch (ins.opcode) {
            case OP_LOAD_CONST: {
                fwprintf(logStream, L"\t%03u: \33[1;34mOP_LOAD_CONST\33[0m", i);
                // print param type and value for common types
                if (ins.params[0].type == PARAM_TYPE_INT) {
                    int32_t v;
                    memcpy(&v, ins.params[0].value, sizeof(int32_t));
                    fwprintf(logStream, L" INT=%d", v);
                } else if (ins.params[0].type == PARAM_TYPE_FLOAT) {
                    double d;
                    memcpy(&d, ins.params[0].value, sizeof(double));
                    fwprintf(logStream, L" FLOAT=%f", d);
                } else if (ins.params[0].type == PARAM_TYPE_CHAR) {
                    wchar_t c;
                    memcpy(&c, ins.params[0].value, sizeof(wchar_t));
                    fwprintf(logStream, L" CHAR=\'%lc\'", c);
                } else if (ins.params[0].type == PARAM_TYPE_INDEX) {
                    uint32_t idx = 0;
                    memcpy(&idx, ins.params[0].value, sizeof(uint32_t));
                    fwprintf(logStream, L" INDEX=%u", idx);
                }
                fwprintf(logStream, L"\n");
                break;
            }
            case OP_LOAD_VAR: {
                void* offest = ins.params[0].value;
                void* size = ins.params[1].value;
                fwprintf(logStream,
                         L"\t%03u: \33[1;34mOP_LOAD_VAR\33[0m "
                         L"offest=%u,    size=%u\n",
                         i, *((uint32_t*)offest), *((uint32_t*)size));
            } break;
            case OP_STORE_VAR:
                fwprintf(logStream,
                         L"\t%03u: \33[1;34mOP_STORE_VAR "
                         L"%u(offest), %u(copySize)\33[0m\n",
                         i, *((uint32_t*)ins.params[0].value), *((uint32_t*)ins.params[1].value));
                break;
            case OP_ADD:
                fwprintf(logStream, L"\t%03u: \33[1;34mOP_ADD\33[0m\n", i);
                break;
            case OP_SUB:
                fwprintf(logStream, L"\t%03u: \33[1;34mOP_SUB\33[0m\n", i);
                break;
            case OP_MUL:
                fwprintf(logStream, L"\t%03u: \33[1;34mOP_MUL\33[0m\n", i);
                break;
            case OP_DIV:
                fwprintf(logStream, L"\t%03u: \33[1;34mOP_DIV\33[0m\n", i);
                break;
            case OP_CAL:
                fwprintf(logStream,
                         L"\t%03u: \33[1;34mOP_CAL\33[0m "
                         L"%ls(funName), %u(paramCount)\n",
                         i, ins.pitch == NULL ? L" " : ins.pitch->fun->name, *((uint32_t*)ins.params[1].value));
                break;
            case OP_RET:
                fwprintf(logStream, L"\t%03u: \33[1;34mOP_RET\33[0m\n", i);
                break;
            case OP_PRINT_STRING:
                fwprintf(logStream, L"\t%03u: \33[1;34mOP_PRINT_STRING\33[0m\n", i);
                break;
            case OP_CHAR_TO_INT:
                fwprintf(logStream, L"\t%03u: \33[1;34m OP_CHAR_TO_INT\33[0m\n", i);
                break;
            case OP_INT_TO_CHAR:
                fwprintf(logStream, L"\t%03u: \33[1;34m OP_INT_TO_CHAR\33[0m\n", i);
                break;
            case OP_INT_TO_FLOAT:
                fwprintf(logStream, L"\t%03u: \33[1;34m OP_INT_TO_FLOAT\33[0m\n", i);
                break;
            case OP_CHAR_TO_FLOAT:
                fwprintf(logStream,
                         L"\t%03u: \33[1;34m "
                         L"OP_CHAR_TO_FLOAT\33[0m\n",
                         i);
                break;
            case OP_CHAR_TO_STRING:
                (logStream, L"\t%03u: \33[1;34m OP_CHAR_TO_STRING\33[0m\n", i);
                break;
            case OP_FLOAT_TO_INT:
                (logStream, L"\t%03u: \33[1;34m OP_FLOAT_TO_INT\33[0m\n", i);
                break;
            case OP_INT_TO_STRING:
                fwprintf(logStream,
                         L"\t%03u: \33[1;34m "
                         L"OP_INT_TO_STRING\33[0m\n",
                         i);
                break;
            case OP_POP:
                fwprintf(logStream, L"\t%03u: \33[1;34m OP_POP\33[0m\n", i);
                break;
            case OP_JMP:
                fwprintf(logStream,
                         L"\t%03u: \33[1;34m OP_JMP "
                         L"(addr)%u\33[0m\n",
                         i, *((uint32_t*)ins.params[0].value));
                break;
            case OP_JMP_CONDITION:
                fwprintf(logStream,
                         L"\t%03u: \33[1;34m OP_JMP_CONDITION "
                         L"(trueAddr)%u  (false)%u\33[0m\n",
                         i, *((uint32_t*)ins.params[0].value), *((uint32_t*)ins.params[1].value));
                break;
            case OP_EQU:
                fwprintf(logStream, L"\t%03u: \33[1;34m OP_EQU\33[0m\n", i);
                break;
            case OP_AND:
                fwprintf(logStream, L"\t%03u: \33[1;34m OP_AND\33[0m\n", i);
                break;
            case OP_AND_LOGIC:
                fwprintf(logStream, L"\t%03u: \33[1;34m OP_AND_LOGIC\33[0m\n", i);
                break;
            case OP_OR:
                fwprintf(logStream, L"\t%03u: \33[1;34m OP_OR\33[0m\n", i);
                break;
            case OP_GT:
                fwprintf(logStream, L"\t%03u: \33[1;34m OP_GT\33[0m\n", i);
                break;
            case OP_LT:
                fwprintf(logStream, L"\t%03u: \33[1;34m OP_LT\33[0m\n", i);
                break;
            case OP_OR_LOGIC:
                fwprintf(logStream, L"\t%03u: \33[1;34m OP_OR_LOGIC\33[0m\n", i);
                break;
            case OP_NEQU:
                fwprintf(logStream, L"\t%03u: \33[1;34m OP_NEQU\33[0m\n", i);
                break;
            case OP_NOT:
                fwprintf(logStream, L"\t%03u: \33[1;34m OP_NOT\33[0m\n", i);
                break;
            case OP_NOT_LOGIC:
                fwprintf(logStream, L"\t%03u: \33[1;34m OP_NOT_LOGIC\33[0m\n", i);
                break;
            case OP_NOP:
                fwprintf(logStream, L"\t%03u: \33[1;32mOP_NOP\33[0m\n", i);
                break;
            case OP_INC: {
                void* offest = ins.params[0].value;
                void* size = ins.params[1].value;
                fwprintf(logStream,
                         L"\t%03u: \33[1;34mOP_INC\33[0m "
                         L"offest=%u,    size=%u\n",
                         i, *((uint32_t*)offest), *((uint32_t*)size));
                break;
            }
            case OP_DEC: {
                void* offest = ins.params[0].value;
                void* size = ins.params[1].value;
                fwprintf(logStream,
                         L"\t%03u: \33[1;34mOP_DEC\33[0m "
                         L"offest=%u,    size=%u\n",
                         i, *((uint32_t*)offest), *((uint32_t*)size));
                break;
            }
        }
    }
}
#endif
static void markUsedFun(Procedure* fun, std::vector<Procedure*>& objFun) {
    // 防空指针且防止循环调用（A调B，B调A）导致的爆栈
    if (fun == nullptr || fun->isUsed) {
        return;
    }
    fun->isUsed = true;
    // 遍历当前过程的所有指令，寻找函数调用
    for (size_t i = 0; i < fun->instructions.size(); i++) {
        Instruction& instr = fun->instructions.at(i);
        if (instr.opcode == OP_CAL) {
            // 安全检查，确保 pitch 和 fun 都存在
            if (instr.pitch != nullptr && instr.pitch->fun != nullptr) {
                IR_Function* targetFun = instr.pitch->fun;
                Procedure* targetProc = targetFun->proc;

                if (targetProc != nullptr) {
                    targetFun->isUsed = true;  // 标记 IR 层的函数
#ifdef HX_DEBUG
                    log(L"标记函数: %ls", targetFun->name);
#endif
                    // 递归追踪被调用的函数
                    markUsedFun(targetProc, objFun);
                }
            }
        }
    }
}
static int getVarSize(IR_DataType type, IR_Class** class_table, int class_table_size);
//----------------------------------------------------------------------------
ObjectCode* generateObjectCode(IR_Program* program, int* err) {
    if (!program || !err) {
        if (err) *err = -1;
        return NULL;
    }
    // 找入口函数
    int mainIndex = getMainFunctionIndex(program);
    if (mainIndex == -255) {
        *err = 255;
        return NULL;
    } else if (mainIndex == -1) {
        setError(ERR_NO_MAIN, 0, NULL);
        *err = 255;
        return NULL;
    }
    ObjectCode* objCode = new (std::nothrow) ObjectCode;
    if (!objCode) {
        *err = -1;
        return NULL;
    }
    objCode->constantPool.size = 0;
    objCode->constantPool.constants = NULL;
    objCode->procedureSize = 0;
    if (!objCode) {
        if (err) *err = -1;
        return NULL;
    }
    objCode->procedureSize = 0;
// 生成main函数的目标代码
#ifdef HX_DEBUG
    log(L"生成函数的目标代码...");
#endif
    FunCallPitchTable pitchTable;
    std::vector<Procedure*> objFun;
    std::vector<std::vector<SymbolTable>> symbols;
    for (int i = 0; i < program->functions.size(); i++) {
        std::vector<SymbolTable> table;
        symbols.push_back(table);
#ifdef HX_DEBUG
        fwprintf(logStream, L"编译函数 %ls\n", program->functions[i]->name);
#endif

        Procedure* newProc = generateFunction(i, program->functions[i], program, pitchTable, &(objCode->constantPool),
                                              program->functions, program->functions.size(), symbols.at(i), err);

        if (newProc == NULL || *err != 0) {
            return NULL;
        }
        newProc->fun = program->functions[i];
        objFun.push_back(newProc);
        if (*err != 0) return NULL;
    }
    if (*err != 0) {
        // freeObjectCode(&objCode);
        return NULL;
    }

    // main在目标中索引等于中间表示中的索引
    markUsedFun(objFun.at(mainIndex), objFun);
    // 填入函数
    for (int i = 0; i < objFun.size(); i++) {
        if (objFun.at(i)->isUsed == true) {
#ifdef HX_DEBUG
            log(L"写入函数%ls", objFun.at(i)->fun->name);
#endif
            objCode->procedures.push_back(objFun.at(i));
            // objFun.at(i)->fun->pitch->index =
            // objCode->procedureSize;
            (pitchTable.enter(objFun.at(i)->fun))->index = objCode->procedureSize;
            if (i == mainIndex) {
                objCode->start = objCode->procedureSize;
#ifdef HX_DEBUG
                log(L"设置入口函数%ls[%d]", objCode->procedures.at(objCode->start)->fun->name, objCode->start);
#endif
            }
            objCode->procedureSize++;
        }
    }

#ifdef HX_DEBUG
    pitchTable.list();
#endif

    // 回填
    for (int i = 0; i < objCode->procedureSize; i++) {
        std::vector<Instruction>& inst = objCode->procedures.at(i)->instructions;
        for (int instIndex = 0; instIndex < inst.size(); instIndex++) {
            if (inst.at(instIndex).opcode == OP_CAL) {
                memcpy((inst.at(instIndex).params[0].value), &(inst.at(instIndex).pitch->index), sizeof(uint32_t));
#ifdef HX_DEBUG
                uint32_t* index = (uint32_t*)(inst.at(instIndex).params[0].value);
                log(L"设置OP_CALL %d", *index);
#endif
            }
        }
    }

    // 变量处理
#ifdef HX_DEBUG
    log(L"处理变量中...");
#endif
    // 计算大小 & 偏移量
#ifdef HX_DEBUG
    log(L"========计算变量大小");
#endif
    for (int i = 0; i < symbols.size(); i++) {
        std::vector<SymbolTable>& funTable = symbols.at(i);
        for (int j = 0; j < funTable.size(); j++) {
            SymbolTable& blockTable = funTable.at(j);
            int offest = 0;
            for (int n = 0; n < blockTable.vars.size(); n++) {
                int size = getVarSize(blockTable.vars.at(n).type, program->classes, program->class_count);
                blockTable.vars.at(n).offest = offest;
                blockTable.vars.at(n).size = size;
                offest += blockTable.vars.at(n).size;
#ifdef HX_DEBUG
                log(L"变量%ls：size = %u, offest = %u", blockTable.vars.at(n).name, blockTable.vars.at(n).size,
                    blockTable.vars.at(n).offest);
#endif
            }
        }
    }
#ifdef HX_DEBUG
    log(L"将从未使用过的变量对应的指标记为无用  & 处理指令里变量的偏移量 & 更新OP_JMP地址");
#endif
    // 将从未使用过的变量对应的指标记为无用  & 处理指令里变量的偏移量 &
    // 更新OP_JMP地址

    for (int i = 0; i < symbols.size(); i++) {
        std::vector<SymbolTable>& funTable = symbols.at(i);
        for (int j = 0; j < funTable.size(); j++) {
            SymbolTable& blockTable = funTable.at(j);
            for (int n = 0; n < blockTable.vars.size(); n++) {
#ifdef HX_DEBUG
                log(L"\n变量%ls相关指令数量：%d", blockTable.vars.at(n).name, blockTable.vars.at(n).instIndex.size());
#endif
                objCode->procedures.at(blockTable.vars.at(n).procIndex)->stackSize += blockTable.vars.at(n).size;
                for (int m = 0; m < blockTable.vars.at(n).instIndex.size(); m++) {
                    switch (objCode->procedures.at(blockTable.vars.at(n).procIndex)
                                ->instructions[blockTable.vars.at(n).instIndex.at(m)]
                                .opcode) {
                        case OP_LOAD_VAR: {
#ifdef HX_DEBUG
                            log(L"========"
                                L"处理LOAD_VAR偏移"
                                L"量及大小");
#endif
                            uint32_t index = 0;
                            memcpy(&index,
                                   objCode->procedures.at(blockTable.vars.at(n).procIndex)
                                       ->instructions[blockTable.vars.at(n).instIndex.at(m)]
                                       .params[0]
                                       .value,
                                   sizeof(uint32_t));
                            if (n != index) continue;
                            uint32_t offest = blockTable.vars.at(n).offest;
                            int32_t _size = blockTable.vars.at(n).size;
                            uint32_t size = (uint32_t)_size;
                            if (_size <= 0) {
                                size = blockTable.vars.at(n).size;
                            }
#ifdef HX_DEBUG
                            log(L"offest = "
                                L"%u, "
                                L"size = "
                                L"%u",
                                offest, size);
#endif
                            memcpy(objCode->procedures.at(blockTable.vars.at(n).procIndex)
                                       ->instructions[blockTable.vars.at(n).instIndex.at(m)]
                                       .params[0]
                                       .value,
                                   &offest, sizeof(uint32_t));
                            memcpy(objCode->procedures.at(blockTable.vars.at(n).procIndex)
                                       ->instructions[blockTable.vars.at(n).instIndex.at(m)]
                                       .params[1]
                                       .value,
                                   &size, sizeof(uint32_t));
                        } break;
                        case OP_INC: {
#ifdef HX_DEBUG
                            log(L"========"
                                L"处理INC偏移"
                                L"量及大小");
#endif
                            uint32_t offest = blockTable.vars.at(n).offest;
                            int32_t _size = blockTable.vars.at(n).size;
                            uint32_t size = (uint32_t)_size;
                            if (_size <= 0) {
                                size = blockTable.vars.at(n).size;
                            }
#ifdef HX_DEBUG
                            log(L"处理INC-> offest = "
                                L"%u, size "
                                L"= %u",
                                offest, size);
#endif
                            memcpy(objCode->procedures.at(blockTable.vars.at(n).procIndex)
                                       ->instructions[blockTable.vars.at(n).instIndex.at(m)]
                                       .params[0]
                                       .value,
                                   &offest, sizeof(uint32_t));
                            memcpy(objCode->procedures.at(blockTable.vars.at(n).procIndex)
                                       ->instructions[blockTable.vars.at(n).instIndex.at(m)]
                                       .params[1]
                                       .value,
                                   &size, sizeof(uint32_t));
                        } break;
                        case OP_DEC: {
#ifdef HX_DEBUG
                            log(L"========"
                                L"处理DEC偏移"
                                L"量及大小");
#endif
                            uint32_t offest = blockTable.vars.at(n).offest;
                            int32_t _size = blockTable.vars.at(n).size;
                            uint32_t size = (uint32_t)_size;
                            if (_size <= 0) {
                                size = blockTable.vars.at(n).size;
                            }
#ifdef HX_DEBUG
                            log(L"处理DEC-> offest = "
                                L"%u, size "
                                L"= %u",
                                offest, size);
#endif
                            memcpy(objCode->procedures.at(blockTable.vars.at(n).procIndex)
                                       ->instructions[blockTable.vars.at(n).instIndex.at(m)]
                                       .params[0]
                                       .value,
                                   &offest, sizeof(uint32_t));
                            memcpy(objCode->procedures.at(blockTable.vars.at(n).procIndex)
                                       ->instructions[blockTable.vars.at(n).instIndex.at(m)]
                                       .params[1]
                                       .value,
                                   &size, sizeof(uint32_t));
                        } break;
                        case OP_STORE_VAR: {
                            // OP_STORE_VAR
                            // <offest>
                            // <copySize>
#ifdef HX_DEBUG
                            log(L"========"
                                L"处理STORE_VAR偏移"
                                L"量");
#endif
                            uint32_t offest = blockTable.vars.at(n).offest;
                            int32_t _size = blockTable.vars.at(n).size;
                            uint32_t size = (uint32_t)_size;
                            if (_size <= 0) {
                                size = blockTable.vars.at(n).size;
                            }
#ifdef HX_DEBUG
                            log(L"处理STORE_VAR-> offest = "
                                L"%u, size "
                                L"= %u",
                                offest, size);
#endif
                            memcpy(objCode->procedures.at(blockTable.vars.at(n).procIndex)
                                       ->instructions[blockTable.vars.at(n).instIndex.at(m)]
                                       .params[0]
                                       .value,
                                   &offest, sizeof(uint32_t));
                            memcpy(objCode->procedures.at(blockTable.vars.at(n).procIndex)
                                       ->instructions[blockTable.vars.at(n).instIndex.at(m)]
                                       .params[1]
                                       .value,
                                   &size, sizeof(uint32_t));
                        } break;
                    }
                }
                if (!blockTable.vars.at(n).isUsed) {
                    for (int m = 0; m < blockTable.vars.at(n).instIndex.size(); m++) {
                        objCode->procedures.at(blockTable.vars.at(n).procIndex)
                            ->instructions[blockTable.vars.at(n).instIndex.at(m)]
                            .isNotUsed = true;
                        for (int instIndex = blockTable.vars.at(n).instIndex.at(m);
                             instIndex < objCode->procedures.at(blockTable.vars.at(n).procIndex)->instructions.size();
                             instIndex++) {
                            if (objCode->procedures.at(blockTable.vars.at(n).procIndex)->instructions.at(instIndex).opcode ==
                                OP_JMP) {
#ifdef HX_DEBUG
                                log(L"更新OP_JMP(第%u指令)", instIndex);
#endif
                                uint32_t jmpAddr = 0;
                                memcpy(&jmpAddr,
                                       (objCode->procedures.at(blockTable.vars.at(n).procIndex)
                                            ->instructions.at(instIndex)
                                            .params[0])
                                           .value,
                                       sizeof(uint32_t));
                                jmpAddr -= 1;
                                memcpy((objCode->procedures.at(blockTable.vars.at(n).procIndex)
                                            ->instructions.at(instIndex)
                                            .params[0])
                                           .value,
                                       &jmpAddr, sizeof(uint32_t));
                            }
                        }
                    }
#ifdef HX_DEBUG
                    log(L"已将变量“%ls”设为无用", blockTable.vars.at(n).name);
#endif
                    objCode->procedures.at(blockTable.vars.at(n).procIndex)->stackSize =
                        objCode->procedures.at(blockTable.vars.at(n).procIndex)->stackSize -
                        getVarSize(blockTable.vars.at(n).type, program->classes, program->class_count);

                    for (int m = n; m < blockTable.vars.size(); m++) {
                        blockTable.vars.at(m).offest -= blockTable.vars.at(n).size;
                    }
                }
            }
        }
    }
#ifdef HX_DEBUG
    for (int i = 0; i < program->functions.size(); i++) {
        listObjectCode_Proc(objFun.at(i));
    }
#endif
    return objCode;
}
static int getClassIndexByName(wchar_t* name, IR_Class** class_table, int class_table_size) {
    if (!name || !class_table) return -1;
    for (int i = 0; i < class_table_size; i++) {
        if (!class_table[i]) continue;
        if (wcscmp(name, class_table[i]->name) == 0) {
            return i;
        }
    }
    return -1;
}
static int getVarSize(IR_DataType type, IR_Class** class_table, int class_table_size) {
    if (!class_table) return 0;
    switch (type.kind) {
        case IR_DT_INT:
            return sizeof(int32_t);
        case IR_DT_FLOAT:
            return sizeof(double);
        case IR_DT_CHAR:
            return sizeof(uint16_t);
        case IR_DT_BOOL:
            return sizeof(bool);
        case IR_DT_CUSTOM:
            return 4;  // void*
        //(class_table)[getClassIndexByName(type.customTypeName, class_table, class_table_size)]->size;
        default:
            return 8;
    }
}
/*生成语句的目标代码*/
static int generateStatement(int& index, FunCallPitchTable& pitchTable, ConstantPool* constantPool,
                             std::vector<IR_Function*>& all_functions, IR_Program* currentProgram, int all_function_count,
                             IR_Function* function, SymbolTable& localeSymbolTable, std::vector<SymbolTable>& outsideScopes,
                             int localeScopeIndex, Procedure* proc, int procIndex, uint32_t& stackSize, uint32_t& localVarSize,
                             int* err);
Procedure* generateFunction(int procIndex, IR_Function* function, IR_Program* currentProgram, FunCallPitchTable& pitchTable,
                            ConstantPool* constantPool, std::vector<IR_Function*>& all_functions, int all_function_count,
                            std::vector<SymbolTable>& symbols, int* err) {
    if (!function || !err) {
        if (err) *err = -1;
        return NULL;
    }
    initLocale();
    Procedure* proc = new (std::nothrow) Procedure();
    if (!proc) {
        *err = -1;
        return NULL;
    }
    proc->fun = function;
    if (function->body_token_count == 0 || !function->bodyTokens) {
        // 空函数体
        proc->stackSize = 0;
        return proc;
    }
    int index = 1;  // 跳过 {
    uint32_t localVarSize = 0;
    SymbolTable localeSymbolTable = {};
    // 填充函数表
    localeSymbolTable.fun = all_functions;
    // 正序填入参数
    if (function->paramCount > 0 && function->params != NULL) {
#ifdef HX_DEBUG
        log(L"填入参数");
#endif
        for (int i = 0; i < function->paramCount; i++) {
            localVarSize++;
            Symbol param;
            param.name = wcsdup(function->params[i].name);
            if (!param.name) {
                *err = -1;
                return NULL;
            }
            param.type = function->params[i].type;
            param.isTypeKnown = true;
            param.procIndex = procIndex;
            param.isUsed = true;
            localeSymbolTable.vars.push_back(param);
        }
    }
    symbols.push_back(localeSymbolTable);
    int localeScopeIndex = symbols.size() - 1;
#ifdef HX_DEBUG
    log(L"all_functions.size = %d", all_functions.size());
#endif
#ifdef HX_DEBUG
    log(L"localeSymbolTable.fun.size = %d", localeSymbolTable.fun.size());
#endif
    while (index < function->body_token_count - 1) {
        if (generateStatement(index, pitchTable, constantPool, localeSymbolTable.fun, currentProgram, all_function_count,
                              function, symbols.at(0), symbols, localeScopeIndex, proc, procIndex, proc->stackSize,
                              localVarSize, err))
            return NULL;
        index++;
    }
    proc->instructionSize = proc->instructions.size();
    function->proc = proc;
    return proc;
}
static int generateStatement(int& index, FunCallPitchTable& pitchTable, ConstantPool* constantPool,
                             std::vector<IR_Function*>& all_functions, IR_Program* currentProgram, int all_function_count,
                             IR_Function* function, SymbolTable& localeSymbolTable,
                             std::vector<SymbolTable>& outsideScopes /*块内与块外的并集,localeSymbolTable包含其中*/,
                             int localeScopeIndex, Procedure* proc, int procIndex, uint32_t& stackSize, uint32_t& localVarSize,
                             int* err) {
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
            generateStatement(index, pitchTable, constantPool, all_functions, currentProgram, all_function_count, function,
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
        if (proc->instructions.back().opcode == OP_STORE_VAR) {
            Instruction popInst = {};
            popInst.opcode = OP_POP;
#ifdef HX_DEBUG
            log(L"手动弹出赋值");
#endif
            int tmp = proc->instructions.size() - 1;
            for (int i = tmp; i >= 0; i--) {
                if (proc->instructions.back().opcode != OP_STORE_VAR) break;
                proc->instructions.push_back(popInst);
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
#ifdef HX_DEBUG
                        log("index+1: %ls,   index+2: %ls", function->bodyTokens[index + 1].value,
                            function->bodyTokens[index + 2].value);
#endif
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
        if (function->bodyTokens[index].type == TOK_OPR_SET) {
            isInit = true;
            if (newVar.isTypeKnown) {  //  <=> 已显示声明类型
            }
            newVar.isTypeKnown = true;
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
        uint32_t varSize = (uint32_t)getVarSize(newVar.type, currentProgram->classes, currentProgram->class_count);
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
        uint32_t varSize = (uint32_t)getVarSize(newVar.type, currentProgram->classes, currentProgram->class_count);
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
        generateStatement(index, pitchTable, constantPool, all_functions, currentProgram, all_function_count, function,
                          localeSymbolTable, outsideScopes, localeScopeIndex, proc, procIndex, stackSize, localVarSize, err);
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
        generateStatement(index, pitchTable, constantPool, all_functions, currentProgram, all_function_count, function,
                          localeSymbolTable, outsideScopes, localeScopeIndex, proc, procIndex, stackSize, localVarSize, err);
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
        generateStatement(index, pitchTable, constantPool, all_functions, currentProgram, all_function_count, function,
                          localeSymbolTable, outsideScopes, localeScopeIndex, proc, procIndex, stackSize, localVarSize, err);
        if (jmpAddr == (uint32_t)(proc->instructions.size())) {  // 用户写了空语句，那就塞个NOP
            Instruction NOPInst = {};
            proc->instructions.push_back(NOPInst);
        }
        proc->instructions.at(jmpInstIndex).params[1].type = PARAM_TYPE_INDEX;
        uint32_t falseJmpAddr = (uint32_t)proc->instructions.size();
        Instruction NOPInst = {};  // 塞NOP防越界
        proc->instructions.push_back(NOPInst);
        memcpy(proc->instructions.at(jmpInstIndex).params[1].value, &falseJmpAddr, sizeof(uint32_t));
    }
    return 0;
}
void generateInstructionsFromAST(std::vector<Instruction>& instructions, int* inst_index, int* inst_size, ASTNode* node,
                                 ConstantPool* constantPool, std::vector<SymbolTable>& symbols, int procIndex, int* err) {
#ifdef HX_DEBUG
    log(L"生成表达式相关指令");
#endif
    if (!inst_index || !node || !inst_size || !err || !constantPool) {
        if (err) *err = -1;
        return;
    }
#ifdef HX_DEBUG
    printAST(node);
#endif
    Instruction newInst = {};
    if (node->kind == NODE_VALUE) {
        newInst.opcode = OP_LOAD_CONST;
        // 设置参数
        if (node->data.value.type.kind == IR_DT_INT) {
            newInst.params[0].type = PARAM_TYPE_INT;
            memcpy(newInst.params[0].value, &(node->data.value.val.i), sizeof(int32_t));
            newInst.params[0].size = sizeof(int32_t);
            newInst.params[1].type = PARAM_TYPE_INT;
        } else if (node->data.value.type.kind == IR_DT_FLOAT) {
            newInst.params[0].type = PARAM_TYPE_FLOAT;
            memcpy(newInst.params[0].value, &(node->data.value.val.f), sizeof(double));
            newInst.params[0].size = sizeof(double);
            newInst.params[1].type = PARAM_TYPE_FLOAT;

        } else if (node->data.value.type.kind == IR_DT_CHAR) {
            newInst.params[0].type = PARAM_TYPE_CHAR;
            memcpy(newInst.params[0].value, &(node->data.value.val.c), sizeof(uint16_t));
            newInst.params[0].size = sizeof(uint16_t);
        } else if (node->data.value.type.kind == IR_DT_STRING) {
            // 加入常量池
            constantPool->constants = (Constant*)realloc(constantPool->constants, sizeof(Constant) * (constantPool->size + 1));
            if (!constantPool->constants) {
                *err = -1;
                return;
            }
            constantPool->constants[constantPool->size].type = CONST_STRING;
            constantPool->constants[constantPool->size].value.string_value =
                (wchar_t*)calloc(wcslen(node->data.value.val.s) + 1, sizeof(wchar_t));
            if (!constantPool->constants[constantPool->size].value.string_value) {
                *err = -1;
                return;
            }
            wcscpy(constantPool->constants[constantPool->size].value.string_value, node->data.value.val.s);
            constantPool->constants[constantPool->size].size = (uint16_t)wcslen(node->data.value.val.s) * sizeof(uint16_t);
            constantPool->size += 1;
            newInst.params[0].type = PARAM_TYPE_INDEX;
            uint32_t strIndex = constantPool->size - 1;
            memcpy(newInst.params[0].value, &(strIndex), sizeof(uint32_t));
            newInst.params[0].size = sizeof(uint32_t);
        } else {
            *err = -1;
            return;
        }
        (*inst_index)++;
        // 类型转换
        /*if(node->typeCast != OP_NOP) {
            Instruction typeCastInst = {};
            typeCastInst.opcode = node->typeCast;
            instructions.push_back(typeCastInst);
            (*inst_index)++;
            (*inst_size)++;
        }*/
    } else if (node->kind == NODE_BINARY && node->data.binary.op == BIN_OPR_SET) {  // 赋值
#ifdef HX_DEBUG
        log(L"分析赋值语句");
#endif
        if ((node->left->kind == NODE_BINARY && node->left->data.binary.op != BIN_OPR_SET)  // 不能给除赋值表达式以外的赋值
            || (node->left->kind == NODE_VALUE)) {
            *err = 255;
            setError(ERR_EXP, node->token->line, NULL);
            return;
        }
        int expInstBegin = *inst_index;
        // 只生成右子树指令
        generateInstructionsFromAST(instructions, inst_index, inst_size, node->right, constantPool, symbols, procIndex, err);
        if (*err != 0) return;
        newInst.opcode = OP_STORE_VAR;
        newInst.params[0].type = PARAM_TYPE_OFFEST;
        // 先不修改值，标记完无用变量后再改
        newInst.params[0].size = sizeof(uint32_t);
        newInst.params[1].size = sizeof(uint32_t);
        newInst.params[1].type = PARAM_TYPE_SIZE;
        instructions.push_back(newInst);
        // 关联需要将右值表达式所有相关指令加上
        for (int i = symbols.size() - 1; i >= 0; i--) {
            int varIndex = -1;
            if ((varIndex = getVarIndex(node->left->data.var.name, &symbols.at(i))) != -1) {
                for (int j = expInstBegin; j < instructions.size(); j++) {
#ifdef HX_DEBUG
                    log(L"将索引为%d指令与第%"
                        L"d作用域的変量%ls关联",
                        j, i, symbols.at(i).vars[varIndex].name);
#endif
                    symbols.at(i).vars[varIndex].instIndex.push_back(j);
                }
            }
        }
        // 关联指令
#ifdef HX_DEBUG
        log(L"关联指令STORE_VAR 目标变量名：%ls", node->data.binary.varName ? node->data.binary.varName : L"(null)");
#endif
        for (int i = symbols.size() - 1; i >= 0; i--) {
            int varIndex = -1;
            if ((varIndex = getVarIndex(node->data.binary.varName, &symbols.at(i))) != -1) {
#ifdef HX_DEBUG
                log(L"将指令OP_STORE_VAR(第%d)与第%"
                    L"d作用域的変量%ls关联",
                    *inst_index, i, symbols.at(i).vars[varIndex].name);
#endif
                symbols.at(i).vars[varIndex].instIndex.push_back(*inst_index);
            }
        }
        (*inst_index)++;
        (*inst_size)++;
        return;
    } else if (node->kind == NODE_UNARY && node->data.unary.op == UAY_OPR_INC) {  // 自增
        newInst.opcode = OP_INC;
        newInst.params[0].type = PARAM_TYPE_OFFEST;
        newInst.params[0].size = sizeof(uint32_t);
        switch (node->resultType.kind) {
            case IR_DT_INT: {
                newInst.params[1].type = PARAM_TYPE_INT;
                break;
            }
            case IR_DT_FLOAT: {
                newInst.params[1].type = PARAM_TYPE_FLOAT;
                break;
            }
            case IR_DT_CHAR: {
                newInst.params[1].type = PARAM_TYPE_CHAR;
                break;
            }
            default:
                setError(ERROR_INC_OR_DEC_OP_VAR, node->token->line, NULL);
                *err = 255;
                return;
        }
        instructions.push_back(newInst);

#ifdef HX_DEBUG
        log(L"关联指令INC 目标变量名：%ls", node->data.unary.varName ? node->data.unary.varName : L"(null)");
#endif
        for (int i = symbols.size() - 1; i >= 0; i--) {
            int varIndex = -1;
            if ((varIndex = getVarIndex(node->data.unary.varName, &symbols.at(i))) != -1) {
#ifdef HX_DEBUG
                log(L"将指令INC(第%d)与第%"
                    L"d作用域的変量%ls关联",
                    *inst_index, i, symbols.at(i).vars[varIndex].name);
#endif
                symbols.at(i).vars[varIndex].instIndex.push_back(*inst_index);
            }
        }
        (*inst_index)++;
        (*inst_size)++;
        return;
    } else if (node->kind == NODE_UNARY && node->data.unary.op == UAY_OPR_DIC) {  // 自减
        newInst.opcode = OP_DEC;
        newInst.params[0].type = PARAM_TYPE_OFFEST;
        newInst.params[0].size = sizeof(uint32_t);
        switch (node->resultType.kind) {
            case IR_DT_INT: {
                newInst.params[1].type = PARAM_TYPE_INT;
                break;
            }
            case IR_DT_FLOAT: {
                newInst.params[1].type = PARAM_TYPE_FLOAT;
                break;
            }
            case IR_DT_CHAR: {
                newInst.params[1].type = PARAM_TYPE_CHAR;
                break;
            }
            default:
                setError(ERROR_INC_OR_DEC_OP_VAR, node->token->line, NULL);
                *err = 255;
                return;
        }
        instructions.push_back(newInst);

#ifdef HX_DEBUG
        log(L"关联指令OP_DEC 目标变量名：%ls", node->data.unary.varName ? node->data.unary.varName : L"(null)");
#endif
        for (int i = symbols.size() - 1; i >= 0; i--) {
            int varIndex = -1;
            if ((varIndex = getVarIndex(node->data.unary.varName, &symbols.at(i))) != -1) {
#ifdef HX_DEBUG
                log(L"将指令OP_DEC(第%d)与第%"
                    L"d作用域的変量%ls关联",
                    *inst_index, i, symbols.at(i).vars[varIndex].name);
#endif
                symbols.at(i).vars[varIndex].instIndex.push_back(*inst_index);
            }
        }
        (*inst_index)++;
        (*inst_size)++;
        return;
    } else if (node->kind == NODE_BINARY) {
        // 生成左子树指令
        generateInstructionsFromAST(instructions, inst_index, inst_size, node->left, constantPool, symbols, procIndex, err);
        if (*err != 0) {
            return;
        }
        // 生成右子树指令
        generateInstructionsFromAST(instructions, inst_index, inst_size, node->right, constantPool, symbols, procIndex, err);
        if (*err != 0) {
            return;
        }
        // 生成二元运算指令
        switch (node->data.binary.op) {
            case BIN_OPR_ADD:  // ADD
                newInst.opcode = OP_ADD;
                break;
            case BIN_OPR_SUB:  // SUB
                newInst.opcode = OP_SUB;
                break;
            case BIN_OPR_MUL:  // MUL
                newInst.opcode = OP_MUL;
                break;
            case BIN_OPR_DIV:  // DIV
                newInst.opcode = OP_DIV;
                break;
            case BIN_OPR_STRING_CONCAT: {  // STRING_CONCAT
                newInst.opcode = OP_STRING_CONCAT;
                break;
            }
            case BIN_OPR_EQU:
                newInst.opcode = OP_EQU;
                break;
            case BIN_OPR_NEQU:
                newInst.opcode = OP_NEQU;
                break;
            case BIN_OPR_GT:
                newInst.opcode = OP_GT;
                break;
            case BIN_OPR_LT:
                newInst.opcode = OP_LT;
                break;
            case BIN_OPR_AND:
                newInst.opcode = OP_AND;
                break;
            case BIN_OPR_AND_LOGIC:
                newInst.opcode = OP_AND_LOGIC;
                break;
            case BIN_OPR_OR:
                newInst.opcode = OP_OR;
                break;
            case BIN_OPR_OR_LOGIC:
                newInst.opcode = OP_OR_LOGIC;
                break;
            default:
                *err = -1;
                return;
        }
        (*inst_index)++;
    } else if (node->kind == NODE_VAR) {
        newInst.opcode = OP_LOAD_VAR;
        // 回填策略：
        /*将newInst.params[0]临时设为变量索引，后头回填时验证索引是否贴合再改*/
        newInst.params[0].type = PARAM_TYPE_OFFEST;
        newInst.params[0].size = sizeof(uint32_t);
        uint32_t index = (uint32_t)node->data.var.index;
        memcpy(newInst.params[0].value, &index, sizeof(uint32_t));
        // 后面再回填
        newInst.params[1].type = PARAM_TYPE_SIZE;
        newInst.params[1].size = sizeof(uint32_t);
        switch (node->data.var.type.kind) {
            case IR_DT_INT:
                newInst.params[1].type = PARAM_TYPE_INT;
                break;
            case IR_DT_FLOAT:
                newInst.params[1].type = PARAM_TYPE_FLOAT;
                break;
            case IR_DT_CHAR:
                newInst.params[1].type = PARAM_TYPE_CHAR;
                break;
            case IR_DT_STRING:
                newInst.params[1].type = PARAM_TYPE_STRING;
                break;
        }
            // 关联指令
#ifdef HX_DEBUG
        log(L"关联指令LOAD_VAR");
#endif
        for (int i = symbols.size() - 1; i >= 0; i--) {
            int varIndex = -1;
            if ((varIndex = getVarIndex(node->data.var.name, &symbols.at(i))) != -1) {
#ifdef HX_DEBUG
                log(L"将指令OP_LOAD_VAR(第%d)与第%"
                    L"d作用域的変量%ls关联",
                    *inst_index, i, symbols.at(i).vars[varIndex].name);
#endif
                symbols.at(i).vars[varIndex].instIndex.push_back(*inst_index);
                symbols.at(i).vars[varIndex].isUsed = true;
            }
        }
        (*inst_index)++;
        instructions.push_back(newInst);
        (*inst_size)++;
        if (node->typeCast != OP_NOP) {
#ifdef HX_DEBUG
            log(L"生成强制转换指令");
#endif
            Instruction castInst = {};
            castInst.opcode = node->typeCast;
            instructions.push_back(castInst);
            (*inst_index)++;
            (*inst_size)++;
        }
        return;
    } else if (node->kind == NODE_FUN_CALL) {
        if (node->data.funCall.arg_count == 0) {
        } else {
            // 生成参数指令
            for (uint32_t i = 0; i < node->data.funCall.arg_count; i++) {
                generateInstructionsFromAST(instructions, inst_index, inst_size, node->data.funCall.args[i], constantPool,
                                            symbols, procIndex, err);
                if (*err != 0) {
                    return;
                }
            }
        }
        // 生成调用指令
        newInst.opcode = OP_CAL;
        newInst.params[0].type = PARAM_TYPE_INDEX;
        /*******(*********)***********************************************
         *                  先设置pitch，后面再回填(pitch在Parser已添加))
         *********************************-*****--**********************/
        newInst.params[0].size = sizeof(uint32_t);
        newInst.pitch = node->data.funCall.pitch;

        newInst.params[1].type = PARAM_TYPE_INT;
        memcpy(newInst.params[1].value, &(node->data.funCall.arg_count), sizeof(uint32_t));
        newInst.params[1].size = sizeof(uint32_t);
        (*inst_index)++;
    } else {
        *err = -1;
        return;
    }
    instructions.push_back(newInst);
    (*inst_size)++;
    return;
}
extern void freeObjectCode(ObjectCode** obj) {
    if (!obj || !(*obj)) return;
    // 释放常量池
    if ((*obj)->constantPool.constants) {
        for (int i = 0; i < (*obj)->constantPool.size; i++) {
            if ((*obj)->constantPool.constants[i].value.string_value) {
                free((*obj)->constantPool.constants[i].value.string_value);
                (*obj)->constantPool.constants[i].value.string_value = NULL;
            }
        }
        free((*obj)->constantPool.constants);
        (*obj)->constantPool.constants = NULL;
    }
    for (int i = 0; i < (*obj)->procedures.size(); i++) {
        if ((*obj)->procedures.at(i)) {
            delete (*obj)->procedures.at(i);
            (*obj)->procedures.at(i) = NULL;
        }
    }
    delete *obj;
    *obj = NULL;
}
#endif