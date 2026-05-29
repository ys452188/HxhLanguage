#ifndef HXHLANG_SRC_HXC_CHECKER_H
#define HXHLANG_SRC_HXC_CHECKER_H
#include <wchar.h>
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
#endif