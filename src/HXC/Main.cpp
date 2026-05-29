#include "config.h"
#define HXC_VERSION 0.114f
//#define HX_DEBUG
bool isInDebugMode = true;
#include <errno.h>
#include <stdio.h>
#include <time.h>
#include <wchar.h>

#include <string>
#ifdef _WIN32
#include <fcntl.h>
#include <io.h>
#endif

#define log(msg, ...) fwprintf(logStream, L"\33[33m[DEB]\33[0m" msg L"\n", ##__VA_ARGS__)
FILE* outputStream = NULL;
FILE* logStream = NULL;
FILE* errorStream = NULL;
#include "Error.h"
#include "Generator.h"
#include "IR.h"
#include "Lexer.h"
#include "Scanner.h"

int main(int argc, char* argv[]) {
    try {
        initLocale();
        clock_t start, end;
        start = clock();
        outputStream = stdout;
        logStream = stdout;
        errorStream = stderr;
#ifdef _WIN32
        _setmode(_fileno(stdout), _O_U16TEXT);
        _setmode(_fileno(stderr), _O_U16TEXT);
#endif
        /*if (argc < 2) {
            fwprintf(stdout, L"\33[31m[ERR]\33[0m请提供源文件路径！\n");
            return -1;
        }
        */
        std::string path = "";
        std::string objPath = "";
#ifdef HX_DEBUG
        path = "../test/test.hxl";
        objPath = "../test/out.hxo";
#else
        for(int i = 1; i < argc; i++) {
            if(strcmp(argv[i], "-v") == 0 || strcmp(argv[i], "--version") == 0) {
                fwprintf(outputStream, L"\33[1;34m[INFO]\33[0m当时版本是 %f 喵~\n快来操作hxc喵\n", HXC_VERSION);
                return 0;
            } else if(strcmp(argv[i], "-o") == 0 || strcmp(argv[i], "--output") == 0) {
                i++;
                if(i < argc) objPath = argv[i];
            } else {
                path = argv[i];
            }
        }
#endif
        // 读取源文件
        wchar_t* src = NULL;
        FILE* sourceFile = fopen(path.c_str(), "r");
        int scannerError = readSourceFile(sourceFile, &src);
        if (scannerError) {
            fwprintf(outputStream, L"\33[31m[ERR]\33[0m在读取源文件时出错了！\n");
            fwprintf(outputStream, L"\33[31m[ERR]\33[0m编译失败。\n");
            return -1;
        }
#ifdef HX_DEBUG
        fwprintf(outputStream, L"源文件：\n%ls\n", src);
#endif
        if (wcslen(src) == 0) {
            fwprintf(outputStream, L"\33[31m[ERR]\33[0m你的源文件里空空如也！\n");
            fwprintf(outputStream, L"\33[31m[ERR]\33[0m编译失败。\n");
            return 255;
        }
        // 词法分析
        int lexerError = 0;
        fwprintf(outputStream, L"\33[1;34m[INFO]\33[0m正在进行词法分析\n");
        Tokens* tokens = lex(src, &lexerError);
        if (lexerError == 255) {
            fwprintf(errorStream, L"%ls\n", errorMessageBuffer);
            freeTokens(&tokens);
            fwprintf(outputStream, L"\33[31m[ERR]\33[0m编译失败。\n");
            return 255;
        } else if (lexerError == -1) {
            fwprintf(errorStream, L"\33[31m[ERR]\33[0m内存分配失败！\n");
            freeTokens(&tokens);
            return -1;
        }
        free(src);
        src = NULL;
        fwprintf(outputStream, L"\33[1;34m[INFO]\33[0m词法分析完成\n");
#ifdef HX_DEBUG
        showTokens(tokens);
#endif
        // 中间表示生成
        IR_Program* program = NULL;
        int irError = 0;
        program = generateIR(tokens, &irError);

        if (irError == 255) {
            fwprintf(errorStream, L"%ls\n", errorMessageBuffer);
            fwprintf(outputStream, L"\33[31m[ERR]\33[0m编译失败。\n");
            return 255;
        } else if (irError == -1) {
            fwprintf(errorStream, L"\33[31m[ERR]\33[0m内存分配失败！\n");
            return -1;
        }
        fwprintf(outputStream, L"\33[1;34m[INFO]\33[0m中间表示生成完成\n");
#ifdef HX_DEBUG
        showIRProgramInfo(program);
#endif

        // 目标代码生成
        fwprintf(outputStream, L"\33[1;34m[INFO]\33[0m正在生成目标代码\n");
        int genError = 0;
        ObjectCode* objCode = generateObjectCode(program, &genError);
        if (genError == 255) {
            fwprintf(errorStream, L"%ls\n", errorMessageBuffer);
            freeIRProgram(&program);
            fwprintf(outputStream, L"\33[31m[ERR]\33[0m编译失败。\n");
            return 255;
        } else if (genError == -1) {
            fwprintf(errorStream, L"\33[31m[ERR]\33[0m内存分配失败！\n");
            freeIRProgram(&program);
            return -1;
        }

        FILE* objFile = fopen(objPath.c_str(), "wb");
        writeObjectCode(objFile, *objCode);
        freeIRProgram(&program);
        freeTokens(&tokens);
        freeObjectCode(&objCode);
        end = clock();
        fwprintf(outputStream, L"\33[1;34m[INFO]\33[0m编译完成。共耗时%lfs\n", (double)(end - start) / CLOCKS_PER_SEC);
        return 0;
    } catch (std::bad_alloc e) {
        fwprintf(errorStream, L"\33[31m[ERR]\33[0m内存分配失败！\n");
    } catch (std::exception e) {
        fwprintf(errorStream, L"\33[31m[ERR]\33[0m标准库抛异常力！\n");
    } catch (std::out_of_range e) {
        fwprintf(errorStream, L"\33[31m[ERR]\33[0m标准库抛异常力！out_of_range！！！\n");
    }
}