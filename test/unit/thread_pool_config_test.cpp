/*
ThreadPoolConfig测试用例
使用Google Test框架
*/

#include "thread_pool_improved.h"
#include "logger.h"
#include "thread_pool_config.h"

#include <fstream>

#include <gtest/gtest.h>

// 声明
using thread_pool_improved::ThreadPool;
using thread_pool_improved::QueueFullPolicy;
using thread_pool_improved::ThreadPoolStruct;
using thread_pool_improved::ThreadPoolConfig;
using thread_pool_improved::Logger;
using thread_pool_improved::LoggerConfig;
using thread_pool_improved::LogLevel;

class ThreadPoolConfigTest : public ::testing::Test {
protected:
    void SetUp() override {
        // 初始化日志系统（仅在第一次调用时生效）
        if (!Logger::GetInstance().IsInitialized()) {
            LoggerConfig log_config;
            log_config.name = "test_config";
            log_config.file_path = "../logs/test_config.log";
            log_config.level = LogLevel::DEBUG;  // 设置为DEBUG级别以看到详细日志
            log_config.enable_console = true;
            log_config.enable_file = true;
            log_config.pattern = "[%Y-%m-%d %H:%M:%S.%e] [%^%l%$] [%n] %v";
            Logger::GetInstance().Initialize(log_config);
        }

        // 创建测试目录
        test_dir_ = "test_config_temp";
        std::filesystem::create_directories(test_dir_);
    }

    void TearDown() override {
        // 清理测试文件
        std::filesystem::remove_all(test_dir_);
    }

    // 创建测试JSON文件的辅助函数
    void CreateTestConfigFile(const std::string& filename, const std::string& content) {
        std::ofstream file(test_dir_ + "/" + filename);
        file << content;
        file.close();
    }

protected:
    std::string test_dir_;

};

// 测试从有效的JSON字符串加载配置
TEST_F(ThreadPoolConfigTest, LoadFromString_ValidJson) {
    std::string json_str = R"({
        "core_threads": 4,
        "max_threads": 8,
        "max_queue_size": 1000,
        "queue_full_policy": "BLOCK"
    })";

    ThreadPoolStruct config = ThreadPoolConfig::LoadFromString(json_str);

    EXPECT_EQ(config.core_threads, 4);
    EXPECT_EQ(config.max_threads, 8);
    EXPECT_EQ(config.max_queue_size, 1000);
    EXPECT_EQ(config.queue_full_policy, QueueFullPolicy::BLOCK);
}

// 测试部分配置的JSON（使用默认值）
TEST_F(ThreadPoolConfigTest, LoadFromString_PartialConfig) {
    std::string json_str = R"({
        "core_threads": 6,
        "queue_full_policy": "DISCARD"
    })";
    
    ThreadPoolStruct config = ThreadPoolConfig::LoadFromString(json_str);
    
    EXPECT_EQ(config.core_threads, 6);
    EXPECT_EQ(config.queue_full_policy, QueueFullPolicy::DISCARD);
    // 其他字段应该保持默认值，但max_threads会自动调整为至少等于core_threads
    EXPECT_GE(config.max_threads, config.core_threads);
    EXPECT_EQ(config.max_queue_size, 1000);
}

// 测试不同的队列策略
TEST_F(ThreadPoolConfigTest, LoadFromString_DifferentPolicies) {
    // 测试BLOCK策略
    std::string json_block = R"({"queue_full_policy": "BLOCK"})";
    ThreadPoolStruct config_block = ThreadPoolConfig::LoadFromString(json_block);
    EXPECT_EQ(config_block.queue_full_policy, QueueFullPolicy::BLOCK);
    
    // 测试OVERWRITE策略
    std::string json_overwrite = R"({"queue_full_policy": "OVERWRITE"})";
    ThreadPoolStruct config_overwrite = ThreadPoolConfig::LoadFromString(json_overwrite);
    EXPECT_EQ(config_overwrite.queue_full_policy, QueueFullPolicy::OVERWRITE);
    
    // 测试DISCARD策略
    std::string json_discard = R"({"queue_full_policy": "DISCARD"})";
    ThreadPoolStruct config_discard = ThreadPoolConfig::LoadFromString(json_discard);
    EXPECT_EQ(config_discard.queue_full_policy, QueueFullPolicy::DISCARD);
    
    // 测试小写策略名
    std::string json_lower = R"({"queue_full_policy": "block"})";
    ThreadPoolStruct config_lower = ThreadPoolConfig::LoadFromString(json_lower);
    EXPECT_EQ(config_lower.queue_full_policy, QueueFullPolicy::BLOCK);
}

// 测试从文件加载配置
TEST_F(ThreadPoolConfigTest, LoadFromFile_ValidFile) {
    std::string config_content = R"({
        "core_threads": 2,
        "max_threads": 6,
        "max_queue_size": 500,
        "queue_full_policy": "OVERWRITE"
    })";
    
    CreateTestConfigFile("valid_config.json", config_content);
    
    ThreadPoolStruct config = ThreadPoolConfig::LoadFromFile(test_dir_ + "/valid_config.json");
    
    EXPECT_EQ(config.core_threads, 2);
    EXPECT_EQ(config.max_threads, 6);
    EXPECT_EQ(config.max_queue_size, 500);
    EXPECT_EQ(config.queue_full_policy, QueueFullPolicy::OVERWRITE);
}

// 测试加载不存在的文件
TEST_F(ThreadPoolConfigTest, LoadFromFile_NonExistentFile) {
    EXPECT_THROW(
        ThreadPoolConfig::LoadFromFile("non_existent_file.json"),
        std::runtime_error
    );
}

// 测试无效的JSON格式
TEST_F(ThreadPoolConfigTest, LoadFromString_InvalidJson) {
    std::string invalid_json = R"({
        "core_threads": 4,
        "max_threads": 8,
        "invalid_syntax"
    })";
    
    EXPECT_THROW(
        ThreadPoolConfig::LoadFromString(invalid_json),
        std::invalid_argument
    );
}

// 测试无效的配置值
TEST_F(ThreadPoolConfigTest, LoadFromString_InvalidConfig) {
    // 核心线程数为0
    std::string json_zero_threads = R"({"core_threads": 0})";
    EXPECT_THROW(
        ThreadPoolConfig::LoadFromString(json_zero_threads),
        std::invalid_argument
    );
    
    // 最大线程数小于核心线程数
    std::string json_invalid_max = R"({
        "core_threads": 8,
        "max_threads": 4
    })";

    EXPECT_THROW(
        ThreadPoolConfig::LoadFromString(json_invalid_max),
        std::invalid_argument
    );
}

// 测试无效的队列策略
TEST_F(ThreadPoolConfigTest, LoadFromString_InvalidPolicy) {
    std::string json_invalid_policy = R"({"queue_full_policy": "INVALID_POLICY"})";
    
    EXPECT_THROW(
        ThreadPoolConfig::LoadFromString(json_invalid_policy),
        std::invalid_argument
    );
}

// 测试使用配置创建线程池
TEST_F(ThreadPoolConfigTest, CreateThreadPoolFromConfig) {
    std::string config_content = R"({
        "core_threads": 3,
        "max_threads": 6,
        "max_queue_size": 200,
        "keep_alive_time_ms": 45000,
        "queue_full_policy": "DISCARD"
    })";
    
    CreateTestConfigFile("thread_pool_config.json", config_content);
    
    // 使用静态方法创建线程池
    auto thread_pool = ThreadPool::CreateFromConfig(test_dir_ + "/thread_pool_config.json");
    
    EXPECT_NE(thread_pool, nullptr);
    EXPECT_EQ(thread_pool->GetQueuePolicy(), QueueFullPolicy::DISCARD);
    
    // 测试线程池基本功能
    std::atomic<int> counter{0};
    
    // 提交一些简单任务
    for (int i = 0; i < 5; ++i) {
        thread_pool->SubmitWithResult([&counter]() {
            counter++;
            return counter.load();
        });
    }
    
    thread_pool->WaitAll();
    EXPECT_EQ(counter.load(), 5);
}

// 测试边界值
TEST_F(ThreadPoolConfigTest, LoadFromString_BoundaryValues) {
    // 最小有效配置
    std::string json_min = R"({
        "core_threads": 1,
        "max_threads": 1,
        "max_queue_size": 1,
        "keep_alive_time_ms": 0
    })";
    
    EXPECT_NO_THROW({
        ThreadPoolStruct config = ThreadPoolConfig::LoadFromString(json_min);
        EXPECT_EQ(config.core_threads, 1);
        EXPECT_EQ(config.max_threads, 1);
        EXPECT_EQ(config.max_queue_size, 1);
    });
    
    // 大数值（会产生警告但不会抛出异常）
    std::string json_large = R"({
        "core_threads": 64,
        "max_threads": 128,
        "max_queue_size": 50000
    })";
    
    EXPECT_NO_THROW({
        ThreadPoolStruct config = ThreadPoolConfig::LoadFromString(json_large);
        EXPECT_EQ(config.core_threads, 64);
        EXPECT_EQ(config.max_threads, 128);
        EXPECT_EQ(config.max_queue_size, 50000);
    });
}