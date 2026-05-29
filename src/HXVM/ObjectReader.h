#ifndef HXHLANG_SRC_HXVM_OBJECT_READER_H
#define HXHLANG_SRC_HXVM_OBJECT_READER_H
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <wchar.h>

#include <string>

#include "HxVector.h"
typedef uint8_t Opcode;
enum {
    OP_NOP = 0,
    OP_LOAD_CONST,  // 加载常量至栈顶 OP_LOAD_CONST <paramType> <paramValue>
    // |
    // OP_LOAD_CONST <constantIndex>
    OP_LOAD_VAR,   // 加载变量至栈顶
    OP_POP,        // 弹出
    OP_STORE_VAR,  // 将栈顶值存入变量  OP_STORE_VAR <offest(u32)>
    // <copySize(u32)>
    OP_ADD,
    OP_SUB,
    OP_MUL,
    OP_DIV,
    OP_JMP,            // OP_JMP <instAddr(u32)>
    OP_JMP_CONDITION,  // JMP_CONDITION <栈顶为真时跳转的地址>
    // <为假时跳转的地址(>size时跳转至末尾)>
    OP_CAL,  // CAL <procIndex>(u32) <paramCount>(u32)
    OP_RET,
    OP_PRINT_STRING,
    // 类型转换
    OP_CHAR_TO_INT,
    OP_INT_TO_CHAR,
    OP_INT_TO_FLOAT,
    OP_CHAR_TO_FLOAT,
    OP_CHAR_TO_STRING,
    OP_FLOAT_TO_INT,
    OP_INT_TO_STRING,
    // 连接字符串
    OP_STRING_CONCAT,
};
typedef uint8_t ParamType;
enum {
    PARAM_TYPE_INT = 0,
    PARAM_TYPE_FLOAT,  // double
    PARAM_TYPE_CHAR,
    PARAM_TYPE_BOOL,
    PARAM_TYPE_STRING,
    PARAM_TYPE_ADDRESS,
    PARAM_TYPE_INDEX,   // uint32_t 索引常量池或过程表
    PARAM_TYPE_SIZE,    // u32
    PARAM_TYPE_OFFEST,  // u32
};
typedef struct Param {
    ParamType type;  // char
    uint8_t size;
    char value[8];
    uint32_t offest;  // 偏移量
} Param;
// 指令
typedef struct Instruction {
    Opcode opcode;  // char
    Param params[3];
} Instruction;
// 过程,用索引访问
typedef struct Procedure {
    uint32_t instructionSize;
    HxVector<Instruction> instructions;
    uint32_t stackSize;     // 栈大小
    uint32_t localVarSize;  // 局部变量数量
} Procedure;
//------------------------------------
// 常量池
enum ConstantType {
    CONST_STRING,
};
typedef struct Constant {
    ConstantType type;  // char, 1字节
    uint32_t size;      // 真实大小，不是字符串长度
    union {
        wchar_t* string_value;
    } value;
} Constant;
typedef struct ConstantPool {
    uint32_t size;
    Constant* constants;
} ConstantPool;
//----------------------------------
typedef struct ObjectCodeHeader {
    char magic[4];  // 魔数 "HXOC"
    float version;
    uint8_t isInDebugMode;
} ObjectCodeHeader;
//--------------------------------------
typedef struct ObjectCode {
    ObjectCodeHeader header;
    ConstantPool constantPool;
    uint32_t procedureSize;
    HxVector<Procedure> procedures;
    int32_t start;  // 入口索引
} ObjectCode;

inline static wchar_t* readWstring(FILE* file) {
    uint32_t byteLen;
    if (fread(&byteLen, sizeof(byteLen), 1, file) != 1) return nullptr;
    if (byteLen == 0) return nullptr;

    uint32_t charCount = byteLen / sizeof(uint16_t);

    uint16_t* buf = (uint16_t*)malloc(byteLen+sizeof(uint16_t));
    if (!buf) return nullptr;

    if (fread(buf, sizeof(uint16_t), charCount, file) != charCount) {
        free(buf);
        return nullptr;
    }

    wchar_t* wstr = (wchar_t*)calloc(charCount+1, sizeof(wchar_t));
    if (!wstr) {
        free(buf);
        return nullptr;
    }

    for (uint32_t i = 0; i < charCount; i++) {
        wstr[i] = (wchar_t)buf[i];
    }

    free(buf);
    return wstr;
}
// 读取指令
inline static int readInstruction(Instruction& instr, FILE* file) {
    // 读取 Opcode
    if (fread(&(instr.opcode), sizeof(Opcode), 1, file) != 1) return -1;
    // 读取 3 个参数
    for (int i = 0; i < 3; i++) {
        char typeChar;
        if (fread(&typeChar, sizeof(char), 1, file) != 1) return -1;
        instr.params[i].type = (ParamType)typeChar;

        if (fread(&(instr.params[i].size), sizeof(uint8_t), 1, file) != 1) return -1;
        if (fread(instr.params[i].value, 1, 8, file) != 8) return -1;
        if (fread(&(instr.params[i].offest), sizeof(uint32_t), 1, file) != 1) return -1;
    }
    return 0;
}
inline int readObjectCode(FILE* file, ObjectCode& obj) {
    if (!file) return -1;

    // 验证头信息 "HXOC"
    char magic[4];
    if (fread(magic, 1, 4, file) != 4) {
        fclose(file);
        return -1;
    }
    if (magic[0] != 'H' || magic[1] != 'X' || magic[2] != 'O' || magic[3] != 'C') {
        fclose(file);
        return -1;
    }
    float version = 0.0f;
    if (fread(&version, sizeof(float), 1, file) != 1) {
        fclose(file);
        return -1;
    }
    if (version > HXVM_VERSION) {
        fwprintf(errorStream, ERR_LABEL L"本虚拟机版本过低，文件要求：%f\n", version);
        fclose(file);
        return -1;
    }
    uint8_t isInDebugMode = false;
    if (fread(&isInDebugMode, sizeof(uint8_t), 1, file) != 1) {
        fclose(file);
        return -1;
    }

    // 读取常量池
    if (fread(&(obj.constantPool.size), sizeof(uint32_t), 1, file) != 1) {
        fclose(file);
        return -1;
    }
    obj.constantPool.constants = (Constant*)malloc(sizeof(Constant) * obj.constantPool.size);
    for (uint32_t i = 0; i < obj.constantPool.size; i++) {
        char typeChar;
        if (fread(&typeChar, sizeof(char), 1, file) != 1) {
            fclose(file);
            return -1;
        }
        obj.constantPool.constants[i].type = (ConstantType)typeChar;
        if (obj.constantPool.constants[i].type == CONST_STRING) {
            // 将 u16 序列读入并转为 wchar_t*
            obj.constantPool.constants[i].value.string_value = readWstring(file);
            // 重新记录当前平台的真实字节大小
            if (obj.constantPool.constants[i].value.string_value) {
                obj.constantPool.constants[i].size =
                    (uint32_t)(wcslen(obj.constantPool.constants[i].value.string_value) * sizeof(wchar_t));
            }
        }
    }
    // 读取过程
    uint32_t procCount;
    if (fread(&procCount, sizeof(uint32_t), 1, file) != 1) {
        fclose(file);
        return -1;
    }
    obj.procedureSize = procCount;
    obj.procedures.resize(procCount);
    for (uint32_t i = 0; i < procCount; i++) {
        Procedure& proc = obj.procedures[i];
        if (fread(&(proc.instructionSize), sizeof(uint32_t), 1, file) != 1) {
            fclose(file);
            return -1;
        }
        for (uint32_t j = 0; j < proc.instructionSize; j++) {
            Instruction instr;
            if (readInstruction(instr, file) != 0) {
                fclose(file);
                return -1;
            }
            proc.instructions.push_back(instr);
        }
        // 读取运行栈和局部变量配置
        if (fread(&(proc.stackSize), sizeof(uint32_t), 1, file) != 1) {
            fclose(file);
            return -1;
        }
        if (fread(&(proc.localVarSize), sizeof(uint32_t), 1, file) != 1) {
            fclose(file);
            return -1;
        }
    }
    // 读取入口
    if (fread(&(obj.start), sizeof(uint32_t), 1, file) != 1) {
        fclose(file);
        return -1;
    }
    fclose(file);
    return 0;
}
inline void freeObjectCode(ObjectCode& obj) {
    // 释放常量池里的每个字符串
    if (obj.constantPool.constants != nullptr) {
        for (uint32_t i = 0; i < obj.constantPool.size; i++) {
            if (obj.constantPool.constants[i].type == CONST_STRING) {
                if (obj.constantPool.constants[i].value.string_value != nullptr) {
                    free(obj.constantPool.constants[i].value.string_value);
                }
            }
        }
        free(obj.constantPool.constants);
        obj.constantPool.constants = nullptr;
    }
    return;
}
#endif