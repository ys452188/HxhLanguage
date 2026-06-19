#include <atomic>

#include "config.h"
// #define HX_DEBUG
std::atomic<bool> shouldExit{false};  // 要退出吗，用于处理SIGINT
#define OP_STACK_SIZE 512             // 操作数栈大小
#define HXVM_VERSION 0.114f
#define ERR_LABEL L"\33[1;31m[E]\33[0m"
#define LOG_LABEL L"\33[1;33m[LOG]\33[0m"
#define INFO_LABEL L"\33[1;34m[INFO]\33[0m"
#define CALL_DEPTH_MAX ((const unsigned int)2000)  // 允许递归调用的最多次数
typedef unsigned char byte;
inline void initLocale(void);
#include <locale.h>
#include <stdint.h>
#include <stdio.h>
#include <time.h>

#include <chrono>
#include <csignal>

#include "HxVector.h"
#define errorStream stdout

#include <pthread.h>

#include <iostream>

#include "Interpreter.h"
#include "ObjectReader.h"
static void signalHandler(int signalNumber);
extern int interpret(ObjectCode&, int& err) noexcept;
typedef struct InterpretData {
    int* err;
    int* ret;
    ObjectCode* obj;
} InterpretData;
void* interpret_packed(void* dataPtr) {
    InterpretData data = *((InterpretData*)dataPtr);
    *(data.ret) = interpret(*(data.obj), *(data.ret), *(data.err));
    return NULL;
}

int main(int argc, char** argv) {
    // 注册 Ctrl+C 信号
    std::signal(SIGINT, signalHandler);
    auto start = std::chrono::high_resolution_clock::now();
    initLocale();
    std::string path = "";
    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "-v") == 0 || strcmp(argv[i], "--version") == 0) {
            wprintf(INFO_LABEL L"当时版本是 %f 喵~\n快来操作hxvm喵\n", HXVM_VERSION);
            return 0;
        } else {
            path = argv[i];
        }
    }
    wprintf(INFO_LABEL L"开始\n");

    memoryAllocer;
    ObjectCode objCode = {};
#ifdef HX_DEBUG
    path = "../test/out.hxo";
#endif
    FILE* file = fopen(path.c_str(), "rb");
    // 读
    if (readObjectCode(file, objCode)) {
        fwprintf(errorStream, ERR_LABEL L"打开文件时发生错误！\n");
        return -1;
    }
#ifdef HX_DEBUG
    wprintf(LOG_LABEL L"读取%d个过程\n", objCode.procedureSize);
    wprintf(LOG_LABEL L"入口索引：%d\n", objCode.start);
#endif
    int err = 0;
    int ret = 0;
    pthread_t mainThread = {};
    InterpretData data = {};
    data.err = &err;
    data.obj = &objCode;
    data.ret = &ret;
    if (pthread_create(&mainThread, NULL, interpret_packed, &data)) {
        auto end = std::chrono::high_resolution_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end - start).count();
        fwprintf(errorStream, ERR_LABEL L"寄！线程创建失败！共存活%lfs\n", (double)((duration) / 1000000.0));
    }
    pthread_join(mainThread, NULL);
    if (err || ret) {
        auto end = std::chrono::high_resolution_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end - start).count();
        fwprintf(errorStream, ERR_LABEL L"寄！运行时发生异常！共存活%lfs\n", (double)((duration) / 1000000.0));
        return -1;
    }

    freeObjectCode(objCode);

    auto end = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end - start).count();
    wprintf(INFO_LABEL L"结束 耗时%lfs\n", (double)((duration) / 1000000.0));
    return 0;
}
void signalHandler(int signalNumber) {
    if (signalNumber == SIGINT) {
        wprintf(INFO_LABEL L"\33[1;31m不......不要停♡\33[0m\n");
        shouldExit.store(true);
    } else if (signalNumber == SIGSEGV) {
        wprintf(INFO_LABEL L"\33[1;31m段内存犯规了！\33[0m\n");
        shouldExit.store(true);
    }
}
void initLocale(void) {
    // 设置Locale
    setlocale(LC_ALL, "C.UTF-8");
    // 设置宽字符流的定向
    fwide(stdout, 1);  // 1 = 宽字符定向
#ifdef _WIN32
    _setmode(_fileno(stdout), _O_U16TEXT);
    _setmode(_fileno(stderr), _O_U16TEXT);
#endif
    return;
}