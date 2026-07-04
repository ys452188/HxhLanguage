#pragma once
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <wchar.h>
#include <string>
#include <termios.h>

#define HXC_VERSION 0.114f

// 获取隐藏输入
int getch(void) {
    struct termios oldt, newt;
    int ch;
    tcgetattr(STDIN_FILENO, &oldt);
    newt = oldt;
    newt.c_lflag &= ~(ICANON | ECHO); // 关闭缓冲区和回显
    tcsetattr(STDIN_FILENO, TCSANOW, &newt);
    ch = getchar();
    tcsetattr(STDIN_FILENO, TCSANOW, &oldt);
    return ch;
}

int drawGUIMenu(void) noexcept {
    struct winsize size;
    int height, width;
    
    if (ioctl(STDOUT_FILENO, TIOCGWINSZ, &size) == 0) {
        height = size.ws_row;
        width = size.ws_col;
    } else {
        wprintf(L"\33[31m[E]\33[0m获取终端大小失败!\n");
        return -1;
    }
    
    if (height < 10 || width < 40) {
        wprintf(L"\33[31m[E]\33[0m你的终端太小了喵，不跟你做了(至少需要10x40)喵\n");
        return -1;
    }

    int selectedChoice = 0; // 0: 输出到, 1: 编译, 2: 退出
    bool running = true;

    // 隐藏光标
    wprintf(L"\033[?25l");

    while (running) {
        // 清屏并设置背景色
        wprintf(L"\033[2J\033[H\033[48;5;213m"); 
        std::wstring blankLine(width, L' ');
        for (int i = 0; i < height; i++) {
            wprintf(L"%ls", blankLine.c_str());
        }

        // 居中计算
        int startY = (height - 6) / 2;
        int startX = (width - 30) / 2;
        if (startX < 0) startX = 0;

        // 绘制菜单内容
        wprintf(L"\033[%d;%dH\033[1;37m✨ Hello, HXC User! ✨", startY, startX);
        wprintf(L"\033[%d;%dH版本：%.3f", startY + 1, startX, HXC_VERSION);
        
        // 使用高亮显示当前选中的菜单项
        wprintf(L"\033[%d;%dH%ls 输出到 (O)", startY + 2, startX, (selectedChoice == 0 ? L"👉" : L"  "));
        wprintf(L"\033[%d;%dH%ls 编译 (C)", startY + 3, startX, (selectedChoice == 1 ? L"👉" : L"  "));
        wprintf(L"\033[%d;%dH%ls 退出 (Q)", startY + 4, startX, (selectedChoice == 2 ? L"👉" : L"  "));
        wprintf(L"\033[%d;%dH（使用 W/S 或 ⬆️/⬇️ 选择，回车确认喵）", startY + 6, startX);
        
        fflush(stdout);

        // 处理键盘输入
        int inputKey = getch();
        if (inputKey == 'w' || inputKey == 'W') {
            selectedChoice = (selectedChoice - 1 + 3) % 3;
        } else if (inputKey == 's' || inputKey == 'S') {
            selectedChoice = (selectedChoice + 1) % 3;
        } else if (inputKey == 10) { // 回车键
            if (selectedChoice == 0) {
                wprintf(L"\033[%d;%dH请输入目标路径: ", startY + 5, startX);
                wprintf(L"\033[?25h"); // 恢复光标以供输入
                getch(); 
                wprintf(L"\033[?25l");
            } else if (selectedChoice == 1) {
                // 编译
                running = false; 
            } else if (selectedChoice == 2) {
                // 退出
                wprintf(L"\033[?25h\033[0m\033[2J\033[H");
                return 255;
            }
        } else if (inputKey == 'q' || inputKey == 'Q') {
            running = false;
            wprintf(L"\033[?25h\033[0m\033[2J\033[H");
            return -1;
        } else if (inputKey == 27) { // 处理 ANSI 转义组合键（如方向键）
            if (getch() == 91) {
                int arrow = getch();
                if (arrow == 65) selectedChoice = (selectedChoice - 1 + 3) % 3; // ⬆️
                if (arrow == 66) selectedChoice = (selectedChoice + 1) % 3;     // ⬇️
            }
        }
    }

    // 退出 TUI 恢复终端正常状态
    wprintf(L"\033[?25h\033[0m\033[2J\033[H");
    fflush(stdout);
    return 0; // 返回0让 Main.cpp 继续往下走去编译
}