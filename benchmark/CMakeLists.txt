cmake_minimum_required(VERSION 3.10)

# 设置项目名称
project(ThreadPoolBenchmark)

# 设置C++标准
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# 设置编译选项
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -O2 -pthread")

# 包含头文件目录
include_directories(../include)

# 添加源文件
set(SOURCES
    main.cpp
    thread_pool_benchmark.cpp
    # ../src/thread_pool_improved.cpp
    # ../src/thread_pool_config.cpp
    # ../src/logger.cpp
)

# 创建可执行文件
add_executable(thread_pool_benchmark ${SOURCES})

# 查找并链接必要的库
# find_package(nlohmann_json CONFIG REQUIRED)
# find_package(Threads REQUIRED)
# find_package(spdlog REQUIRED)

target_link_libraries(thread_pool_benchmark 
    PRIVATE
    thread_pool_lib
    Threads::Threads
    spdlog::spdlog
    nlohmann_json::nlohmann_json
)

# 设置输出目录
set_target_properties(thread_pool_benchmark
    PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin
)