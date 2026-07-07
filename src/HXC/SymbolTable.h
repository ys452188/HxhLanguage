#pragma once
typedef struct Instruction Instruction;
typedef struct Token Token;
typedef uint8_t Opcode;
typedef enum IR_DataTypeKind {
    IR_DT_INT,
    IR_DT_FLOAT,
    IR_DT_STRING,
    IR_DT_CHAR,
    IR_DT_BOOL,
    IR_DT_INT_ARR,  // 数组
    IR_DT_FLOAT_ARR,
    IR_DT_STRING_ARR,
    IR_DT_CHAR_ARR,
    IR_DT_BOOL_ARR,
    IR_DT_CUSTOM_ARR,
    IR_DT_INT_REFER,  // 引用类型
    IR_DT_FLOAT_REFER,
    IR_DT_STRING_REFER,
    IR_DT_CHAR_REFER,
    IR_DT_BOOL_REFER,
    IR_DT_CUSTOM_REFER,
    IR_DT_VOID,
    IR_DT_CUSTOM
} IR_DataTypeKind;
typedef struct IR_DataType {
    IR_DataTypeKind kind;
    int arrayLength;
    wchar_t* customTypeName;  // 当kind为IR_DT_CUSTOM 时使用
} IR_DataType;
//-----------------------------------------------------------

typedef struct Procedure Procedure;
typedef struct IR_FunctionParam {
    wchar_t* name;
    IR_DataType type;
} IR_FunctionParam;
class FunCallPitch;
typedef struct IR_Function {
    wchar_t* name;
    IR_FunctionParam* params;
    int paramCount;
    bool isReturnTypeKnown;
    IR_DataType returnType;
    Token* bodyTokens;  // 函数体的Token流
    int body_token_count;
    bool isUsed;  // 标记该函数是否被使用过

    FunCallPitch* pitch;
    Procedure* proc;
} IR_Function;
//-------------------------------------------------------------
typedef struct IR_Variable {
    wchar_t* name;
    IR_DataType type;
    bool isTypeKnown;
    bool isOnlyRead;
    int address;  // 变量在符号号表中的地址
} IR_Variable;
//-------------------------------------------------------------
typedef union IR_ClassMemberData {
    IR_Variable* variable;
    IR_Function* function;
} IR_ClassMemberData;
enum IR_ClassMemberType { IR_CM_VARIABLE, IR_CM_FUNCTION };
typedef struct IR_ClassMember {
    IR_ClassMemberType type;
    IR_ClassMemberData data;
} IR_ClassMember;
typedef struct IR_ClassBody {
    IR_ClassMember* publicMembers;
    int public_member_count;
    IR_ClassMember* privateMembers;
    int private_member_count;
    IR_ClassMember* protectedMembers;
    int protected_member_count;
} IR_ClassBody;
typedef struct IR_Class {
    int line;  // 类定义所在行号
    wchar_t* name;
    wchar_t* parent_name;  // 父类名
    int fatherIndex;       // 父类在类表中的索引，-1表示无父类

    IR_ClassBody body;

    int size;  // 类的大小，单位：字节
} IR_Class;
//---------------------------------------------------------------
typedef struct IR_Program {
    IR_Variable** global_variables;
    int global_variable_count;
    std::vector<IR_Function*> functions;
    IR_Class** classes;
    int class_count;
} IR_Program;

// 变量处理：存储各变量与其对应的指令，用于回填、标记是否有用
class Symbol {
   public:
    bool isUsed;
    wchar_t* name;
    bool isTypeKnown;
    IR_DataType type;
    int size;
    int offest;  // 在栈中的偏移量增加的量
    std::vector<int> instIndex;
    int procIndex;

    Symbol() : isUsed(false), name(nullptr), isTypeKnown(false), size(0), offest(0) {}
    // 拷贝构造函数
    Symbol(const Symbol& other) {
        this->isUsed = other.isUsed;
        this->isTypeKnown = other.isTypeKnown;
        this->type = other.type;
        this->size = other.size;
        this->offest = other.offest;
        if (other.name != nullptr) {
            this->name = wcsdup(other.name);
        } else {
            this->name = nullptr;
        }
        this->instIndex = other.instIndex;
        this->procIndex = other.procIndex;
    }

    // 赋值运算符实现
    // 圣杯写法
    Symbol& operator=(Symbol other) {
        // 把当前对象的内容和other进行大交换
        std::swap(this->name, other.name);
        std::swap(this->isTypeKnown, other.isTypeKnown);
        std::swap(this->type, other.type);
        std::swap(this->size, other.size);
        std::swap(this->offest, other.offest);
        std::swap(this->instIndex, other.instIndex);
        std::swap(this->procIndex, other.procIndex);
        std::swap(this->isUsed, other.isUsed);
        return *this;
    }

    ~Symbol() {
        free(name);
        name = nullptr;
    }
};

typedef class SymbolTable {
    public:
    std::vector<IR_Function*> fun;  // 函数表（数组）
    std::vector<Symbol> vars;
    uint32_t var_size;
    SymbolTable& operator=(SymbolTable other) {
        std::swap(this->fun, other.fun);
        std::swap(this->vars, other.vars);
        std::swap(this->var_size, other.var_size);
        return *this;
    }
    SymbolTable(const SymbolTable& other) {
        this->fun = other.fun;
        this->vars = other.vars;
        this->var_size = other.var_size;
    }
    SymbolTable():var_size(0) {}
} SymbolTable;
class FunCallPitch {  // 回填CALL指令,被指向
   public:
    FunCallPitch(IR_Function* ir_fun) noexcept : fun(ir_fun), index(-1) {}
    IR_Function* fun;
    int index;
};
class FunCallPitchTable {
    public:
    std::vector<FunCallPitch*> pitches;
    FunCallPitch* enter(IR_Function* fun) {
        for (int i = 0; i < pitches.size(); i++) {
            if (pitches.at(i)->fun == fun) return pitches.at(i);
        }
        FunCallPitch* pitch = new (std::nothrow) FunCallPitch(fun);
        if (pitch == nullptr) return NULL;
        pitches.push_back(pitch);
        return pitch;
    }
#ifdef HX_DEBUG
    void list() {
        fwprintf(logStream, L"回填函数列表：\n");
        for (int i = 0; i < pitches.size(); i++) {
            fwprintf(logStream, L"\t%03u\33[1;32m%ls\33[0m index:%d\n", i, pitches.at(i)->fun->name, pitches.at(i)->index);
        }
    }
#endif
    ~FunCallPitchTable() {
        for (int i = 0; i < pitches.size(); i++) {
#ifdef HX_DEBUG
            fwprintf(logStream, L"释放：%ls\n", pitches.at(i)->fun->name);
#endif
            if (pitches.at(i) != NULL) delete pitches.at(i);
            pitches.at(i) = NULL;
        }
    }
};
enum NodeKind { NODE_VALUE, NODE_VAR, NODE_UNARY, NODE_BINARY, NODE_FUN_CALL };
typedef struct ASTNode {
    IR_DataType resultType;
    NodeKind kind;
    Opcode typeCast;                   // 标记：类型转换
    struct ASTNode* arrayAccessIndex;  // 数组访问的索引表达式, 为NULL表示不是数组访问
    union {
        struct {
            IR_DataType type;
            union {
                double f;
                int32_t i;
                wchar_t* s;
                uint16_t c;
            } val;
        } value;
        struct {
            wchar_t* name;
            int index;
            int blockIndex;
            IR_DataType type;
        } var;
        struct {
            int op;
            wchar_t* varName;  // inc,dic需要
        } unary;
        struct {
            int op;
            wchar_t* varName;  // mov需要
        } binary;              // ADD, SUB, MUL, DIV, MOV, STRING_CONCAT
        struct {
            wchar_t* name;
            FunCallPitch* pitch;
            IR_DataType ret_type;
            struct ASTNode** args;
            uint32_t arg_count;
        } funCall;
    } data;
    struct ASTNode* left;
    struct ASTNode* right;
    Token* token;  // 用于错误定位
} ASTNode;