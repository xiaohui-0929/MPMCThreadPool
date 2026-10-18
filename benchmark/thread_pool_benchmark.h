#pragma once

#include "../include/thread_pool_improved.h"

#include <iostream>
#include <fstream>

#include <nlohmann/json.hpp>

namespace benchmark {
// 简单的加一任务
class IncrementTask : public thread_pool_improved::TaskBase {
public:
    explicit IncrementTask(std::atomic<long long>& counter) : counter_(counter) {}

    void Execute() override {
        std::this_thread::sleep_for(std::chrono::microseconds(10));
        counter_.fetch_add(1, std::memory_order_relaxed);
    }
private:
    std::atomic<long long>& counter_;
};

// 压测配置
struct BenchmarkConfig {
    // 线程池配置
    size_t core_threads = 8;
    size_t max_threads = 16;
    size_t max_queue_size = 100000;
    size_t keep_alive_time_ms = 60000;
    std::string queue_full_policy = "BLOCK";
    bool enable_dynamic_threads = false;
    size_t thread_creation_threshold = 1;
    size_t thread_idle_timeout_ms = 30000;
    size_t load_check_interval_ms = 20;
    double scale_up_threshold = 0.8;
    double scale_down_threshold = 0.2;
    size_t min_idle_time_for_removal_ms = 10000;
    size_t max_consecutive_idle_checks = 3;
    
    // 压测配置
    size_t total_tasks = 10000000;              // 任务总数
    size_t duration_seconds = 30;               // 按时间运行模式 运行时间
    size_t warmup_seconds = 5;                  // 预热时间
    bool use_duration_mode = true;              // 使用按时间运行模式
    bool enable_logging = true;                 // 控制是否输出到日志
    bool enable_console_output = false;         // 控制是否输出到控制台
    bool enable_real_time_monitoring = true;
    size_t monitoring_interval_ms = 1000;       // 监控间隔时间

    // 从JSON文件中加载
    static BenchmarkConfig LoadFromFile(const std::string& config_path) {
        BenchmarkConfig config;

        try {
            std::ifstream file(config_path);
            if (!file.is_open()) {
                std::cerr << "警告: 无法打开配置文件 " << config_path << ", 使用默认配置" << std::endl;
                return config;
            }

            nlohmann::json json_config;
            file >> json_config;

            // 加载线程池配置
            if (json_config.contains("thread_pool")) {
                auto& pool_config = json_config["thread_pool"];

                if (pool_config.contains("core_threads"))
                    config.core_threads = pool_config["core_threads"];
                if (pool_config.contains("max_threads"))
                    config.max_threads = pool_config["max_threads"];
                if (pool_config.contains("max_queue_size"))
                    config.max_queue_size = pool_config["max_queue_size"];
                if (pool_config.contains("keep_alive_time_ms"))
                    config.keep_alive_time_ms = pool_config["keep_alive_time_ms"];
                if (pool_config.contains("queue_full_policy"))
                    config.queue_full_policy = pool_config["queue_full_policy"];
                if (pool_config.contains("enable_dynamic_threads"))
                    config.enable_dynamic_threads = pool_config["enable_dynamic_threads"];
                if (pool_config.contains("thread_creation_threshold"))
                    config.thread_creation_threshold = pool_config["thread_creation_threshold"];
                if (pool_config.contains("thread_idle_timeout_ms"))
                    config.thread_idle_timeout_ms = pool_config["thread_idle_timeout_ms"];
                if (pool_config.contains("load_check_interval_ms"))
                    config.load_check_interval_ms = pool_config["load_check_interval_ms"];
                if (pool_config.contains("scale_up_threshold"))
                    config.scale_up_threshold = pool_config["scale_up_threshold"];
                if (pool_config.contains("scale_down_threshold"))
                    config.scale_down_threshold = pool_config["scale_down_threshold"];
                if (pool_config.contains("min_idle_time_for_removal_ms"))
                    config.min_idle_time_for_removal_ms = pool_config["min_idle_time_for_removal_ms"];
                if (pool_config.contains("max_consecutive_idle_checks"))
                    config.max_consecutive_idle_checks = pool_config["max_consecutive_idle_checks"];
            }

            // 加载压测配置
            if (json_config.contains("benchmark")) {
                auto& benchmark_config = json_config["benchmark"];

                if (benchmark_config.contains("total_tasks"))
                    config.total_tasks = benchmark_config["total_tasks"];
                if (benchmark_config.contains("duration_seconds"))
                    config.duration_seconds = benchmark_config["duration_seconds"];
                if (benchmark_config.contains("warmup_seconds"))
                    config.warmup_seconds = benchmark_config["warmup_seconds"];
                if (benchmark_config.contains("use_duration_mode"))
                    config.use_duration_mode = benchmark_config["use_duration_mode"];
                if (benchmark_config.contains("enable_logging"))
                    config.enable_logging = benchmark_config["enable_logging"];
                if (benchmark_config.contains("enable_console_output"))
                    config.enable_console_output = benchmark_config["enable_console_output"];
                if (benchmark_config.contains("enable_real_time_monitoring"))
                    config.enable_real_time_monitoring = benchmark_config["enable_real_time_monitoring"];
                if (benchmark_config.contains("monitoring_interval_ms"))
                    config.monitoring_interval_ms = benchmark_config["monitoring_interval_ms"];
            }

            if (config.enable_console_output) {
                std::cout << "成功从配置文件加载配置: " << config_path << std::endl;
            }
        } catch (const std::exception& e) {
            std::cerr << "警告: 解析配置文件失败: " << e.what() << ", 使用默认配置" << std::endl;
        } catch (...) {
            std::cerr << "警告: 解析配置文件失败: 未知原因" << std::endl;
        }

        return config;
    }
};

// 压测结果
struct BenchmarkResult {
    size_t tasks_completed = 0;                                 // 完成的任务数
    double duration_seconds = 0.0;                              // 实际运行时间
    double throughput_per_second = 0.0;                         // 每秒吞吐量
    size_t peak_threads = 0;                                    // 峰值线程数
    double avg_task_time_ns = 0.0;                              // 平均任务时间(纳秒)
    size_t queue_peak_size = 0;                                 // 队列峰值大小
    
    // 队列使用率相关统计
    size_t max_queue_size = 0;                                  // 队列最大容量
    double peak_queue_usage_rate = 0.0;                         // 队列峰值使用率
    size_t tasks_discarded = 0;                                 // 被丢弃的任务数
    size_t tasks_overwritten = 0;                               // 被覆盖的任务数
    double final_queue_usage_rate = 0.0;                        // 最终队列使用率
};

// 线程池压测类
class ThreadPoolBenchmark {
public:
    explicit ThreadPoolBenchmark(const BenchmarkConfig& config = BenchmarkConfig{});

    // 运行压测
    BenchmarkResult RunBenchmark();

    // 打印结果
    void PrintResult(const BenchmarkResult& result);
private:
    // 按时间运行压测
    BenchmarkResult RunDurationBenchmark();

    // 按任务数运行压测
    BenchmarkResult RunTaskCountBenchmark();

    // 初始化日志系统
    void InitializeLogging();

    // 实时监控线程函数
    void MonitoringLoop(std::shared_ptr<thread_pool_improved::ThreadPool> thread_pool);

private:
    BenchmarkConfig config_;
    std::atomic<long long> task_counter_{0};
    std::atomic<bool> monitoring_active_{false};
};

}