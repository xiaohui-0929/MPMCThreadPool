cmake_minimum_required(VERSION 3.10)
project(ThreadPool)

# 设置输出完整、详细的编译命令
set(CMAKE_VERBOSE_MAKEFILE ON)

# 设置C++标准
set(CMAKE_CXX_STANDARD 14)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# 编译器特定优化
if(CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -Wextra -O2")
elseif(CMAKE_CXX_COMPILER_ID STREQUAL "Clang")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -Wextra -O2")
endif()

# 查找依赖包
find_package(nlohmann_json CONFIG REQUIRED)
find_package(Threads REQUIRED)

# 设置头文件路径
include_directories(include)

# todo 定义源文件
#set(THREAD_POOL_SOURCES
    #src/thread_pool_improved.cpp
    #src/thread_pool_config.cpp
    #src/logger.cpp
#)

# todo 创建静态库
#add_library(thread_pool_static STATIC $(THREAD_POOL_SOURCES))

# 创建可执行文件（验证环境搭建是否成功）
add_executable(test_installation src/test_installation.cpp)

# todo 链接依赖
