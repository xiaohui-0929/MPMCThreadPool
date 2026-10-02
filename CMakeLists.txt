cmake_minimum_required(VERSION 3.10)
project(ThreadPool)

# 设置输出完整、详细的编译命令
#set(CMAKE_VERBOSE_MAKEFILE ON)

# 设置C++标准
set(CMAKE_CXX_STANDARD 17)
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

# 创建静态库
add_library(thread_pool_lib 
    src/thread_pool_improved.cpp
)

# 链接依赖
target_link_libraries(thread_pool_lib
    Threads::Threads
)


# 添加测试子目录
add_subdirectory(test)

