#ifndef HXHLANG_SRC_HXC_OBJECTCODE_H
#define HXHLANG_SRC_HXC_OBJECTCODE_H
#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>
#include <wchar.h>

#include <cstdio>
#include <string>

typedef uint8_t Opcode;
enum {
    OP_NOP = 0,
    OP_LOAD_CONST,  // 加载常量至栈顶 OP_LOAD_CONST <paramType> <paramValue>
    // |
    // OP_LOAD_CONST <constantIndex>
    OP_LOAD_VAR,             // 加载变量至栈顶  LOAD_VAR <offest(u32)> <size(u32)(type为压栈后槽位标记的类型))>
    OP_STORE_ARRAY_ELEMENT,  // 将栈顶值存入数组元素, 索引用栈顶  STORE_ARRAY_ELEMENT <offest(u32)> <size(按u32读>
    OP_LOAD_ELEMENT_FROM_ARRAY,  // 加载数组元素至栈顶， 索引用栈顶  LOAD_ELEMENT_FROM_ARRAY <offest(u32)> <size(按u32读，
                                 // type为压栈后槽位标记的类型)>
    OP_POP,        // 弹出
    OP_STORE_VAR,  // 将栈顶值存入变量  OP_STORE_VAR <offest(u32)>
    // <copySize(u32, type表示栈顶应转换的类型)>

    OP_ADD,
    OP_SUB,
    OP_MUL,
    OP_DIV,   // 次栈顶除栈顶
    OP_EQU,   // 必须知道两参数的类型
    OP_NEQU,  // 必须知道两参数的类型
    OP_GT,    // 次栈顶 > 栈顶  -> true
    OP_LT,    // 次栈顶 < 栈顶  -> true
    OP_AND,
    OP_OR,
    OP_NOT,
    OP_AND_LOGIC,
    OP_OR_LOGIC,
    OP_NOT_LOGIC,
    OP_INC,  // INC <offest> <varType>
    OP_DEC,

    OP_JMP,  // OP_JMP <instAddr(u32)>
    // JMP_CONDITION <栈顶为真时跳转的地址(index u32)> <为假时跳转的地址(index u32)>
    OP_JMP_CONDITION,
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

    OP_HEAP_ALLOC,  // OP_HEAP_ALLOC size(u32)：分配内存的地址放栈顶

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
    int offest = 0;  // 偏移量增加的量
    int sizeAdd = 0; // 大小增加的量
} Param;
// 指令
typedef struct Instruction {
    bool isNotUsed;  // 为true的指令将不会写入
    Opcode opcode;   // char
    Param params[3];
    FunCallPitch* pitch;  // 回填，仅OP_CAL使用,不写入文件
} Instruction;
// 过程,用索引访问
typedef struct Procedure {
    bool isUsed = false;      // 这个变量不会写入
    IR_Function* fun = NULL;  // 这个变量也不会写入

    uint32_t instructionSize = 0;
    std::vector<Instruction> instructions;
    uint32_t stackSize = 0;  // 栈大小
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
    uint32_t size = 0;
    Constant* constants = NULL;
} ConstantPool;
//----------------------------------
typedef struct ObjectCodeHeader {
    char magic[4];  // 魔数 "HXOC"
    float version = 0.0f;
} ObjectCodeHeader;
//--------------------------------------
typedef struct ObjectCode {
    ObjectCodeHeader header;
    ConstantPool constantPool;
    uint32_t procedureSize = 0;
    std::vector<Procedure*> procedures;
    int32_t start = 0;  // 入口索引
} ObjectCode;
//--------------------------------------
// 写入目标代码
extern int writeObjectCode(FILE* objFile, ObjectCode& obj) noexcept;

static int writeHeader(FILE* file) noexcept {
#ifdef HX_DEBUG
    log(L"写入文件头");
#endif
    ObjectCodeHeader header = {};
    header.magic[0] = 'H';
    header.magic[1] = 'X';
    header.magic[2] = 'O';
    header.magic[3] = 'C';
    header.version = HXC_VERSION;
    if (fwrite(&(header.magic), sizeof(header.magic), 1, file) != 1) return -1;
    if (fwrite(&(header.version), sizeof(header.version), 1, file) != 1) return -1;
    return 0;
}
// 存的是真实大小
/************************
    |------------------|
    |     size(u32     |
    |------------------|
    |  value[0](u16)   |
    |------------------|
    |  value[1](u16)   |
    ..................
*************************/
static int writeWstring(const wchar_t* wstr, FILE* file) noexcept {
    if (!wstr) {
        uint32_t byteLen = 0;
        return fwrite(&byteLen, sizeof(byteLen), 1, file) == 1 ? 0 : -1;
    }

    uint32_t len = wcslen(wstr);
    uint32_t byteLen = len * sizeof(uint16_t);  // 不含 \0
    if (fwrite(&byteLen, sizeof(byteLen), 1, file) != 1) return -1;

    for (uint32_t i = 0; i < len; i++) {
        uint16_t cp = (uint16_t)wstr[i];
        if (fwrite(&cp, sizeof(cp), 1, file) != 1) return -1;
    }
    return 0;
}
static int writeParam(Param& param, FILE* file) noexcept {
    // 写type
    char type = (char)(param.type);
    if (fwrite(&(type), sizeof(char), 1, file) != 1) return -1;
    // size
    if (fwrite(&(param.size), sizeof(uint8_t), 1, file) != 1) return -1;
    // value
#ifdef HX_DEBUG
    // log(L"%d", *((int32_t*)(param.value)));
#endif
    if (fwrite(&(param.value), sizeof(param.value), 1, file) != 1) return -1;
    // 偏移量
    if (fwrite(&(param.offest), sizeof(uint32_t), 1, file) != 1) return -1;
    return 0;
}
static int writeInstruction(Instruction& inst, FILE* file) {
    if (inst.isNotUsed) {
#ifdef HX_DEBUG
        log(L"指令无用,跳过");
#endif
        return 0;
    }
#ifdef HX_DEBUG
    fwprintf(logStream, L"写入指令");
    switch (inst.opcode) {
        case OP_LOAD_CONST: {
            fwprintf(logStream, L"\33[1;34mOP_LOAD_CONST\33[0m)\n");
            break;
        }
        case OP_PRINT_STRING:
            fwprintf(logStream, L"\33[1;34mOP_PRINT_STRING\33[0m\n");
            break;
        case OP_LOAD_VAR:
            fwprintf(logStream, L"\33[1;34mOP_LOAD_VAR\33[0m)\n");
            break;
        case OP_HEAP_ALLOC:
            fwprintf(logStream, L"\33[1;34mOP_HEAP_ALLOC\33[0m)\n");
            break;
        case OP_STORE_VAR:
            fwprintf(logStream, L"\33[1;34mOP_STORE_VAR\33[0m)\n");
            break;
        case OP_ADD:
            fwprintf(logStream, L"\33[1;34mOP_ADD\33[0m)\n");
            break;
        case OP_SUB:
            fwprintf(logStream, L"\33[1;34mOP_SUB\33[0m)\n");
            break;
        case OP_MUL:
            fwprintf(logStream, L"\33[1;34mOP_MUL\33[0m)\n");
            break;
        case OP_DIV:
            fwprintf(logStream, L"\33[1;34mOP_DIV\33[0m)\n");
            break;
        case OP_CAL:
            fwprintf(logStream, L"\33[1;34mOP_CAL\33[0m)\n");
            break;
        case OP_RET:
            fwprintf(logStream, L"\33[1;34mOP_RET\33[0m)\n");
            break;
        case OP_CHAR_TO_INT:
            fwprintf(logStream, L"\33[1;34m OP_CHAR_TO_INT\33[0m\n");
            break;
        case OP_INT_TO_CHAR:
            fwprintf(logStream, L"\33[1;34m OP_INT_TO_CHAR\33[0m\n");
            break;
        case OP_INT_TO_FLOAT:
            fwprintf(logStream, L"\33[1;34m OP_INT_TO_FLOAT\33[0m\n");
            break;
        case OP_CHAR_TO_FLOAT:
            fwprintf(logStream, L"\33[1;34m OP_CHAR_TO_FLOAT\33[0m\n");
            break;
        case OP_CHAR_TO_STRING:
            fwprintf(logStream, L"\33[1;34m OP_CHAR_TO_STRING\33[0m\n");
            break;
        case OP_FLOAT_TO_INT:
            fwprintf(logStream, L"\33[1;34m OP_FLOAT_TO_INT\33[0m\n");
            break;
        case OP_INT_TO_STRING:
            fwprintf(logStream, L"\33[1;34m OP_INT_TO_STRING\33[0m\n");
            break;
        case OP_POP:
            fwprintf(logStream, L"\33[1;34m OP_POP\33[0m\n");
            break;
        case OP_JMP:
            fwprintf(logStream, L"\33[1;34m OP_JMP\33[0m\n");
            break;
        case OP_JMP_CONDITION:
            fwprintf(logStream, L"\33[1;34m OP_JMP_CONDITION\33[0m\n");
            break;
        case OP_EQU:
            fwprintf(logStream, L"\33[1;34m OP_EQU\33[0m\n");
            break;
        case OP_OR:
            fwprintf(logStream, L"\33[1;34m OP_OR\33[0m\n");
            break;
        case OP_LT:
            fwprintf(logStream, L"\33[1;34m OP_LT\33[0m\n");
            break;
        case OP_GT:
            fwprintf(logStream, L"\33[1;34m OP_GT\33[0m\n");
            break;
        case OP_OR_LOGIC:
            fwprintf(logStream, L"\33[1;34m OP_OR_LOGIC\33[0m\n");
            break;
        case OP_AND:
            fwprintf(logStream, L"\33[1;34m OP_AMD\33[0m\n");
            break;
        case OP_AND_LOGIC:
            fwprintf(logStream, L"\33[1;34m OP_AMD_LOGIC\33[0m\n");
            break;
        case OP_NOT:
            fwprintf(logStream, L"\33[1;34m OP_NOT\33[0m\n");
            break;
        case OP_NOT_LOGIC:
            fwprintf(logStream, L"\33[1;34m OP_NOT_LOGIC\33[0m\n");
            break;
        case OP_INC:
            fwprintf(logStream, L"\33[1;34m OP_INC\33[0m\n");
            break;
        case OP_DEC:
            fwprintf(logStream, L"\33[1;34m OP_DEC\33[0m\n");
            break;
        case OP_STORE_ARRAY_ELEMENT:
            fwprintf(logStream, L"\33[1;34m OP_STORE_ARRAY_ELEMENT\33[0m\n");
            break;
        case OP_LOAD_ELEMENT_FROM_ARRAY:
            fwprintf(logStream, L"\33[1;34m OP_LOAD_ELEMENT_FROM_ARRAY\33[0m\n");
            break;
        default:
            fwprintf(logStream, L"\33[1;32mOP_NOP\33[0m)\n");
    }
#endif
    // 写opcode
    if (fwrite(&(inst.opcode), sizeof(Opcode), 1, file) != 1) return -1;
    // param
    for (int i = 0; i < 3; i++) {
        if (writeParam((inst.params[i]), file)) return -1;
    }
    return 0;
}
static int writeProcedure(Procedure& proc, FILE* file) noexcept {
    // 过滤出真正需要写入的指令
    std::vector<Instruction> validInstructions;
    for (size_t i = 0; i < proc.instructions.size(); i++) {
        if (!proc.instructions.at(i).isNotUsed) {
            validInstructions.push_back(proc.instructions.at(i));
        }
    }

    // 更新并写入正确的指令数量
    proc.instructionSize = static_cast<uint32_t>(validInstructions.size());
#ifdef HX_DEBUG
    log(L"写 instructionSize:%d", proc.instructionSize);
#endif
    if (fwrite(&(proc.instructionSize), sizeof(uint32_t), 1, file) != 1) return -1;

    // 严格写入对应数量的有效指令
    for (size_t i = 0; i < validInstructions.size(); i++) {
        if (writeInstruction(validInstructions[i], file)) return -1;
    }
    // stackSize
#ifdef HX_DEBUG
    log(L"写stackSize:%d", proc.stackSize);
#endif
    if (fwrite(&(proc.stackSize), sizeof(uint32_t), 1, file) != 1) return -1;
    return 0;
}
int writeObjectCode(FILE* objFile, ObjectCode& obj) noexcept {
    if (!objFile) return -1;
    if (writeHeader(objFile)) return -1;
    // 写ConstantPoolSize
    if (fwrite(&(obj.constantPool.size), sizeof(uint32_t), 1, objFile) != 1) return -1;
    // 写ConstantPool.constants
    for (int i = 0; i < obj.constantPool.size; i++) {
        // 写tyoe
        char type = (char)(obj.constantPool.constants[i].type);
        if (fwrite(&(type), sizeof(char), 1, objFile) != 1) return -1;
        if (obj.constantPool.constants[i].type == CONST_STRING) {
            if (writeWstring(obj.constantPool.constants[i].value.string_value, objFile)) return -1;
        }
    }
    // ProcedureSize
    if (fwrite(&(obj.procedureSize), sizeof(uint32_t), 1, objFile) != 1) return -1;
    // procedures
    for (int i = 0; i < obj.procedureSize; i++) {
#ifdef HX_DEBUG
        log(L"写入过程%d", i);
#endif
        if (writeProcedure(*(obj.procedures.at(i)), objFile)) return -1;
    }
    // 入口索引
    if (fwrite(&(obj.start), sizeof(uint32_t), 1, objFile) != 1) return -1;
    fclose(objFile);
    return 0;
}
#endif