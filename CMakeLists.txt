cmake_minimum_required(VERSION 3.16)
project(hard2Ser_3_0 LANGUAGES C CXX)

# 设置 C++ 标准
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# 强制设置运行库为 /MD (MultiThreadedDLL)，即使在 Debug 下也如此，
# 解决与阿里云 OSS C++ SDK (Release 模式) 的 Iterator Debug Level 冲突问题
set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreadedDLL")

# 开启 Qt 自动编译器处理（MOC/UIC/RCC）
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTORCC ON)

# MSVC 编译选项优化 (解决中文编码警告，启用多处理器并行编译)
if(MSVC)
    add_compile_options(/utf-8 /MP /Zc:char8_t-)
    add_compile_definitions(UNICODE _UNICODE)
endif()

# 查找 Qt 5 依赖库
find_package(Qt5 COMPONENTS Core Gui Widgets REQUIRED)

# 设置头文件包含目录
include_directories(
    ${CMAKE_CURRENT_SOURCE_DIR}/src
    ${CMAKE_CURRENT_SOURCE_DIR}/src/comm
    ${CMAKE_CURRENT_SOURCE_DIR}/src/ipc
    ${CMAKE_CURRENT_SOURCE_DIR}/src/ui
    ${CMAKE_CURRENT_SOURCE_DIR}/src/work
    ${CMAKE_CURRENT_SOURCE_DIR}/3rdParty/networkKit/include
    ${CMAKE_CURRENT_SOURCE_DIR}/3rdParty/networkKit/include/hv
    ${CMAKE_CURRENT_SOURCE_DIR}/3rdParty/aliCloudOss/inc
)

# 递归寻找所有源文件与头文件、UI文件、资源文件
file(GLOB_RECURSE SRC_FILES
    "src/*.cpp"
    "src/*.c"
    "src/*.h"
    "src/*.ui"
    "src/*.qrc"
)

# 设置库文件包含目录
link_directories(
    ${CMAKE_CURRENT_SOURCE_DIR}/3rdParty/networkKit/lib
    ${CMAKE_CURRENT_SOURCE_DIR}/3rdParty/aliCloudOss/lib/alibabacloud/oss
)

# 生成可执行文件
add_executable(${PROJECT_NAME} ${SRC_FILES})

# 链接 Qt 模块与第三方库
target_link_libraries(${PROJECT_NAME}
    PRIVATE
    Qt5::Core
    Qt5::Gui
    Qt5::Widgets
    hv                             # 链接 libhv 动态导入库 (原本为 hv_static)
    alibabacloud-oss-cpp-sdk       # 阿里云 OSS SDK
    libcurl                        # Curl
    libeay32                       # OpenSSL Crypto
    ssleay32                       # OpenSSL SSL
    Ws2_32                         # Windows 套接字依赖
    Iphlpapi                       # IP 帮助 API
)

# 拷贝依赖的第三方 DLL 到可执行文件输出路径 (针对 Windows 动态库运行)
add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "${CMAKE_CURRENT_SOURCE_DIR}/3rdParty/networkKit/bin/hv.dll"
        "$<TARGET_FILE_DIR:${PROJECT_NAME}>"
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "${CMAKE_CURRENT_SOURCE_DIR}/3rdParty/aliCloudOss/bin/libcurl.dll"
        "$<TARGET_FILE_DIR:${PROJECT_NAME}>"
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "${CMAKE_CURRENT_SOURCE_DIR}/3rdParty/aliCloudOss/bin/libeay32.dll"
        "$<TARGET_FILE_DIR:${PROJECT_NAME}>"
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "${CMAKE_CURRENT_SOURCE_DIR}/3rdParty/aliCloudOss/bin/ssleay32.dll"
        "$<TARGET_FILE_DIR:${PROJECT_NAME}>"
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "${CMAKE_CURRENT_SOURCE_DIR}/3rdParty/aliCloudOss/bin/zlibwapi.dll"
        "$<TARGET_FILE_DIR:${PROJECT_NAME}>"
)
