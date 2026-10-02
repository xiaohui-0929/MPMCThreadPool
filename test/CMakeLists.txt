cmake_minimum_required(VERSION 3.10)
project(ThreadPoolTest)

# 设置C++标准
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# 查找 Google Test
find_package(GTest REQUIRED)

# 包含头文件目录
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/../include)

# 创建可执行文件
add_executable(circular_queue_test
    unit/circular_q_test.cpp)

add_executable(mpmc_blocking_queue_test
    unit/mpmc_blocking_q_test.cpp)

add_executable(thread_pool_test
    unit/thread_pool_test.cpp)

# 链接必要的库
target_link_libraries(circular_queue_test
    GTest::gtest
    GTest::gtest_main
    thread_pool_lib)

target_link_libraries(mpmc_blocking_queue_test
    GTest::gtest
    GTest::gtest_main
    thread_pool_lib)

target_link_libraries(thread_pool_test
    GTest::gtest
    GTest::gtest_main
    thread_pool_lib)

# 启用测试
enable_testing()
add_test(NAME CirculatQueueTests COMMAND circular_queue_test)
add_test(NAME MPMCBlockingQueueTests COMMAND mpmc_blocking_queue_test)
add_test(NAME ThreadPoolTests COMMAND thread_pool_test)

# 添加编译选项
target_compile_options(circular_queue_test PRIVATE
    -Wall -Wextra -Wpedantic)
target_compile_options(mpmc_blocking_queue_test PRIVATE
    -Wall -Wextra -Wpedantic)
target_compile_options(thread_pool_test PRIVATE
    -Wall -Wextra -Wpedantic)