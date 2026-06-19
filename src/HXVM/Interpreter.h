#ifndef HXHLANG_SRC_HXVM_INTERPRETER_H
#define HXHLANG_SRC_HXVM_INTERPRETER_H
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <wchar.h>

#include <atomic>
extern std::atomic<bool> shouldExit;
#include <cstdlib>

#include "HxVector.h"
#include "ObjectReader.h"

class HxMemoryAllocer {
   private:
    HxVector<void*> heapMemoryBlocks;
    HxVector<uint32_t> freeableMemIndex;
#ifdef HX_DEBUG
    size_t usedMemory;
#endif
   public:
    HxMemoryAllocer() {
#ifdef HX_DEBUG
        this->usedMemory = 0;
#endif
    }
    void markFreeable(void* mem) {
        for (uint32_t i = 0; i < this->heapMemoryBlocks.size(); i++) {
            if (this->heapMemoryBlocks[i] == mem) {
#ifdef HX_DEBUG
                wprintf(LOG_LABEL L"标记内存 %p\n", this->heapMemoryBlocks[i]);
#endif
                this->freeableMemIndex.push_back(i);
                return;
            }
        }
    }
    void gc() {
        for (int i = 0; i < freeableMemIndex.size(); i++) {
#ifdef HX_DEBUG
            wprintf(LOG_LABEL L"释放内存 %p\n", this->heapMemoryBlocks[freeableMemIndex[i]]);
#endif
            if (this->heapMemoryBlocks[freeableMemIndex[i]]) free(this->heapMemoryBlocks[freeableMemIndex[i]]);
            this->heapMemoryBlocks[freeableMemIndex[i]] = NULL;
        }
        while (!freeableMemIndex.empty()) {
            freeableMemIndex.pop_back();
        }
    }
    ~HxMemoryAllocer() {
        for (int i = 0; i < this->heapMemoryBlocks.size(); i++) {
            if (this->heapMemoryBlocks[i]) {
#ifdef HX_DEBUG
                wprintf(LOG_LABEL L"释放内存 %p\n", this->heapMemoryBlocks[i]);
#endif
                free(this->heapMemoryBlocks[i]);
                this->heapMemoryBlocks[i] = NULL;
            }
        }
    }
    void* hxMalloc(const unsigned int size) noexcept {
        if (size == 0) return NULL;
#ifdef HX_DEBUG
        wprintf(LOG_LABEL L"申请一块%u字节的内存\n", size);
#endif
        void* memory = calloc(size, sizeof(char));
        while (!memory && size != 0) {
            fwprintf(errorStream, ERR_LABEL "内存分配失败！重试中\n");
            memory = calloc(size, sizeof(char));
        }
#ifdef HX_DEBUG
        this->usedMemory += size;
#endif
        if (memory) this->heapMemoryBlocks.push_back(memory);
#ifdef HX_DEBUG
        wprintf(LOG_LABEL L"申请内存 %p\n", memory);
        wprintf(LOG_LABEL L"HxAllocr内存用量：%zu byte\n", this->usedMemory);
#endif
        return memory;
    }
    void* hxRealloc(void* old, int oldSize, int newSize) noexcept {
        void* memory = realloc(old, newSize);
#ifdef HX_DEBUG
        this->usedMemory = this->usedMemory - oldSize + newSize;
        wprintf(LOG_LABEL L"HxAllocr内存用量：%zu byte\n", this->usedMemory);
#endif
        if (!memory) return NULL;
        for (int i = 0; i < this->heapMemoryBlocks.size(); i++) {
            if (this->heapMemoryBlocks[i] == old) {
                this->heapMemoryBlocks[i] = memory;
                break;
            }
        }

        return memory;
    }
};
alignas(64) static thread_local HxMemoryAllocer memoryAllocer;

typedef struct Symbol Symbol;
typedef struct CallFrame {  // 栈帧
    Procedure* proc;
    int instIndex;  // 解释到哪了
    char* stack;
} CallFrame;
// 栈帧压栈
inline static int pushCallFrame(Procedure& proc, HxVector<CallFrame*>& frames) {
    CallFrame* frame = (CallFrame*)memoryAllocer.hxMalloc(sizeof(CallFrame));
    if (frame == NULL) return -1;
    frame->proc = &proc;
#ifdef HX_DEBUG
    wprintf(LOG_LABEL L"过程使用栈内存大小：%d\n", proc.stackSize);
#endif
    frame->stack = (char*)(memoryAllocer.hxMalloc(proc.stackSize));
    frame->instIndex = 0;
    if (frame->stack == NULL && proc.stackSize != 0) {
        fwprintf(errorStream, ERR_LABEL "内存分配失败！\n");
        return -1;
    }
    frames.push_back(frame);
    return 0;
}

typedef enum OpStackType {
    TYPE_INT = 1,
    TYPE_FLOAT,  // double
    TYPE_CHAR,
    TYPE_BOOL,
    TYPE_STRING,
    TYPE_ADDRESS,  // size = 4
} StackType;
typedef struct _OpStack {
    OpStackType type;
    int size;
    char value[8];  // type为string时存wchar_t*
} _OpStack;
typedef struct OpStack {
    _OpStack opStack[OP_STACK_SIZE];
    int top;
} OpStack;
typedef struct Symbol {
    OpStackType type;
    void* address;
} Symbol;
// 真假判定
inline static bool isOperandTrue(const _OpStack& op) noexcept {
    switch (op.type) {
        case TYPE_BOOL:
            return op.value[0] != 0;
        case TYPE_INT:
            return *((const int32_t*)op.value) != 0;
        case TYPE_FLOAT:
            return *((const double*)op.value) != 0.0;
        case TYPE_CHAR:
            return *((const uint16_t*)op.value) != 0;
        case TYPE_ADDRESS:
            return ((const void*)op.value) != nullptr;
        case TYPE_STRING:
            return *((const void**)op.value) != nullptr;
        default:
            return false;
    }
}
inline static int interpretInstruction(Instruction& inst, OpStack& opStack, char*& stack, ObjectCode& obj, int& instIndex,
                                       int& frameTop, HxVector<CallFrame*>& frames);
int interpret(ObjectCode& obj, int& ret, int& err) noexcept {
#ifdef HX_DEBUG
    wprintf(LOG_LABEL L"开始解释\n");
#endif
    // 找入口
    if (obj.procedures.empty() || obj.start >= obj.procedures.size()) {
        fwprintf(errorStream, ERR_LABEL L"无效的程序入口索引 %d\n", obj.start);
        err = -1;
        return -1;
    }
    Procedure& entry = obj.procedures[(obj.start)];
    HxVector<CallFrame*> frames;
    frames.reserve(512);
    int frameTop = 0;
    if (pushCallFrame(entry, frames) != 0) return -1;
    OpStack opStack = {};

    while (!frames.empty() && (!shouldExit.load())) {
        if (interpretInstruction(frames[(frameTop)]->proc->instructions[(frames[(frameTop)]->instIndex)], opStack,
                                 frames[(frameTop)]->stack, obj, frames[(frameTop)]->instIndex, frameTop, frames))
            return -1;
    }

    if (opStack.top - 1 < OP_STACK_SIZE && opStack.top - 1 >= 0) {
        ret = (int)*((int32_t*)opStack.opStack[opStack.top - 1].value);
        wprintf(INFO_LABEL L"主函数返回：%d\n", ret);
    }

    return 0;
}

static inline int promoteNumeric(_OpStack& a, _OpStack& b) {
    // 只处理 bool / int / float，其它类型交给上层报错
    if ((a.type != TYPE_INT && a.type != TYPE_FLOAT && a.type != TYPE_BOOL) ||
        (b.type != TYPE_INT && b.type != TYPE_FLOAT && b.type != TYPE_BOOL)) {
#ifdef HX_DEBUG
        wprintf(LOG_LABEL L"a.type: ");
        switch (a.type) {
            case TYPE_ADDRESS:
                wprintf(L"address");
                break;
            case TYPE_INT:
                wprintf(L"i32");
                break;
            case TYPE_FLOAT:
                wprintf(L"double");
                break;
            case TYPE_CHAR:
                wprintf(L"char(u16)");
                break;
            case TYPE_BOOL:
                wprintf(L"bool");
                break;
        }
        wprintf(L"\n");
        wprintf(LOG_LABEL L"b.type: ");
        switch (b.type) {
            case TYPE_ADDRESS:
                wprintf(L"address");
                break;
            case TYPE_INT:
                wprintf(L"i32");
                break;
            case TYPE_FLOAT:
                wprintf(L"double");
                break;
            case TYPE_CHAR:
                wprintf(L"char(u16)");
                break;
            case TYPE_BOOL:
                wprintf(L"bool");
                break;
        }
        wprintf(L"\n");
#endif
        return -1;
    }

    // 只要有一个是 float，就全部提升为 float
    if (a.type == TYPE_FLOAT || b.type == TYPE_FLOAT) {
        if (a.type == TYPE_INT) {
            int32_t v = *(int32_t*)a.value;
            double d = (double)v;
            memcpy(a.value, &d, sizeof(double));
            a.type = TYPE_FLOAT;
            a.size = sizeof(double);
        } else if (a.type == TYPE_BOOL) {
            char v = *(char*)a.value;
            double d = (double)v;
            memcpy(a.value, &d, sizeof(double));
            a.type = TYPE_FLOAT;
            a.size = sizeof(double);
        }
        if (b.type == TYPE_INT) {
            int32_t v = *(int32_t*)b.value;
            double d = (double)v;
            memcpy(b.value, &d, sizeof(double));
            b.type = TYPE_FLOAT;
            b.size = sizeof(double);
        } else if (b.type == TYPE_BOOL) {
            char v = *(char*)b.value;
            double d = (double)v;
            memcpy(b.value, &d, sizeof(double));
            b.type = TYPE_FLOAT;
            b.size = sizeof(double);
        }
    }
    return 0;
}

inline int interpretInstruction(Instruction& inst, OpStack& opStack, char*& stack, ObjectCode& obj, int& instIndex,
                                int& frameTop, HxVector<CallFrame*>& frames) {
#ifdef HX_DEBUG
    // wprintf(LOG_LABEL L"__________________________________________\n");
#endif
#ifdef HX_DEBUG
    // wprintf(LOG_LABEL L"解释指令\n");
#endif
    switch (inst.opcode) {
        case OP_LOAD_CONST: {
#ifdef HX_DEBUG
            wprintf(LOG_LABEL L"加载常量到操作数栈\n");
#endif
            if (opStack.top >= OP_STACK_SIZE) {
                fwprintf(errorStream, ERR_LABEL L"停♡......快停下♡人家栈都被......\n");
                return -1;
            }
            switch (inst.params[0].type) {
                case PARAM_TYPE_INT:
                    opStack.opStack[opStack.top].type = TYPE_INT;
                    opStack.opStack[opStack.top].size = sizeof(int32_t);
                    break;
                case PARAM_TYPE_FLOAT:
                    opStack.opStack[opStack.top].type = TYPE_FLOAT;
                    opStack.opStack[opStack.top].size = sizeof(double);
                    break;
                case PARAM_TYPE_CHAR:
                    opStack.opStack[opStack.top].type = TYPE_CHAR;
                    opStack.opStack[opStack.top].size = sizeof(uint16_t);
                    break;
                case PARAM_TYPE_BOOL:
                    opStack.opStack[opStack.top].type = TYPE_BOOL;
                    opStack.opStack[opStack.top].size = sizeof(char);
                    break;
                case PARAM_TYPE_STRING:
                    opStack.opStack[opStack.top].type = TYPE_STRING;
                    opStack.opStack[opStack.top].size = sizeof(uint16_t*);
                    break;
                case PARAM_TYPE_ADDRESS:
                    opStack.opStack[opStack.top].type = TYPE_ADDRESS;
                    opStack.opStack[opStack.top].size = sizeof(void*);
                    break;
                default:
                    fwprintf(errorStream, ERR_LABEL L"非法指令格式\n");
                    return -1;
                    break;
            }
            if (inst.params[0].type == PARAM_TYPE_STRING) {
                if (inst.params[1].type != PARAM_TYPE_INDEX) {
                    fwprintf(errorStream, ERR_LABEL L"非法指令格式\n");
                    return -1;
                }
                uint32_t* index = (uint32_t*)(&(inst.params[1].value));
                if (obj.constantPool.constants[*index].type != CONST_STRING) {
                    fwprintf(errorStream, ERR_LABEL L"非法指令格式\n");
                    return -1;
                }
                wchar_t* wstr = obj.constantPool.constants[*index].value.string_value;
                memcpy(opStack.opStack[opStack.top].value, wstr, sizeof(wchar_t*));
            } else {
                if (inst.params[0].type == PARAM_TYPE_INDEX) {
                    fwprintf(errorStream, ERR_LABEL L"非法指令格式\n");
                    return -1;
                }
                memcpy(opStack.opStack[opStack.top].value, inst.params[0].value, 8);
#ifdef HX_DEBUG
                switch (opStack.opStack[opStack.top].type) {
                    case TYPE_INT:
                        wprintf(LOG_LABEL L"%d\n", *((int32_t*)opStack.opStack[opStack.top].value));
                        break;
                    case TYPE_FLOAT:
                        wprintf(LOG_LABEL L"%lf\n", *((double*)opStack.opStack[opStack.top].value));
                        break;
                }
#endif
            }
            opStack.top++;
            break;
        }
        case OP_ADD: {
#ifdef HX_DEBUG
            wprintf(LOG_LABEL L"相加\n");
#endif
            if (opStack.top < 2) {
                fwprintf(errorStream, ERR_LABEL L"栈中操作数还不够喵~就一点点操作数还想运算，可真是个杂鱼喵~\n");
                return -1;
            }
            _OpStack rhs = opStack.opStack[--opStack.top];
            _OpStack lhs = opStack.opStack[--opStack.top];
            if (promoteNumeric(lhs, rhs) != 0) {
                fwprintf(errorStream, ERR_LABEL L"不支持的加法操作数类型\n");
                return -1;
            }
            _OpStack result = {};
            if (lhs.type == TYPE_INT) {
                int32_t res = *(int32_t*)lhs.value + *(int32_t*)rhs.value;
                result.type = TYPE_INT;
                result.size = sizeof(int32_t);
                memcpy(result.value, &res, sizeof(int32_t));
#ifdef HX_DEBUG
                wprintf(LOG_LABEL L"结果：%d\n", res);
#endif
            } else {
                double res = *(double*)lhs.value + *(double*)rhs.value;
                result.type = TYPE_FLOAT;
                result.size = sizeof(double);
                memcpy(result.value, &res, sizeof(double));
#ifdef HX_DEBUG
                wprintf(LOG_LABEL L"结果：%lf\n", res);
#endif
            }
            if (opStack.top >= OP_STACK_SIZE) {
                fwprintf(errorStream, ERR_LABEL L"停♡......快停下♡人家栈都被......\n\n");
                return -1;
            }

            opStack.opStack[opStack.top++] = result;
            break;
        }
        case OP_SUB: {
#ifdef HX_DEBUG
            wprintf(LOG_LABEL L"相减\n");
#endif
            if (opStack.top < 2) {
                fwprintf(errorStream, ERR_LABEL L"栈中操作数还不够喵~就一点点操作数还想运算，可真是个杂鱼喵~\n");
                return -1;
            }
            _OpStack rhs = opStack.opStack[--opStack.top];
            _OpStack lhs = opStack.opStack[--opStack.top];

            if (promoteNumeric(lhs, rhs) != 0) {
                fwprintf(errorStream, ERR_LABEL L"不支持的减法操作数类型\n");
                return -1;
            }

            _OpStack result = {};

            if (lhs.type == TYPE_INT) {
                int32_t res = *(int32_t*)lhs.value - *(int32_t*)rhs.value;
                result.type = TYPE_INT;
                result.size = sizeof(int32_t);
                memcpy(result.value, &res, sizeof(int32_t));

#ifdef HX_DEBUG
                wprintf(LOG_LABEL L"结果：%d\n", res);
#endif
            } else {
                double res = *(double*)lhs.value - *(double*)rhs.value;
                result.type = TYPE_FLOAT;
                result.size = sizeof(double);
                memcpy(result.value, &res, sizeof(double));
#ifdef HX_DEBUG
                wprintf(LOG_LABEL L"结果：%lf\n", res);
#endif
            }

            opStack.opStack[opStack.top++] = result;
            break;
        }
        case OP_MUL: {
#ifdef HX_DEBUG
            wprintf(LOG_LABEL L"相乘\n");
#endif
            if (opStack.top < 2) {
                fwprintf(errorStream, ERR_LABEL L"栈中操作数还不够喵~就一点点操作数还想运算，可真是个杂鱼喵~\n");
                return -1;
            }
            _OpStack rhs = opStack.opStack[--opStack.top];
            _OpStack lhs = opStack.opStack[--opStack.top];

            if (promoteNumeric(lhs, rhs) != 0) {
                fwprintf(errorStream, ERR_LABEL L"不支持的乘法操作数类型喵\n");
                return -1;
            }

            _OpStack result = {};

            if (lhs.type == TYPE_INT) {
                int32_t res = (*(int32_t*)lhs.value) * (*(int32_t*)rhs.value);
                result.type = TYPE_INT;
                result.size = sizeof(int32_t);
                memcpy(result.value, &res, sizeof(int32_t));
#ifdef HX_DEBUG
                wprintf(LOG_LABEL L"结果：%d\n", res);
#endif
            } else {
                double res = (*(double*)lhs.value) * (*(double*)rhs.value);
                result.type = TYPE_FLOAT;
                result.size = sizeof(double);
                memcpy(result.value, &res, sizeof(double));
#ifdef HX_DEBUG
                wprintf(LOG_LABEL L"结果：%lf\n", res);
#endif
            }

            opStack.opStack[opStack.top++] = result;
            break;
        }
        case OP_DIV: {
#ifdef HX_DEBUG
            wprintf(LOG_LABEL L"相除\n");
#endif
            if (opStack.top < 2) {
                fwprintf(errorStream, ERR_LABEL L"栈中操作数还不够喵~就一点点操作数还想运算，可真是个杂鱼喵~\n");
                return -1;
            }
            _OpStack rhs = opStack.opStack[--opStack.top];
            _OpStack lhs = opStack.opStack[--opStack.top];

            if (promoteNumeric(lhs, rhs) != 0) {
                fwprintf(errorStream, ERR_LABEL L"不支持的除法操作数类型喵\n");
                return -1;
            }

            _OpStack result = {};

            if (lhs.type == TYPE_INT) {
                if ((*(int32_t*)rhs.value) == 0) {
                    fwprintf(errorStream, ERR_LABEL L"不能除以0喵\n");
                    return -1;
                }
                int32_t res = (*(int32_t*)lhs.value) / (*(int32_t*)rhs.value);
                result.type = TYPE_INT;
                result.size = sizeof(int32_t);
                memcpy(result.value, &res, sizeof(int32_t));
#ifdef HX_DEBUG
                wprintf(LOG_LABEL L"结果：%d\n", res);
#endif
            } else {
                if ((*(double*)rhs.value) == 0) {
                    fwprintf(errorStream, ERR_LABEL L"不能除以0喵\n");
                    return -1;
                }
                double res = (*(double*)lhs.value) / (*(double*)rhs.value);
                result.type = TYPE_FLOAT;
                result.size = sizeof(double);
                memcpy(result.value, &res, sizeof(double));
#ifdef HX_DEBUG
                wprintf(LOG_LABEL L"结果：%lf\n", res);
#endif
            }

            opStack.opStack[opStack.top++] = result;
            break;
        }
        case OP_INC: {
#ifdef HX_DEBUG
            wprintf(LOG_LABEL L"自增\n");
#endif
            void* data = stack + *((uint32_t*)inst.params[0].value);
            switch (inst.params[1].type) {
                case PARAM_TYPE_INT: {
                    (*((int32_t*)data))++;
                    break;
                }
                case PARAM_TYPE_FLOAT: {
                    (*((double*)data))++;
                }
                case PARAM_TYPE_CHAR: {
                    (*((uint16_t*)data))++;
                    break;
                }
            }
            break;
        }
        case OP_DEC: {
#ifdef HX_DEBUG
            wprintf(LOG_LABEL L"自减\n");
#endif
            void* data = stack + *((uint32_t*)inst.params[0].value);
            switch (inst.params[1].type) {
                case PARAM_TYPE_INT: {
                    (*((int32_t*)data))--;
                    break;
                }
                case PARAM_TYPE_FLOAT: {
                    (*((double*)data))--;
                    break;
                }
                case PARAM_TYPE_CHAR: {
                    (*((uint16_t*)data))--;
                    break;
                }
            }
            break;
        }
        case OP_EQU: {
#ifdef HX_DEBUG
            wprintf(LOG_LABEL L"相等\n");
#endif
            if (opStack.top < 2) {
                fwprintf(errorStream, ERR_LABEL L"栈中操作数还不够喵~就一点点操作数还想运算，可真是个杂鱼喵~\n");
                return -1;
            }
            _OpStack& rhs = opStack.opStack[--opStack.top];
            _OpStack& lhs = opStack.opStack[--opStack.top];

            if (promoteNumeric(lhs, rhs) != 0) {
                fwprintf(errorStream, ERR_LABEL L"不支持的相等操作数类型\n");
                return -1;
            }

            _OpStack result = {};

            if (lhs.type == TYPE_INT && rhs.type == TYPE_INT) {
                char res = ((*(int32_t*)lhs.value) == (*(int32_t*)rhs.value));
                result.type = TYPE_BOOL;
                result.size = sizeof(char);
                memcpy(result.value, &res, sizeof(char));
#ifdef HX_DEBUG
                wprintf(LOG_LABEL L"结果：%d\n", (int)res);
#endif
            } else if (lhs.type == TYPE_FLOAT && rhs.type == TYPE_FLOAT) {
                char res = ((*(double*)lhs.value) == (*(double*)rhs.value));
                result.type = TYPE_BOOL;
                result.size = sizeof(char);
                memcpy(result.value, &res, sizeof(char));
#ifdef HX_DEBUG
                wprintf(LOG_LABEL L"结果：%d\n", (int)res);
#endif
            } else if (lhs.type == TYPE_BOOL || rhs.type == TYPE_BOOL) {
                // 非0的任何布尔值视作相等
                char num1 = 0;
                char num2 = 0;
                for (int i = 0; i < rhs.size; i++) {
                    if (rhs.value[i] != 0) {
                        num1 = 1;
                        break;
                    }
                }
                for (int i = 0; i < lhs.size; i++) {
                    if (lhs.value[i] != 0) {
                        num2 = 1;
                        break;
                    }
                }
                char res = (num1 == num2);
                result.type = TYPE_BOOL;
                result.size = sizeof(char);
                memcpy(result.value, &res, sizeof(char));
#ifdef HX_DEBUG
                wprintf(LOG_LABEL L"结果：%d\n", (int)res);
#endif
            } else {
                int compareSize = 0;
                if (lhs.size > rhs.size) {
                    compareSize = lhs.size;
                } else
                    compareSize = rhs.size;
                char res = memcmp(lhs.value, rhs.value, compareSize) ? 1 : 0;
                result.type = TYPE_BOOL;
                result.size = sizeof(char);
                memcpy(result.value, &res, sizeof(char));
#ifdef HX_DEBUG
                wprintf(LOG_LABEL L"结果：%d\n", (int)res);
#endif
            }
            opStack.opStack[opStack.top++] = result;
            break;
        }
        case OP_LT: {
#ifdef HX_DEBUG
            wprintf(LOG_LABEL L"次栈顶<栈顶\n");
#endif
            if (opStack.top < 2) {
                fwprintf(errorStream, ERR_LABEL L"栈中操作数还不够喵~就一点点操作数还想运算，可真是个杂鱼喵~\n");
                return -1;
            }
            _OpStack& rhs = opStack.opStack[--opStack.top];
            _OpStack& lhs = opStack.opStack[--opStack.top];

            if (promoteNumeric(lhs, rhs) != 0) {
                fwprintf(errorStream, ERR_LABEL L"不支持的操作数类型喵\n");
                return -1;
            }

            _OpStack result = {};

            if (lhs.type == TYPE_INT && rhs.type == TYPE_INT) {
                char res = ((*(int32_t*)lhs.value) < (*(int32_t*)rhs.value));
                result.type = TYPE_BOOL;
                result.size = sizeof(char);
                memcpy(result.value, &res, sizeof(char));
#ifdef HX_DEBUG
                wprintf(LOG_LABEL L"结果：%d\n", (bool)res);
#endif
            } else if (lhs.type == TYPE_FLOAT && rhs.type == TYPE_FLOAT) {
                char res = ((*(double*)lhs.value) < (*(double*)rhs.value));
                result.type = TYPE_BOOL;
                result.size = sizeof(char);
                memcpy(result.value, &res, sizeof(char));
#ifdef HX_DEBUG
                wprintf(LOG_LABEL L"结果：%d\n", (bool)res);
#endif
            } else if (lhs.type == TYPE_BOOL || rhs.type == TYPE_BOOL) {
                char num1 = 0;
                char num2 = 0;
                num1 = isOperandTrue(lhs);
                num2 = isOperandTrue(rhs);
                char res = (num1 < num2);
                result.type = TYPE_BOOL;
                result.size = sizeof(char);
                memcpy(result.value, &res, sizeof(char));
#ifdef HX_DEBUG
                wprintf(LOG_LABEL L"结果：%d\n", (int)res);
#endif
            } else {
                int compareSize = 0;
                if (lhs.size > rhs.size) {
                    compareSize = lhs.size;
                } else
                    compareSize = rhs.size;
                char res = (memcmp(lhs.value, rhs.value, compareSize) < 0) ? 1 : 0;
                result.type = TYPE_BOOL;
                result.size = sizeof(char);
                memcpy(result.value, &res, sizeof(char));
#ifdef HX_DEBUG
                wprintf(LOG_LABEL L"结果：%d\n", (int)res);
#endif
            }
            opStack.opStack[opStack.top++] = result;
            break;
        }
        case OP_GT: {
#ifdef HX_DEBUG
            wprintf(LOG_LABEL L"次栈顶>栈顶\n");
#endif
            if (opStack.top < 2) {
                fwprintf(errorStream, ERR_LABEL L"栈中操作数还不够喵~就一点点操作数还想运算，可真是个杂鱼喵~\n");
                return -1;
            }
            _OpStack& rhs = opStack.opStack[--opStack.top];
            _OpStack& lhs = opStack.opStack[--opStack.top];

            if (promoteNumeric(lhs, rhs) != 0) {
                fwprintf(errorStream, ERR_LABEL L"不支持的操作数类型喵\n");
                return -1;
            }

            _OpStack result = {};

            if (lhs.type == TYPE_INT && rhs.type == TYPE_INT) {
                char res = ((*(int32_t*)lhs.value) > (*(int32_t*)rhs.value));
                result.type = TYPE_BOOL;
                result.size = sizeof(char);
                memcpy(result.value, &res, sizeof(char));
#ifdef HX_DEBUG
                wprintf(LOG_LABEL L"结果：%d\n", (bool)res);
#endif
            } else if (lhs.type == TYPE_FLOAT && rhs.type == TYPE_FLOAT) {
                char res = ((*(double*)lhs.value) > (*(double*)rhs.value));
                result.type = TYPE_BOOL;
                result.size = sizeof(char);
                memcpy(result.value, &res, sizeof(char));
#ifdef HX_DEBUG
                wprintf(LOG_LABEL L"结果：%d\n", (bool)res);
#endif
            } else if (lhs.type == TYPE_BOOL || rhs.type == TYPE_BOOL) {
                char num1 = 0;
                char num2 = 0;
                num1 = isOperandTrue(lhs);
                num2 = isOperandTrue(rhs);
                char res = (num1 > num2);
                result.type = TYPE_BOOL;
                result.size = sizeof(char);
                memcpy(result.value, &res, sizeof(char));
#ifdef HX_DEBUG
                wprintf(LOG_LABEL L"结果：%d\n", (int)res);
#endif
            } else {
                int compareSize = 0;
                if (lhs.size > rhs.size) {
                    compareSize = lhs.size;
                } else
                    compareSize = rhs.size;
                char res = (memcmp(lhs.value, rhs.value, compareSize) > 0) ? 1 : 0;
                result.type = TYPE_BOOL;
                result.size = sizeof(char);
                memcpy(result.value, &res, sizeof(char));
#ifdef HX_DEBUG
                wprintf(LOG_LABEL L"结果：%d\n", (int)res);
#endif
            }
            opStack.opStack[opStack.top++] = result;
            break;
        }
        case OP_OR: {
#ifdef HX_DEBUG
            wprintf(LOG_LABEL L"次栈顶|栈顶\n");
#endif
            if (opStack.top < 2) {
                fwprintf(errorStream, ERR_LABEL L"栈中操作数还不够喵~就一点点操作数还想运算，可真是个杂鱼喵~\n");
                return -1;
            }
            _OpStack& rhs = opStack.opStack[--opStack.top];
            _OpStack& lhs = opStack.opStack[--opStack.top];

            if (promoteNumeric(lhs, rhs) != 0) {
                fwprintf(errorStream, ERR_LABEL L"不支持的操作数类型喵\n");
                return -1;
            }

            _OpStack result = {};

            if (lhs.type == TYPE_INT && rhs.type == TYPE_INT) {
                int32_t res = ((*(int32_t*)lhs.value) | (*(int32_t*)rhs.value));
                result.type = TYPE_INT;
                result.size = sizeof(int32_t);
                memcpy(result.value, &res, sizeof(int32_t));
#ifdef HX_DEBUG
                wprintf(LOG_LABEL L"结果：%d\n", (int32_t)res);
#endif
            } else if (lhs.type == TYPE_FLOAT && rhs.type == TYPE_FLOAT) {
                fwprintf(errorStream, ERR_LABEL L"不支持的操作数类型喵\n");
                return -1;
            } else if (lhs.type == TYPE_BOOL || rhs.type == TYPE_BOOL) {
                char num1 = 0;
                char num2 = 0;
                num1 = isOperandTrue(lhs);
                num2 = isOperandTrue(rhs);
                char res = (num1 | num2);
                result.type = TYPE_BOOL;
                result.size = sizeof(char);
                memcpy(result.value, &res, sizeof(char));
#ifdef HX_DEBUG
                wprintf(LOG_LABEL L"结果：%d\n", (int)res);
#endif
            } else {
                int compareSize = 0;
                if (lhs.size > rhs.size) {
                    compareSize = lhs.size;
                    result.type = lhs.type;
                } else
                    compareSize = rhs.size;
                result.type = rhs.type;

                for (int i = 0; i < compareSize; i++) {
                    result.value[i] = lhs.value[i] | rhs.value[i];
                }
                result.size = compareSize;
#ifdef HX_DEBUG
                wprintf(LOG_LABEL L"结果(强转为i32)：%d\n", *((int32_t*)result.value));
#endif
            }
            opStack.opStack[opStack.top++] = result;
            break;
        }
        case OP_AND_LOGIC: {
#ifdef HX_DEBUG
            wprintf(LOG_LABEL L"逻辑与\n");
#endif
            if (opStack.top < 2) {
                fwprintf(errorStream, ERR_LABEL L"栈中操作数还不够喵~就一点点操作数还想运算，可真是个杂鱼喵~\n");
                return -1;
            }
            // 弹出操作数
            _OpStack rhs = opStack.opStack[--opStack.top];
            _OpStack lhs = opStack.opStack[--opStack.top];
            bool lhsVal = isOperandTrue(lhs);
            bool rhsVal = isOperandTrue(rhs);

            _OpStack result = {};
            result.size = sizeof(char);
            result.type = TYPE_BOOL;
            result.value[0] = (char)(lhsVal && rhsVal);

#ifdef HX_DEBUG
            wprintf(LOG_LABEL L"结果：%d\n", (int)result.value[0]);
#endif
            opStack.opStack[opStack.top++] = result;
            break;
        }
        case OP_OR_LOGIC: {
#ifdef HX_DEBUG
            wprintf(LOG_LABEL L"逻辑或\n");
#endif
            if (opStack.top < 2) {
                fwprintf(errorStream, ERR_LABEL L"栈中操作数还不够喵~就一点点操作数还想运算，可真是个杂鱼喵~\n");
                return -1;
            }
            _OpStack rhs = opStack.opStack[--opStack.top];
            _OpStack lhs = opStack.opStack[--opStack.top];

            bool lhsVal = isOperandTrue(lhs);
            bool rhsVal = isOperandTrue(rhs);

            _OpStack result = {};
            result.size = sizeof(char);
            result.type = TYPE_BOOL;
            result.value[0] = (char)(lhsVal || rhsVal);

#ifdef HX_DEBUG
            wprintf(LOG_LABEL L"结果：%d\n", (int)result.value[0]);
#endif
            opStack.opStack[opStack.top++] = result;
            break;
        }
        case OP_NOT_LOGIC: {
#ifdef HX_DEBUG
            wprintf(LOG_LABEL L"逻辑非\n");
#endif
            if (opStack.top < 1) {
                fwprintf(errorStream, ERR_LABEL L"栈中操作数还不够喵~就一点点操作数还想运算，可真是个杂鱼喵~\n");
                return -1;
            }
            _OpStack& topOp = opStack.opStack[opStack.top - 1];

            bool val = isOperandTrue(topOp);

            topOp.size = sizeof(char);
            topOp.type = TYPE_BOOL;
            topOp.value[0] = (char)(!val);

#ifdef HX_EBUG
            wprintf(LOG_LABEL L"结果：%d\n", (int)result.value[0]);
#endif
            break;
        }
        case OP_CAL: {  // CAL <proc_index>(u32) <paramCount>(u32)
#ifdef HX_DEBUG
            wprintf(LOG_LABEL L"调用\n");
#endif
            if (inst.params[0].type != PARAM_TYPE_INDEX) {
                fwprintf(errorStream, ERR_LABEL L"非法指令格式\n");
                return -1;
            }
            if (inst.params[1].type != PARAM_TYPE_INT) {
                fwprintf(errorStream, ERR_LABEL L"非法指令格式\n");
                return -1;
            }
            int32_t argCount = 0;
            memcpy(&argCount, inst.params[1].value, sizeof(int32_t));
            int32_t procAddr = 0;
            memcpy(&procAddr, inst.params[0].value, sizeof(int32_t));
            Procedure& proc = (obj.procedures[(procAddr)]);
            if (opStack.top < argCount) {
                fwprintf(errorStream, ERR_LABEL L"参数不够喵~\n");
                return -1;
            }
            // 插入栈帧
            if (pushCallFrame(proc, frames) != 0) return -1;
            frameTop++;
            // 复制实参
#ifdef HX_DEBUG
            wprintf(LOG_LABEL L"调用->从栈中复制参数\n");
#endif
            uint32_t currentParamOffset = 0;  // 从子函数栈底偏移0开始铺参数
            for (int i = opStack.top - argCount; i < opStack.top; i++) {
                memcpy(frames[frameTop]->stack + currentParamOffset, opStack.opStack[i].value, opStack.opStack[i].size);
#ifdef HX_DEBUG
                wprintf(LOG_LABEL L"调用->从栈中复制参数->%d byte, offest: %d \n", opStack.opStack[i].size, currentParamOffset);
#endif
                currentParamOffset += opStack.opStack[i].size;  // 铺完一个，挪动指针
            }
            opStack.top -= argCount;  // 弹出参数
            // 调用者跳过CAL以防无限递归
            frames[frameTop - 1]->instIndex++;
            return 0;
            break;
        }
        case OP_PRINT_STRING: {
            wprintf(L"%ls", obj.constantPool.constants[*((uint32_t*)inst.params[0].value)].value.string_value);
            break;
        }
        case OP_RET: {
#ifdef HX_DEBUG
            wprintf(LOG_LABEL L"返回\n");
#endif
#ifdef HX_DEBUG
            wprintf(LOG_LABEL L"__________________________________________\n");
#endif
            memoryAllocer.markFreeable((void*)frames[frameTop]->stack);
            memoryAllocer.markFreeable((void*)frames[frameTop]);
            frames.pop_back();
            frameTop--;
            return 0;
            break;
        }
        case OP_POP: {
#ifdef HX_DEBUG
            wprintf(LOG_LABEL L"弾出一个元素\n");
#endif
            if (opStack.top <= 0) {
                fwprintf(errorStream, ERR_LABEL L"栈中空荡荡的喵，因此不能OP_POP喵\n");
                return -1;
            }
            opStack.top--;
            break;
        }
        case OP_CHAR_TO_INT: {
            _OpStack& topRef = opStack.opStack[opStack.top - 1];
#ifdef HX_DEBUG
            wprintf(LOG_LABEL L"char(u16)->i32\n");
#endif
            // 读出原有的 16 位字符值
            uint16_t charVal = *((uint16_t*)topRef.value);
            int32_t intVal = (int32_t)charVal;

            topRef.type = TYPE_INT;
            topRef.size = sizeof(int32_t);
            memset(topRef.value, 0,
                   8);  // 清理原有内存，防止脏数据干扰
            memcpy(topRef.value, &intVal, sizeof(int32_t));
            break;
        }
        case OP_INT_TO_CHAR: {
            _OpStack& topRef = opStack.opStack[opStack.top - 1];
#ifdef HX_DEBUG
            wprintf(LOG_LABEL L"i32->char(u16)\n");
#endif
            int32_t intVal = *((int32_t*)topRef.value);
            uint16_t charVal = (uint16_t)intVal;  // 强制截断

            topRef.type = TYPE_CHAR;
            topRef.size = sizeof(uint16_t);
            memset(topRef.value, 0, 8);
            memcpy(topRef.value, &charVal, sizeof(uint16_t));
            break;
        }
        case OP_INT_TO_FLOAT: {
            _OpStack& topRef = opStack.opStack[opStack.top - 1];
#ifdef HX_DEBUG
            wprintf(LOG_LABEL L"i32->double\n");
#endif
            int32_t intVal = *((int32_t*)topRef.value);
            double floatVal = (double)intVal;

            topRef.type = TYPE_FLOAT;
            topRef.size = sizeof(double);
            memset(topRef.value, 0, 8);
            memcpy(topRef.value, &floatVal, sizeof(double));
            break;
        }
        case OP_CHAR_TO_FLOAT: {
#ifdef HX_DEBUG
            wprintf(LOG_LABEL L"char(u16)->double\n");
#endif
            _OpStack& topRef = opStack.opStack[opStack.top - 1];
            uint16_t charVal = *((uint16_t*)topRef.value);
            double floatVal = (double)charVal;

            topRef.type = TYPE_FLOAT;
            topRef.size = sizeof(double);
            memset(topRef.value, 0, 8);
            memcpy(topRef.value, &floatVal, sizeof(double));
            break;
        }
        case OP_CHAR_TO_STRING: {
        }
        case OP_FLOAT_TO_INT: {
#ifdef HX_DEBUG
            wprintf(LOG_LABEL L"double->i32\n");
#endif
            _OpStack& topRef = opStack.opStack[opStack.top - 1];
            double floatVal = *((double*)topRef.value);
            int32_t intVal = (int32_t)floatVal;

            topRef.type = TYPE_INT;
            topRef.size = sizeof(int32_t);
            memset(topRef.value, 0, 8);
            memcpy(topRef.value, &intVal, sizeof(int32_t));
            break;
        }
        case OP_INT_TO_STRING: {
            _OpStack& topRef = opStack.opStack[opStack.top - 1];
#ifdef HX_DEBUG
            wprintf(LOG_LABEL L"i32 -> string\n");
#endif

            int32_t intVal = *((int32_t*)topRef.value);
            wchar_t buffer[32];
            int written = swprintf(buffer, 32, L"%d", intVal);
            if (written < 0) {
                fwprintf(errorStream, ERR_LABEL L"字符串转换失败\n");
                return -1;
            }
            size_t allocSize = (written + 1) * sizeof(wchar_t);
            wchar_t* str = (wchar_t*)memoryAllocer.hxMalloc(allocSize);
            if (!str) {
                fwprintf(errorStream, ERR_LABEL L"内存分配失败\n");
                return -1;
            }
            wcscpy(str, buffer);
            topRef.type = TYPE_STRING;
            topRef.size = sizeof(wchar_t*);
            memset(topRef.value, 0, 8);
            memcpy(topRef.value, &str, sizeof(wchar_t*));
            break;
        }
        case OP_JMP: {
#ifdef HX_DEBUG
            wprintf(LOG_LABEL L"跳转\n");
#endif
            if (inst.params[0].type != PARAM_TYPE_INDEX) {
                fwprintf(errorStream, ERR_LABEL L"非法指令格式\n");
                return -1;
            }
            uint32_t instAddr = 0;
            memcpy(&instAddr, inst.params[0].value, sizeof(uint32_t));
            instIndex = (int)instAddr;
            return 0;
        }
        case OP_STORE_VAR: {
#ifdef HX_DEBUG
            wprintf(LOG_LABEL L"存储变量\n");
#endif
            if (opStack.top > OP_STACK_SIZE || opStack.top == 0) {
                fwprintf(errorStream, ERR_LABEL L"停♡......快停下♡人家栈都被......\n");
                return -1;
            }
            uint32_t size = 0;
            uint32_t offest = 0;
            memcpy(&size, inst.params[1].value, sizeof(uint32_t));
            memcpy(&offest, inst.params[0].value, sizeof(uint32_t));
#ifdef HX_DEBUG
            wprintf(LOG_LABEL L"将大小为%u的数据存储至偏移量%u处\n", size, offest);
#endif
            void* addr = stack + offest;
            memcpy(addr, opStack.opStack[opStack.top - 1].value, size);
            break;
        }
        case OP_LOAD_VAR: {
#ifdef HX_DEBUG
            wprintf(LOG_LABEL L"读取变量\n");
#endif
            if (opStack.top >= OP_STACK_SIZE) {
                fwprintf(errorStream, ERR_LABEL L"停♡......快停下♡人家栈都被......\n");
                return -1;
            }
            uint32_t size = 0;
            uint32_t offest = 0;
            memcpy(&size, inst.params[1].value, sizeof(uint32_t));
            memcpy(&offest, inst.params[0].value, sizeof(uint32_t));
#ifdef HX_DEBUG
            wprintf(LOG_LABEL L"将偏移量%u处，大小%u byte的数据加载至栈顶\n", offest, size);
#endif

            switch (inst.params[1].type) {
                case PARAM_TYPE_INT: {
                    opStack.opStack[opStack.top].type = TYPE_INT;
                } break;
                case PARAM_TYPE_FLOAT: {
                    opStack.opStack[opStack.top].type = TYPE_FLOAT;
                } break;
                case PARAM_TYPE_CHAR: {
                    opStack.opStack[opStack.top].type = TYPE_CHAR;
                } break;
                case PARAM_TYPE_BOOL: {
                    opStack.opStack[opStack.top].type = TYPE_BOOL;
                } break;
                case PARAM_TYPE_STRING: {
                    opStack.opStack[opStack.top].type = TYPE_STRING;
                } break;
                case PARAM_TYPE_ADDRESS: {
                    opStack.opStack[opStack.top].type = TYPE_ADDRESS;
                } break;
                default: {
                    fwprintf(errorStream, ERR_LABEL L"非法指令格式\n");
                    return -1;
                } break;
            }
            void* addr = stack + offest;
            opStack.opStack[opStack.top].size = size;
            memcpy(opStack.opStack[opStack.top].value, addr, size);
#ifdef HX_DEBUG
            switch (opStack.opStack[opStack.top].type) {
                case TYPE_INT:
                    wprintf(LOG_LABEL L"%d\n", *((int32_t*)opStack.opStack[opStack.top].value));
                    break;
                case TYPE_FLOAT:
                    wprintf(LOG_LABEL L"%lf\n", *((double*)opStack.opStack[opStack.top].value));
                    break;
            }
#endif
            opStack.top++;
            break;
        }
        case OP_JMP_CONDITION: {
#ifdef HX_DEBUG
            wprintf(LOG_LABEL L"条件跳转\n");
#endif
            if (opStack.top <= 0) {
                fwprintf(errorStream, ERR_LABEL L"栈中空荡荡的喵，因此不能OP_JMP_CONDITION喵\n");
                return -1;
            }
            bool condition = false;
            // 判断栈顶条件
            switch (opStack.opStack[opStack.top - 1].type) {
                case TYPE_INT: {
                    int32_t val = 0;
                    memcpy(&val, opStack.opStack[opStack.top - 1].value, opStack.opStack[opStack.top - 1].size);
                    if (val) condition = true;
                    break;
                }
                case TYPE_FLOAT: {
                    double val = 0;
                    memcpy(&val, opStack.opStack[opStack.top - 1].value, opStack.opStack[opStack.top - 1].size);
                    if (val) condition = true;
                    break;
                }
                case TYPE_CHAR: {
                    uint16_t val = (uint16_t)(L'\0');
                    memcpy(&val, opStack.opStack[opStack.top - 1].value, opStack.opStack[opStack.top - 1].size);
                    if ((wchar_t)val != L'\0') condition = true;
                    break;
                }
                case TYPE_BOOL: {
                    char val = (char)0;
                    memcpy(&val, opStack.opStack[opStack.top - 1].value, opStack.opStack[opStack.top - 1].size);
                    if (val) condition = true;
                    break;
                }
                case TYPE_STRING: {  // void* size = 4
                    uint32_t val = 0;
                    memcpy(&val, opStack.opStack[opStack.top - 1].value, opStack.opStack[opStack.top - 1].size);
                    if (val) condition = true;
                    break;
                }
                case TYPE_ADDRESS: {  // void* size = 4
                    uint32_t val = 0;
                    memcpy(&val, opStack.opStack[opStack.top - 1].value, opStack.opStack[opStack.top - 1].size);
                    if (val) condition = true;
                    break;
                }
            }
            // 条件需弹出
            if (opStack.top > 0) opStack.top--;
            if (condition) {
                uint32_t trueAddr = 0U;
                memcpy(&trueAddr, inst.params[0].value, sizeof(uint32_t));
                instIndex = (int)trueAddr;
#ifdef HX_DEBUG
                wprintf(LOG_LABEL L"条件跳转->true-> %d\n", instIndex);
#endif
                return 0;
            } else {
                uint32_t falseAddr = 0U;
                memcpy(&falseAddr, inst.params[1].value, sizeof(uint32_t));
                instIndex = (int)falseAddr;
#ifdef HX_DEBUG
                wprintf(LOG_LABEL L"条件跳转->false-> %d\n", instIndex);
#endif
                return 0;
            }
            break;
        }
        case OP_NOP: {
            break;
        }
        default: {
            wprintf(ERR_LABEL L"未知指令！\n");
            return -1;
        }
    }
#ifdef HX_DEBUG
    wprintf(LOG_LABEL L"__________________________________________\n");
#endif
    frames[(frameTop)]->instIndex++;
    return 0;
}
#endif