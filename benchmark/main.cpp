/*
压测入口
*/ 

#include "thread_pool_benchmark.h"

int main(int argc, char* argv[]) {
    // 从配置文件加载配置
    std::string config_path = "../../config/thread_pool.json";
    if (argc > 1 && std::string(argv[1]) == "--config") {
        if (argc > 2) {
            config_path = argv[2];
        }
    }

    benchmark::BenchmarkConfig config = benchmark::BenchmarkConfig::LoadFromFile(config_path);

    // 解析命令行参数（可覆盖配置文件）
    int arg_idx = 1;
    if (argc > arg_idx && std::string(argv[arg_idx]) == "--config") {
        arg_idx += 2; // 跳过 --config 和配置文件路径
    }
    
    if (argc > arg_idx) {
        config.core_threads = std::stoul(argv[arg_idx]);
        arg_idx++;
    }
    if (argc > arg_idx) {
        config.duration_seconds = std::stoul(argv[arg_idx]);
        arg_idx++;
    }
    if (argc > arg_idx) {
        std::string mode = argv[arg_idx];
        if (mode == "tasks") {
            config.use_duration_mode = false;
            arg_idx++;
            if (argc > arg_idx) {
                config.total_tasks = std::stoul(argv[arg_idx]);
            }
        }
    }

    if (config.enable_console_output) {
        std::cout << "=== 线程池吞吐量压测工具 ===" << std::endl;
        std::cout << "使用方法: " << argv[0] << " [--config 配置文件路径] [核心线程数] [测试时间秒] [模式(time|tasks)] [任务数(仅tasks模式)]" << std::endl;
        std::cout << "当前配置:" << std::endl;
        std::cout << "  核心线程数: " << config.core_threads << std::endl;
        std::cout << "  最大线程数: " << config.max_threads << std::endl;
        std::cout << "  队列大小: " << config.max_queue_size << std::endl;
        std::cout << "  日志记录: " << (config.enable_logging ? "启用" : "禁用") << std::endl;
        std::cout << "  控制台输出: " << (config.enable_console_output ? "启用" : "禁用") << std::endl;
        std::cout << "  实时监控: " << (config.enable_real_time_monitoring ? "启用" : "禁用") << std::endl;
    }
    
    if (config.enable_console_output) {
        if (config.use_duration_mode) {
            std::cout << "  测试模式: 按时间测试" << std::endl;
            std::cout << "  测试时间: " << config.duration_seconds << " 秒" << std::endl;
            std::cout << "  预热时间: " << config.warmup_seconds << " 秒" << std::endl;
        } else {
            std::cout << "  测试模式: 按任务数测试" << std::endl;
            std::cout << "  任务总数: " << config.total_tasks << std::endl;
        }
        
        std::cout << std::endl;
    }

    // 执行压测
    try {
        // 创建压测对象
        benchmark::ThreadPoolBenchmark benchmark_test(config);

        // 运行压测
        auto result = benchmark_test.RunBenchmark();

        // 打印结果
        benchmark_test.PrintResult(result);



    } catch (const std::exception& e) {
        std::cerr << "压测过程中发生错误: " << e.what() << std::endl;
        return 1;
    } catch (...) {
        std::cerr << "压测过程中发生错误: 未知原因" << std::endl;
        return 1;
    }

    return 0;
}