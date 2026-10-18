/*
压测代码
*/
#include "thread_pool_benchmark.h"
#include "../include/logger.h"

namespace benchmark {
ThreadPoolBenchmark::ThreadPoolBenchmark(const BenchmarkConfig& config) 
    : config_(config) {
    if (config_.enable_logging) {
        InitializeLogging();
    }
}

// 运行压测
BenchmarkResult ThreadPoolBenchmark::RunBenchmark() {
    if (config_.enable_console_output) {
        std::cout << "=== 线程池吞吐量压测开始 ===" << std::endl;
        std::cout << "核心线程数: " << config_.core_threads << ", 最大线程数: " << config_.max_threads << std::endl;
    }

    if (config_.enable_logging) {
        LOG_INFO("开始线程池压测 - 核心线程: {}, 最大线程: {}, 队列大小: {}", 
                config_.core_threads, config_.max_threads, config_.max_queue_size);
    }

    if (config_.use_duration_mode) {
        if (config_.enable_console_output) {
            std::cout << "测试模式: 按时间测试 (" << config_.duration_seconds << "秒)" << std::endl;
            std::cout << "预热时间: " << config_.warmup_seconds << "秒" << std::endl;
        }
        return RunDurationBenchmark();
    } else {
        if (config_.enable_console_output) {
            std::cout << "测试模式: 按任务数测试 (" << config_.total_tasks << "个任务)" << std::endl;
        }
        return RunTaskCountBenchmark();
    }
}

// 打印结果
void ThreadPoolBenchmark::PrintResult(const BenchmarkResult& result) {
    if (config_.enable_console_output) {
        std::cout << "\n=== 压测结果 ===" << std::endl;
        std::cout << std::fixed << std::setprecision(2);
        
        std::cout << "完成任务数: " << result.tasks_completed << std::endl;
        std::cout << "运行时间: " << result.duration_seconds << " 秒" << std::endl;
        std::cout << "吞吐量: " << result.throughput_per_second << " 任务/秒" << std::endl;
        std::cout << "峰值线程数: " << result.peak_threads << std::endl;
        
        if (result.avg_task_time_ns > 0) {
            std::cout << "平均任务时间: " << result.avg_task_time_ns << " 纳秒" << std::endl;
        }
        
        // 队列使用率相关信息
        std::cout << "\n=== 队列使用率统计 ===" << std::endl;
        std::cout << "队列最大容量: " << result.max_queue_size << std::endl;
        std::cout << "队列峰值大小: " << result.queue_peak_size << std::endl;
        std::cout << std::fixed << std::setprecision(2);
        std::cout << "队列峰值使用率: " << (result.peak_queue_usage_rate * 100.0) << "%" << std::endl;
        std::cout << "最终队列使用率: " << (result.final_queue_usage_rate * 100.0) << "%" << std::endl;
        
        if (result.tasks_discarded > 0) {
            std::cout << "丢弃任务数: " << result.tasks_discarded << std::endl;
            double discard_rate = static_cast<double>(result.tasks_discarded) / 
                                 (result.tasks_completed + result.tasks_discarded) * 100.0;
            std::cout << "任务丢弃率: " << discard_rate << "%" << std::endl;
        }
        
        if (result.tasks_overwritten > 0) {
            std::cout << "覆盖任务数: " << result.tasks_overwritten << std::endl;
        }
        
        // 队列使用率评估
        std::cout << "\n=== 队列使用率评估 ===" << std::endl;
        if (result.peak_queue_usage_rate > 0.9) {
            std::cout << "队列状态: 高负载 (峰值使用率 >90%)" << std::endl;
            std::cout << "建议: 考虑增加队列容量或优化任务处理速度" << std::endl;
        } else if (result.peak_queue_usage_rate > 0.7) {
            std::cout << "队列状态: 中等负载 (峰值使用率 >70%)" << std::endl;
            std::cout << "建议: 监控队列使用情况，必要时调整容量" << std::endl;
        } else if (result.peak_queue_usage_rate > 0.3) {
            std::cout << "队列状态: 轻度负载 (峰值使用率 >30%)" << std::endl;
            std::cout << "建议: 队列容量适中" << std::endl;
        } else {
            std::cout << "队列状态: 低负载 (峰值使用率 <30%)" << std::endl;
            std::cout << "建议: 可以考虑减少队列容量以节省内存" << std::endl;
        }
        
        // 性能评估
        double tasks_per_thread_per_second = result.throughput_per_second / config_.core_threads;
        std::cout << "每线程吞吐量: " << tasks_per_thread_per_second << " 任务/秒/线程" << std::endl;
        
        std::cout << "\n=== 性能评估 ===" << std::endl;
        if (result.throughput_per_second > 100000) {
            std::cout << "性能等级: 优秀 (>100K 任务/秒)" << std::endl;
        } else if (result.throughput_per_second > 50000) {
            std::cout << "性能等级: 良好 (>50K 任务/秒)" << std::endl;
        } else if (result.throughput_per_second > 10000) {
            std::cout << "性能等级: 一般 (>10K 任务/秒)" << std::endl;
        } else {
            std::cout << "性能等级: 需要优化 (<10K 任务/秒)" << std::endl;
        }
        
        std::cout << "==================" << std::endl;
    }
    
    if (config_.enable_logging) {
        LOG_INFO("压测完成 - 吞吐量: {:.2f} 任务/秒, 峰值线程: {}, 性能等级: {}", 
                result.throughput_per_second, result.peak_threads,
                result.throughput_per_second > 100000 ? "优秀" : 
                result.throughput_per_second > 50000 ? "良好" : 
                result.throughput_per_second > 10000 ? "一般" : "需要优化");
    }
}

// 按时间运行压测
BenchmarkResult ThreadPoolBenchmark::RunDurationBenchmark() {
    BenchmarkResult result;

    // 创建线程池配置 - 使用配置文件中的完整参数
    thread_pool_improved::ThreadPoolStruct pool_config;
    pool_config.core_threads = config_.core_threads;
    pool_config.max_threads = config_.max_threads;
    pool_config.max_queue_size = config_.max_queue_size;
    // 解析队列满策略
    if (config_.queue_full_policy == "BLOCK") {
        pool_config.queue_full_policy = thread_pool_improved::QueueFullPolicy::BLOCK;
    } else if (config_.queue_full_policy == "DISCARD") {
        pool_config.queue_full_policy = thread_pool_improved::QueueFullPolicy::DISCARD;
    } else {
        pool_config.queue_full_policy = thread_pool_improved::QueueFullPolicy::BLOCK;
    }
    // 动态线程管理参数
    pool_config.enable_dynamic_threads = config_.enable_dynamic_threads;
    pool_config.thread_creation_threshold = config_.thread_creation_threshold;
    pool_config.thread_idle_timeout = std::chrono::milliseconds(config_.thread_idle_timeout_ms);
    pool_config.load_check_interval = std::chrono::milliseconds(config_.load_check_interval_ms);
    pool_config.scale_up_threshold = config_.scale_up_threshold;
    pool_config.scale_down_threshold = config_.scale_down_threshold;
    pool_config.min_idle_time_for_removal = std::chrono::milliseconds(config_.min_idle_time_for_removal_ms);
    pool_config.max_consecutive_idle_checks = config_.max_consecutive_idle_checks;

    if (config_.enable_logging) {
        LOG_INFO("线程池配置 - 核心线程: {}, 最大线程: {}, 队列大小: {}", 
                pool_config.core_threads, pool_config.max_threads, pool_config.max_queue_size);
    }

    // 创建线程池
    auto thread_pool = std::make_unique<thread_pool_improved::ThreadPool>(pool_config);
    // 重置计数器
    task_counter_.store(0);

    if (config_.enable_console_output) {
        std::cout << "开始预热..." << std::endl;
    }
    if (config_.enable_logging) {
        LOG_INFO("开始预热阶段 - 预热时间: {}秒", config_.warmup_seconds);
    }

    // 预热阶段
    auto warmup_start = std::chrono::high_resolution_clock::now();
    auto warmup_end = warmup_start + std::chrono::seconds(config_.warmup_seconds);

    while (std::chrono::high_resolution_clock::now() < warmup_end) {
        auto task = std::make_unique<IncrementTask>(task_counter_);
        thread_pool->Submit(std::move(task));
    }

    // 等待预热完成
    thread_pool->WaitAll();

    if (config_.enable_console_output) {
        std::cout << "预热完成，开始正式测试..." << std::endl;
    }
    if (config_.enable_logging) {
        LOG_INFO("预热完成，线程池当前状态 - 活跃线程: {}, 队列大小: {}", 
                thread_pool->ActiveThreads(), thread_pool->QueueSize());
    }

    // 重置计数器 开始正式测试
    task_counter_.store(0);

    auto test_start = std::chrono::high_resolution_clock::now();
    auto test_end = test_start + std::chrono::seconds(config_.duration_seconds);

    size_t tasks_submitted = 0;
    size_t max_queue_size = 0;

    // 开启一个新线程 用于实时监控
    std::thread monitoring_thread;
    if (config_.enable_real_time_monitoring) {
        monitoring_active_.store(true);
        // 创建一个指向thread_pool对应内存的shared_ptr
        std::shared_ptr<thread_pool_improved::ThreadPool> shared_pool(thread_pool.get(), 
            [](thread_pool_improved::ThreadPool*){});
        monitoring_thread = std::thread(&ThreadPoolBenchmark::MonitoringLoop, this, shared_pool);
    }

    if (config_.enable_logging) {
        LOG_INFO("开始正式测试 - 测试时间: {}秒, 实时监控: {}", 
                config_.duration_seconds, config_.enable_real_time_monitoring ? "启用" : "禁用");
    }

    // 主测试循环 在配置时间内不断提交任务
    while (std::chrono::high_resolution_clock::now() < test_end) {
        auto task = std::make_unique<IncrementTask>(task_counter_);
        bool submitted = thread_pool->Submit(std::move(task));
        
        if (submitted) {
            tasks_submitted++;
            max_queue_size = std::max(max_queue_size, thread_pool->QueueSize());
        }
    }

    // 停止监控线程
    if (config_.enable_real_time_monitoring && monitoring_thread.joinable()) {
        monitoring_active_.store(false);
        monitoring_thread.join();
    }

    auto actual_test_end = std::chrono::high_resolution_clock::now();
    
    if (config_.enable_console_output) {
        std::cout << "测试时间结束，等待所有任务完成..." << std::endl;
    }

    // 等待所有任务完成
    thread_pool->WaitAll();

    // auto final_end = std::chrono::high_resolution_clock::now();

    // 计算结果
    result.tasks_completed = task_counter_.load();
    result.duration_seconds = std::chrono::duration<double>(actual_test_end - test_start).count();
    result.throughput_per_second = result.tasks_completed / result.duration_seconds;

    // 获取完整的统计信息
    auto stats = thread_pool->GetStats();
    result.peak_threads = stats.peak_threads;
    result.queue_peak_size = stats.peak_queue_size;
    result.max_queue_size = stats.max_queue_size;
    result.peak_queue_usage_rate = stats.peak_queue_usage_rate;
    result.tasks_discarded = stats.tasks_discarded;
    result.tasks_overwritten = stats.tasks_overwritten;
    result.final_queue_usage_rate = stats.queue_usage_rate;

    if (result.tasks_completed > 0) {
        result.avg_task_time_ns = (result.duration_seconds * 1e9) / result.tasks_completed;
    }
    
    if (config_.enable_console_output) {
        std::cout << "所有任务执行完成！" << std::endl;
    }
    
    return result;
}

// 按任务数运行压测
BenchmarkResult ThreadPoolBenchmark::RunTaskCountBenchmark() {
    BenchmarkResult result;
    
    // 创建线程池配置 - 使用配置文件中的完整参数
    thread_pool_improved::ThreadPoolStruct pool_config;
    pool_config.core_threads = config_.core_threads;
    pool_config.max_threads = config_.max_threads;
    pool_config.max_queue_size = config_.max_queue_size;
    // 解析队列满策略
    if (config_.queue_full_policy == "BLOCK") {
        pool_config.queue_full_policy = thread_pool_improved::QueueFullPolicy::BLOCK;
    } else if (config_.queue_full_policy == "DISCARD") {
        pool_config.queue_full_policy = thread_pool_improved::QueueFullPolicy::DISCARD;
    } else {
        pool_config.queue_full_policy = thread_pool_improved::QueueFullPolicy::BLOCK;
    }
    // 动态线程管理参数
    pool_config.enable_dynamic_threads = config_.enable_dynamic_threads;
    pool_config.thread_creation_threshold = config_.thread_creation_threshold;
    pool_config.thread_idle_timeout = std::chrono::milliseconds(config_.thread_idle_timeout_ms);
    pool_config.load_check_interval = std::chrono::milliseconds(config_.load_check_interval_ms);
    pool_config.scale_up_threshold = config_.scale_up_threshold;
    pool_config.scale_down_threshold = config_.scale_down_threshold;
    pool_config.min_idle_time_for_removal = std::chrono::milliseconds(config_.min_idle_time_for_removal_ms);
    pool_config.max_consecutive_idle_checks = config_.max_consecutive_idle_checks;

    if (config_.enable_logging) {
        LOG_INFO("线程池配置 - 核心线程: {}, 最大线程: {}, 队列大小: {}", 
                pool_config.core_threads, pool_config.max_threads, pool_config.max_queue_size);
    }
    
    // 创建线程池
    auto thread_pool = std::make_unique<thread_pool_improved::ThreadPool>(pool_config);
    
    // 重置计数器
    task_counter_.store(0);
    
    std::cout << "开始提交 " << config_.total_tasks << " 个任务..." << std::endl;
    
    auto start_time = std::chrono::high_resolution_clock::now();

    // 使用多线程并发提交任务以提高队列利用率
    const size_t submit_thread_count = 4;
    const size_t tasks_per_thread = config_.total_tasks / submit_thread_count;
    const size_t remaining_tasks = config_.total_tasks % submit_thread_count;

    std::cout << "使用 " << submit_thread_count << " 个线程并发提交任务以提高队列利用率..." << std::endl;

    std::vector<std::thread> submit_threads;
    std::atomic<size_t> submitted_count{0};

    // 创建提交线程
    for (size_t t = 0; t < submit_thread_count; ++t) {
        size_t start_task = t * tasks_per_thread;
        size_t end_task = (t == submit_thread_count - 1) ? 
                         start_task + tasks_per_thread + remaining_tasks : 
                         start_task + tasks_per_thread;
        submit_threads.emplace_back([this, &thread_pool, start_task, end_task, &submitted_count]() {
            // 批量提交任务
            const size_t batch_size = 1000;
            std::vector<std::unique_ptr<IncrementTask>> task_batch;
            task_batch.reserve(batch_size);

            for (size_t i = start_task; i < end_task; ++i) {
                task_batch.emplace_back(std::make_unique<IncrementTask>(task_counter_));
                // 当批次满了或者是最后一批时，提交所有任务
                if (task_batch.size() == batch_size || i == end_task - 1) {
                    for (auto& task : task_batch) {
                        thread_pool->Submit(std::move(task));
                    }
                    submitted_count.fetch_add(task_batch.size());
                    task_batch.clear();

                    // 如果队列使用率过低，稍微减慢提交速度让队列有机会积累
                    auto stats = thread_pool->GetStats();
                    if (stats.queue_usage_rate < 0.1) {  // 队列使用率低于10%时稍作延迟
                        std::this_thread::sleep_for(std::chrono::microseconds(100));
                    }
                }
            }
        });
    }

    // 监控提交进度
    std::thread progress_thread([&submitted_count, this]() {
        size_t last_count = 0;
        while (submitted_count.load() < config_.total_tasks) {
            std::this_thread::sleep_for(std::chrono::milliseconds(500));
            size_t current_count = submitted_count.load();
            if (current_count != last_count) {
                std::cout << "已提交: " << current_count << " / " << config_.total_tasks 
                         << " (" << (current_count * 100.0 / config_.total_tasks) << "%)" << std::endl;
                last_count = current_count;
            }
        }
    });

    // 等待所有提交线程完成
    for (auto& t : submit_threads) {
        t.join();
    }
    progress_thread.join();
    
    std::cout << "所有任务已提交，等待执行完成..." << std::endl;
    
    // 等待所有任务完成
    thread_pool->WaitAll();
    
    auto end_time = std::chrono::high_resolution_clock::now();

    // 计算结果
    result.tasks_completed = task_counter_.load();
    result.duration_seconds = std::chrono::duration<double>(end_time - start_time).count();
    result.throughput_per_second = result.tasks_completed / result.duration_seconds;
    
    // 获取完整的统计信息
    auto stats = thread_pool->GetStats();
    result.peak_threads = stats.peak_threads;
    result.queue_peak_size = stats.peak_queue_size;
    result.max_queue_size = stats.max_queue_size;
    result.peak_queue_usage_rate = stats.peak_queue_usage_rate;
    result.tasks_discarded = stats.tasks_discarded;
    result.tasks_overwritten = stats.tasks_overwritten;
    result.final_queue_usage_rate = stats.queue_usage_rate;
    
    if (result.tasks_completed > 0) {
        result.avg_task_time_ns = (result.duration_seconds * 1e9) / result.tasks_completed;
    }
    
    return result;
}

// 初始化日志系统
void ThreadPoolBenchmark::InitializeLogging() {
    thread_pool_improved::LoggerConfig log_config;
    log_config.name = "benchmark";
    log_config.level = thread_pool_improved::LogLevel::INFO;
    log_config.enable_console = false;  // 禁用控制台输出
    log_config.enable_file = true;
    log_config.file_path = "../../logs/benchmark.log";
    log_config.pattern = "[%Y-%m-%d %H:%M:%S.%e] [%^%l%$] [benchmark] %v";
    
    auto& logger = thread_pool_improved::Logger::GetInstance();
    if (!logger.IsInitialized()) {
        bool success = logger.Initialize(log_config);
        if (success) {
            LOG_INFO("压测日志系统初始化成功");
        } else {
            std::cerr << "警告: 压测日志系统初始化失败" << std::endl;
        }
    }
}

// 实时监控线程函数
void ThreadPoolBenchmark::MonitoringLoop(std::shared_ptr<thread_pool_improved::ThreadPool> thread_pool) {
    auto last_tasks_completed = task_counter_.load();
    auto last_time = std::chrono::high_resolution_clock::now();

    while (monitoring_active_.load()) {
        std::this_thread::sleep_for(std::chrono::milliseconds(config_.monitoring_interval_ms));

        auto current_time = std::chrono::high_resolution_clock::now();
        auto current_tasks_completed = task_counter_.load();

        auto time_diff = std::chrono::duration<double>(current_time - last_time).count();
        auto tasks_diff = current_tasks_completed - last_tasks_completed;
        
        // 吞吐量 每秒执行多少任务
        double current_throughput = tasks_diff / time_diff;

        // 获取线程池信息
        auto stats = thread_pool->GetStats();
        size_t queue_size = thread_pool->QueueSize();
        size_t active_threads = thread_pool->ActiveThreads();

        // 计算队列使用率
        double queue_usage_rate = 0.0;
        if (stats.max_queue_size > 0) {
            queue_usage_rate = static_cast<double>(queue_size) / stats.max_queue_size * 100.0;
        }

        if (config_.enable_console_output) {
            std::cout << std::fixed << std::setprecision(0)
                      << "[监控] 当前吞吐量: " << current_throughput << " 任务/秒, "
                      << "线程数: " << active_threads << ", "
                      << "队列大小: " << queue_size;
            
            // 添加队列使用率显示
            if (stats.max_queue_size > 0) {
                std::cout << std::fixed << std::setprecision(1)
                          << " (" << queue_usage_rate << "%)";
            }
            
            std::cout << ", 已完成: " << current_tasks_completed << std::endl;
        }

        if (config_.enable_logging) {
            LOG_INFO("实时监控 - 吞吐量: {:.0f} 任务/秒, 活跃线程: {}, 队列: {}, 完成: {}", 
                    current_throughput, active_threads, queue_size, current_tasks_completed);
        }
        
        // 更新last
        last_tasks_completed = current_tasks_completed;
        last_time = current_time;
    }
}

}