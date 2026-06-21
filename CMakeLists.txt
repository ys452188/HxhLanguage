#########CMake语法好难看啊啊啊啊啊##########

cmake_minimum_required(VERSION 3.5...3.25)

########配置文件
set(CONFIG_FILE "${CMAKE_CURRENT_SOURCE_DIR}/config.txt")

if(EXISTS ${CONFIG_FILE})
    message(STATUS "发现配置文件: ${CONFIG_FILE}，正在加载喵~")
    # 按行读取文件
    file(STRINGS ${CONFIG_FILE} CONFIG_LINES)
    foreach(LINE IN LISTS CONFIG_LINES)
        string(STRIP "${LINE}" LINE)
        if(NOT LINE MATCHES "^#" AND NOT LINE STREQUAL "")
            #严格匹配 KEY=VALUE 结构
            if(LINE MATCHES "^([^=]+)=(.*)$")
                set(KEY ${CMAKE_MATCH_1})
                set(VAL ${CMAKE_MATCH_2})
                string(STRIP "${KEY}" KEY)
                string(STRIP "${VAL}" VAL)
            
                set(${KEY} ${VAL})
                message(STATUS "  配置项: ${KEY} -> ${VAL}")
            else()
                message(WARNING "发现无法解析的配置行: ${LINE}")
            endif()
        endif()
    endforeach()
else()
    set(HX_DEBUG ON)
    message(WARNING "未找到config.txt喵，使用默认配置喵")
endif()

if(HX_DEBUG STREQUAL "ON")
    set(HX_DEBUG_MACRO 1)
else()
    unset(HX_DEBUG_MACRO) 
endif()

if(LANG STREQUAL "zh_TW")
    set(LANG_zh_TW_MACRO 1)
else()
    unset(LANG_zh_TW_MACRO) 
endif() 

if(NOT WIN32)
    if(OBJECT_OS STREQUAL "WINDOWS")
        set(CMAKE_C_COMPILER "x86_64-w64-mingw32-gcc")
        set(CMAKE_CXX_COMPILER "x86_64-w64-mingw32-g++")
        add_compile_options(-static)
        add_compile_options(-static-libgcc)
        add_compile_options(-static-libstdc++)
    endif()
endif()

configure_file(
    "${CMAKE_CURRENT_SOURCE_DIR}/config.h.in"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/HXC/config.h"
)
configure_file(
    "${CMAKE_CURRENT_SOURCE_DIR}/config.h.in"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/HXVM/config.h"
)

project(HxHLanguage VERSION 0.114 LANGUAGES CXX)
#################

if(MSVC)
    add_compile_options(/Zp1)
else()
    add_compile_options(-fno-strict-aliasing) # 禁用严格别名优化
endif()

add_compile_options(-O3)
if(WIN32)
    add_executable(hxc.exe src/HXC/Main.cpp)
else()
    add_executable(hxc src/HXC/Main.cpp) 
endif()

add_compile_options(-flto -fno-exceptions)
if(WIN32)
    add_executable(hxvm.exe src/HXVM/Main.cpp)
else()
    add_executable(hxvm src/HXVM/Main.cpp) 
endif()

if(CMAKE_SYSTEM_PROCESSOR STREQUAL "aarch64" OR CMAKE_SYSTEM_PROCESSOR STREQUAL "arm64")
    # 强制增加 TLS 对齐
    set_target_properties(hxvm PROPERTIES COMPILE_FLAGS "-ftls-model=global-dynamic")
    # 尝试强制对齐
    add_definitions("-falign-functions=64")
endif()
if(CMAKE_SYSTEM_PROCESSOR STREQUAL "aarch64")
    # 强制使用全局动态 TLS 模型
    target_compile_options(hxvm PRIVATE "-ftls-model=global-dynamic")
    #链接器不过于激进地压缩段
    target_link_options(hxvm PRIVATE "-Wl,-z,common-page-size=65536" "-Wl,-z,max-page-size=65536")
endif()

if(OBJECT_OS STREQUAL "ANDROID_TERMUX")
    add_custom_command(
        TARGET hxvm
        POST_BUILD
        COMMAND sh "${CMAKE_CURRENT_SOURCE_DIR}/TermuxPackage.sh"
        COMMENT "打包中"
    )
endif()