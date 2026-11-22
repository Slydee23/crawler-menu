cmake_minimum_required(VERSION 3.18)
project(crawlermenu C CXX ASM)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# ShadowHook (MIT, https://github.com/bytedance/android-inline-hook), built as a static
# library from the submodule sources. Its own CMakeLists needs CMake 4 and builds a shared
# lib with its own JNI_OnLoad, so we compile the sources directly and leave out sh_jni.c.
set(SH_DIR ${CMAKE_CURRENT_SOURCE_DIR}/third_party/shadowhook/shadowhook/src/main/cpp)
file(GLOB SH_SRC
    ${SH_DIR}/*.c
    ${SH_DIR}/arch/arm64/*.c
    ${SH_DIR}/arch/arm64/*.S
    ${SH_DIR}/common/*.c
    ${SH_DIR}/third_party/*/*.c)
list(FILTER SH_SRC EXCLUDE REGEX "sh_jni\.c$")
add_library(shadowhook_static STATIC ${SH_SRC})
target_compile_features(shadowhook_static PRIVATE c_std_11)
target_include_directories(shadowhook_static
    PUBLIC ${SH_DIR}/include ${SH_DIR}/third_party/xdl
    PRIVATE ${SH_DIR} ${SH_DIR}/arch/arm64 ${SH_DIR}/common ${SH_DIR}/third_party/bsd ${SH_DIR}/third_party/lss)
target_compile_options(shadowhook_static PRIVATE -Os -ffunction-sections -fdata-sections -fvisibility=hidden)

add_library(crawlermenu SHARED
    src/main.cpp
    src/il2cpp.cpp
)
target_include_directories(crawlermenu PRIVATE src)
target_compile_options(crawlermenu PRIVATE -fvisibility=hidden -fno-exceptions -fno-rtti -Wall -Wextra)
target_link_libraries(crawlermenu PRIVATE shadowhook_static log dl)
target_link_options(crawlermenu PRIVATE
    -Wl,--gc-sections -Wl,--exclude-libs,ALL -Wl,-z,max-page-size=16384 -s)

# ShadowHook dlopen()s this empty helper during init to verify its linker hooks.
# It must ship next to libcrawlermenu.so in the APK's lib/arm64-v8a/.
add_library(shadowhook_nothing SHARED ${SH_DIR}/nothing/sh_nothing.c)
target_link_options(shadowhook_nothing PRIVATE
    -Wl,-z,max-page-size=16384 -Wl,--strip-all -Wl,--build-id=none -nostdlib -nostartfiles -nodefaultlibs)
add_dependencies(crawlermenu shadowhook_nothing)
