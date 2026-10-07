/*
ThreadPool测试用例
使用Google Test框架
*/

#include "thread_pool_improved.h"

#include <gtest/gtest.h>

// 声明
using thread_pool_improved::ThreadPool;
using thread_pool_improved::QueueFullPolicy;
using thread_pool_improved::ThreadPoolStruct;
using thread_pool_improved::ThreadPoolState;
using thread_pool_improved::ShutdownOption;

// ==================基本功能测试======================

class ThreadPoolTest : public ::testing::Test {
protected:
    void SetUp() override {};
    void TearDown() override {};
};

// 测试基本功能
TEST_F(ThreadPoolTest, BasicFunctionality) {
    ThreadPool pool(2);

    std::atomic<size_t> counter{0};         // 共享计数
    std::vector<std::future<void>> futures; // 返回

    // 提交10个任务
    for (size_t i = 0; i < 10; ++i) {
        futures.push_back(
            pool.SubmitWithResult([&counter]() {
                std::this_thread::sleep_for(std::chrono::milliseconds(10));
                counter++;
            })
        );
    }

    // 等待任务完成
    for (auto& future : futures) {
        future.wait();
    }
    // 每个任务都应该对counter原子+1
    EXPECT_EQ(counter.load(), 10);
}

// 测试带返回值的任务
TEST_F(ThreadPoolTest, TaskWithReturnValues) {
    ThreadPool pool(2);
    // 测试不同返回值类型
    auto int_future = pool.SubmitWithResult([]() -> int {
        return 1998;
    });

    auto string_future = pool.SubmitWithResult([]() -> std::string {
        return "Hello xiaohui";
    });

    auto complex_future = pool.SubmitWithResult([](int a, int b) -> int {
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
        return a + b;
    }, 18, 10);

    EXPECT_EQ(int_future.get(), 1998);
    EXPECT_EQ(string_future.get(), "Hello xiaohui");
    EXPECT_EQ(complex_future.get(), 28);
}

// 测试并发执行
TEST_F(ThreadPoolTest, ConcurrentExecution) {
    const int num_threads = 4;
    const int num_tasks = 100;

    ThreadPool pool(num_threads);

    std::atomic<int> active_threads{0};     // 当前活动线程个数
    std::atomic<int> max_active_threads{0}; // 最大线程个数
    std::atomic<int> completed_tasks{0};

    std::vector<std::future<void>> futures;

    // 提交任务
    for (int i = 0; i < num_tasks; ++i) {
        futures.push_back(
            pool.SubmitWithResult([&active_threads, &max_active_threads, &completed_tasks]() {
                int current = active_threads.fetch_add(1) + 1;  
                int max = max_active_threads.load();            
                while (max < current && !max_active_threads.compare_exchange_strong(max, current)) {
                    // 自旋直到更新成功
                    // 若 最大线程数 小于 当前活动线程数
                    // 则将 最大线程数 更新为 当前活动线程数
                }
                std::this_thread::sleep_for(std::chrono::milliseconds(5));  // 模拟任务执行
                active_threads.fetch_sub(1);    // 执行结束后活动线程数减1
                completed_tasks.fetch_add(1);
            })
        );
    }

    // 等待任务完成
    for (auto& future : futures) {
        future.wait();
    }

    EXPECT_EQ(active_threads.load(), 0);
    EXPECT_EQ(max_active_threads.load(), num_threads);
    EXPECT_EQ(completed_tasks.load(), num_tasks);
}

// 测试异常处理
TEST_F(ThreadPoolTest, ExceptionHandling) {
    ThreadPool pool(2);
    // 提交一个会抛异常的任务
    auto future_1 = pool.SubmitWithResult([]() -> int {
        throw std::runtime_error("测试异常");
        return 0;
    });
    // 提交一个正常的任务
    auto future_2 = pool.SubmitWithResult([]() -> int {
        return 28;
    });
    EXPECT_THROW(future_1.get(), std::runtime_error);
    EXPECT_EQ(future_2.get(), 28);
}

// 测试线程池停止
TEST_F(ThreadPoolTest, ThreadPoolStop) {
    ThreadPool pool(2);
    std::atomic<int> completed_tasks{0};
    std::vector<std::future<void>> futures;

    for (int i = 0; i < 5; ++i) {
        pool.SubmitWithResult([&completed_tasks]() {
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
            completed_tasks++;
        });
    }

    pool.Stop();

    bool all_completed = true;
    for (auto& future : futures) {
        auto status = future.wait_for(std::chrono::milliseconds(1000));
        if (status == std::future_status::timeout) {
            all_completed = false;
            std::cout << "任务超时未完成" << std::endl;
        }
    }

    if (all_completed) {
        EXPECT_EQ(completed_tasks.load(), 5);
    } else {
        std::cout << "完成的任务数: " << completed_tasks.load()  << "/5" << std::endl;
        EXPECT_GT(completed_tasks.load(), 0);
    }

    EXPECT_TRUE(pool.IsStopped());

    // 停止后再提交 应该失败
    EXPECT_THROW(
        pool.SubmitWithResult([]() { return 28; }),
        std::runtime_error
    );
}

// 测试WaitAll
TEST_F(ThreadPoolTest, WaitAllFunctionality) {
    ThreadPool pool(2);

    for (int i = 0; i < 10; ++i) {
        pool.SubmitWithResult([]() {
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
        });
    }

    // 通过WaitAll等待任务执行完毕
    auto start = std::chrono::steady_clock::now();
    pool.WaitAll();
    auto duration = std::chrono::steady_clock::now() - start;

    EXPECT_LT(duration, std::chrono::milliseconds(5000));
}

// 测试性能基准
TEST_F(ThreadPoolTest, PerformanceBenchmark) {
    const int num_threads = 4;
    const int num_tasks = 1000;
    ThreadPool pool(num_threads);

    auto start = std::chrono::high_resolution_clock::now();

    std::vector<std::future<int>> futures;
    for (int i = 0; i < num_tasks; ++i) {
        futures.push_back(
            pool.SubmitWithResult([i]() -> int {
                int result = 0;
                for (int j = 0; j < 1000; ++j) {
                    result += i * j;
                }
                return result;
            })
        );
    }

    // 等待任务完成
    int total = 0;
    for (auto& future : futures) {
        total += future.get();
    }

    auto end = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);

    EXPECT_EQ(futures.size(), num_tasks);
    EXPECT_GT(total, 0);

    std::cout << "执行 " << num_tasks << " 个任务耗时: " << duration.count() << "ms" << std::endl;
    std::cout << "total: " << total << std::endl;
}

// 测试内存管理
TEST_F(ThreadPoolTest, MemoryManagement) {
    std::shared_ptr<int> shared_counter = std::make_shared<int>(0);

    {
        ThreadPool pool(2);

        for (int i = 0; i < 10; ++i) {
            pool.SubmitWithResult([shared_counter]() { 
                (*shared_counter)++;
                std::this_thread::sleep_for(std::chrono::milliseconds(10));
            });
        }

        // 等待一段时间让线程池执行
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
    }

    // 线程池销毁后 共享指针依然有效
    EXPECT_GT(*shared_counter, 0);
    std::cout << "shared_couter: " << *shared_counter << std::endl;
}

// 测试边界情况
TEST_F(ThreadPoolTest, EdgeCases) {
    EXPECT_THROW(ThreadPool(0), std::invalid_argument);

    // 测试单线程池
    ThreadPool pool(1);
    std::atomic<int> counter{0};
    
    auto future = pool.SubmitWithResult([&counter]() -> int {
        counter.fetch_add(1);
        return 42;
    });
    
    EXPECT_EQ(future.get(), 42);
    EXPECT_EQ(counter.load(), 1);
}

// 测试队列大小查询
TEST_F(ThreadPoolTest, QueueSizeQuery) {
    ThreadPool pool(1, 10);

    EXPECT_EQ(pool.QueueSize(), 0);

    std::vector<std::future<void>> futures;
    for (int i = 0; i < 10; ++i) {
        futures.push_back(
            pool.SubmitWithResult([]() {
                std::this_thread::sleep_for(std::chrono::milliseconds(10));
            })
        );
    }

    // 短暂等待 保证部分任务进入队列
    std::this_thread::sleep_for(std::chrono::milliseconds(10));

    EXPECT_GT(pool.QueueSize(), 0);
    std::cout << "queue size: " << pool.QueueSize() << std::endl;

    for (auto& future : futures) {
        future.wait();
    }

    EXPECT_EQ(pool.QueueSize(), 0);
}

// 测试大量任务
TEST_F(ThreadPoolTest, LargeNumberOfTasks) {
    const int num_tasks = 10000;
    ThreadPool pool(4);

    std::atomic<int> completed{0};
    std::vector<std::future<void>> futures;

    for (int i = 0; i < num_tasks; ++i) {
        futures.push_back(
            pool.SubmitWithResult([&completed]() {
                completed++;
            })
        );
    }

    // 等待所有任务完成
    for (auto& future : futures) {
        future.wait();
    }

    EXPECT_EQ(completed.load(), num_tasks);
}


// ==================队列策略测试======================
class QueuePolicyTest : public ::testing::Test {
protected:
    void SetUp() override {};
    void TearDown() override {};
};

// 测试阻塞策略 - 队列满时应阻塞

TEST_F(QueuePolicyTest, BlockPolicyTest) {
    ThreadPool pool(1, 2, QueueFullPolicy::BLOCK);

    std::atomic<int> completed_tasks{0};
    std::atomic<bool> task_started{false};

    // 任务1 - 设置为长时间执行 作用是长时间占用唯一线程 造成阻塞
    auto future1 = pool.SubmitWithResult([&completed_tasks, &task_started]() -> int {
        task_started = true;
        std::this_thread::sleep_for(std::chrono::milliseconds(500));
        completed_tasks++;
        return 1;
    });

    // 等待任务1开始执行
    if (!task_started.load()) {
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }
    EXPECT_EQ(pool.QueueSize(), 0); // 任务1提交后便开始执行 队列数量应该为0

    // 任务2 - 设置为长时间执行
    auto future2 = pool.SubmitWithResult([&completed_tasks, &task_started]() -> int {
        completed_tasks++;
        return 2;
    });
    // 队列数量为1
    EXPECT_EQ(pool.QueueSize(), 1); // 任务2提交后 唯一线程被占用 队列数量应该为1

    // 任务3
    auto start_time_future3 = std::chrono::steady_clock::now();
    auto future3 = pool.SubmitWithResult([&completed_tasks, &task_started]() -> int {
        completed_tasks++;
        return 3;
    });
    auto elapsed_future3 = std::chrono::steady_clock::now() - start_time_future3;
    // 任务3提交用时应该极小
    EXPECT_TRUE(elapsed_future3 < std::chrono::milliseconds(10));
    // 队列数量为2
    EXPECT_EQ(pool.QueueSize(), 2); // 任务3提交后 唯一线程被占用 队列数量应该为2

    // 任务4
    auto start_time = std::chrono::steady_clock::now();
    auto future4 = pool.SubmitWithResult([&completed_tasks, &task_started]() -> int {
        completed_tasks++;
        return 4;
    });

    // 检查从任务4 发起提交 到 结束提交 的用时
    auto elapsed = std::chrono::steady_clock::now() - start_time;
    // 验证第四个任务确实被阻塞了（提交用时大于非阻塞任务用时）
    EXPECT_FALSE(elapsed < std::chrono::milliseconds(10));
    EXPECT_TRUE(elapsed > std::chrono::milliseconds(100));

    // 等待所有任务完成
    future1.wait();
    future2.wait();
    future3.wait();
    future4.wait();
    
    EXPECT_EQ(completed_tasks.load(), 4);
    EXPECT_EQ(future1.get(), 1);
    EXPECT_EQ(future2.get(), 2);
    EXPECT_EQ(future3.get(), 3);
    EXPECT_EQ(future4.get(), 4);
}

// 测试覆盖策略 - 队列满时应该覆盖最旧的任务
TEST_F(QueuePolicyTest, OverwritePolicyTest) {
    // 创建一个只有1个线程和队列大小为1的线程池
    ThreadPool pool(1, 1, QueueFullPolicy::OVERWRITE);
    
    std::atomic<int> completed_tasks{0};
    std::atomic<bool> task_started{false};
    
    // 提交一个长时间运行的任务
    auto future1 = pool.SubmitWithResult([&completed_tasks, &task_started]() {
        task_started = true;
        std::this_thread::sleep_for(std::chrono::milliseconds(500));
        completed_tasks++;
        return 1;
    });
    
    // 等待任务开始执行
    while (!task_started.load()) {
        std::this_thread::sleep_for(std::chrono::microseconds(10));
    }
    
    // 提交第二个任务
    auto future2 = pool.SubmitWithResult([&completed_tasks]() {
        completed_tasks++;
        return 2;
    });

    // 提交第三个任务（应该覆盖第二个任务）
    auto future3 = pool.SubmitWithResult([&completed_tasks]() {
        completed_tasks++;
        return 3;
    });
    
    // 等待所有任务完成
    future1.wait();
    future2.wait();
    future3.wait();
    
    // 由于覆盖策略，第二个任务被丢弃，所以完成的任务数只为2
    EXPECT_EQ(completed_tasks.load(), 2);
    EXPECT_THROW(future2.get(), std::future_error); // future2被覆盖 会抛出异常
    EXPECT_EQ(future3.get(), 3);
}

// 测试丢弃策略 - 队列满时应该丢弃新任务
TEST_F(QueuePolicyTest, DiscardPolicyTest) {
    // 创建一个只有1个线程和队列大小为1的线程池
    ThreadPool pool(1, 1, QueueFullPolicy::DISCARD);
    
    std::atomic<int> completed_tasks{0};
    std::atomic<bool> task1_started{false};
    
    // 提交第一个任务（长时间运行）
    auto future1 = pool.SubmitWithResult([&completed_tasks, &task1_started]() {
        task1_started = true;
        std::this_thread::sleep_for(std::chrono::milliseconds(500));
        completed_tasks++;
        return 1;
    });
    
    // 等待第一个任务开始执行
    while (!task1_started.load()) {
        std::this_thread::sleep_for(std::chrono::microseconds(10));
    }
    
    // 提交第二个任务（应该进入队列等待）
    auto future2 = pool.SubmitWithResult([&completed_tasks]() {
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
        completed_tasks++;
        return 2;
    });
    
    // 提交第三个任务（应该被丢弃，因为队列已满）
    auto future3 = pool.SubmitWithResult([&completed_tasks]() {
        completed_tasks++;
        return 3;
    });
    
    // 等待前两个任务完成
    future1.wait();
    future2.wait();
    future3.wait();
    
    // 验证只有前两个任务被执行
    EXPECT_EQ(completed_tasks.load(), 2);
    EXPECT_EQ(future1.get(), 1);
    EXPECT_EQ(future2.get(), 2);
    
    // 第三个任务应该抛出异常，因为它被丢弃了
    EXPECT_THROW(future3.get(), std::future_error);
}

// ==================动态线程管理测试======================
class DynamicThreadPoolTest : public ::testing::Test {
protected:
    void SetUp() override {};
    void TearDown() override {};

    // 创建动态线程管理配置
    ThreadPoolStruct CreateDynamicConfig() {
        ThreadPoolStruct config;
        config.core_threads = 2;
        config.max_threads = 6;
        config.max_queue_size = 10;
        config.enable_dynamic_threads = true;
        config.thread_creation_threshold = 3;
        config.thread_idle_timeout = std::chrono::milliseconds(500);
        config.load_check_interval = std::chrono::milliseconds(100);
        config.scale_up_threshold = 0.8;
        config.scale_down_threshold = 0.3;
        config.min_idle_time_for_removal = std::chrono::milliseconds(300);
        config.max_consecutive_idle_checks = 2;
        config.queue_full_policy = QueueFullPolicy::BLOCK;
        return config;
    }
    // 创建静态线程池配置（用于对比）
    ThreadPoolStruct CreateStaticConfig() {
        ThreadPoolStruct config;
        config.core_threads = 2;
        config.max_threads = 6;
        config.max_queue_size = 10;
        config.enable_dynamic_threads = false;
        config.queue_full_policy = QueueFullPolicy::BLOCK;
        return config;
    }
};

// 测试线程动态创建
TEST_F(DynamicThreadPoolTest, DynamicThreadCreation) {
    auto config = CreateDynamicConfig();
    ThreadPool pool(config);

    // 初始应该只有核心线程
    EXPECT_EQ(pool.GetCurrentThreadCount(), config.core_threads);
    EXPECT_EQ(pool.GetCoreThreadCount(), config.core_threads);
    EXPECT_EQ(pool.GetMaxThreadCount(), config.max_threads);

    std::atomic<int> active_tasks{0};
    std::atomic<int> completed_tasks{0};
    std::vector<std::future<void>> futures;

    // 提交大量任务，触发线程创建
    for (int i = 0; i < 10; ++i) {
        futures.push_back(
            pool.SubmitWithResult([&active_tasks, &completed_tasks]() {
                active_tasks.fetch_add(1);
                std::this_thread::sleep_for(std::chrono::milliseconds(200));
                active_tasks.fetch_sub(1);
                completed_tasks.fetch_add(1);
            })
        );
    }

    // 等待一段时间让负载均衡器有机会创建新线程
    std::this_thread::sleep_for(std::chrono::milliseconds(300));

    // 检查当前是否有额外的线程
    EXPECT_GT(pool.GetCurrentThreadCount(), config.core_threads);
    EXPECT_LE(pool.GetCurrentThreadCount(), config.max_threads);

    // 等待所有任务完成
    for (auto& future : futures) {
        future.wait();
    }

    EXPECT_EQ(completed_tasks.load(), 10);

    // 获取统计信息
    auto stats = pool.GetStats();
    EXPECT_GT(stats.threads_created, config.core_threads);
    EXPECT_GT(stats.peak_threads, config.core_threads);
    EXPECT_EQ(stats.tasks_completed, 10);
    EXPECT_EQ(stats.tasks_failed, 0);
}

// 测试线程空闲回收
TEST_F(DynamicThreadPoolTest, ThreadIdleTimeoutRecycling) {
    auto config = CreateDynamicConfig();
    // 设置更快的回收参数以便测试
    config.thread_idle_timeout = std::chrono::milliseconds(200);
    config.min_idle_time_for_removal = std::chrono::milliseconds(150);
    config.max_consecutive_idle_checks = 2;
    config.load_check_interval = std::chrono::milliseconds(50);

    ThreadPool pool(config);

    std::atomic<int> completed_tasks{0};
    std::vector<std::future<void>> futures;

    // 第一阶段：提交大量任务创建额外线程
    for (int i = 0; i < 12; ++i) {
        futures.push_back(
            pool.SubmitWithResult([&completed_tasks, i]() {
                std::this_thread::sleep_for(std::chrono::milliseconds(100));
                completed_tasks.fetch_add(1);
            })
        );
    }

    // 等待一些时间让线程池创建额外线程
    std::this_thread::sleep_for(std::chrono::milliseconds(200));

    // 查看峰值线程数
    size_t peak_threads = pool.GetCurrentThreadCount();
    EXPECT_GT(peak_threads, config.core_threads);
    EXPECT_LE(peak_threads, config.max_threads);

    // 等待所有任务完成
    for (auto& future : futures) {
        future.wait();
    }

    EXPECT_EQ(completed_tasks.load(), 12);

    // 第二阶段：等待线程回收
    std::this_thread::sleep_for(std::chrono::milliseconds(800)); // 等待足够长时间
    
    size_t after_recycle_threads = pool.GetCurrentThreadCount();
    // 验证线程确实被回收了
    EXPECT_LT(after_recycle_threads, peak_threads);
    EXPECT_GE(after_recycle_threads, config.core_threads);

    // 验证统计信息
    auto stats = pool.GetStats();
    EXPECT_GT(stats.threads_created, config.core_threads);
    EXPECT_GT(stats.threads_destroyed, 0); // 应该有线程被销毁
    EXPECT_EQ(stats.peak_threads, peak_threads);
}

// 测试线程回收的统计信息准确性
TEST_F(DynamicThreadPoolTest, ThreadRecyclingStatistics) {
    auto config = CreateDynamicConfig();
    config.thread_idle_timeout = std::chrono::milliseconds(150);
    config.min_idle_time_for_removal = std::chrono::milliseconds(100);
    config.max_consecutive_idle_checks = 2;
    config.load_check_interval = std::chrono::milliseconds(50);
    
    ThreadPool pool(config);
    
    // 获取初始统计信息
    auto initial_stats = pool.GetStats();
    EXPECT_EQ(initial_stats.threads_created, config.core_threads);
    EXPECT_EQ(initial_stats.threads_destroyed, 0);
    EXPECT_EQ(initial_stats.peak_threads, config.core_threads);
    
    std::vector<std::future<void>> futures;
    
    // 创建负载，触发线程创建
    for (int i = 0; i < 15; ++i) {
        futures.push_back(
            pool.SubmitWithResult([]() {
                std::this_thread::sleep_for(std::chrono::milliseconds(80));
            })
        );
    }
    
    // 等待线程创建
    std::this_thread::sleep_for(std::chrono::milliseconds(200));
    
    auto peak_stats = pool.GetStats();
    size_t created_threads = peak_stats.threads_created;
    size_t peak_count = peak_stats.peak_threads;
    
    EXPECT_GT(created_threads, config.core_threads);
    EXPECT_GT(peak_count, config.core_threads);
    
    // 等待所有任务完成
    for (auto& future : futures) {
        future.wait();
    }
    
    // 等待线程回收
    std::this_thread::sleep_for(std::chrono::milliseconds(600));
    
    auto final_stats = pool.GetStats();
    
    // 验证统计信息
    EXPECT_EQ(final_stats.threads_created, created_threads); // 创建数应该保持不变
    EXPECT_GT(final_stats.threads_destroyed, 0); // 应该有线程被销毁
    EXPECT_EQ(final_stats.peak_threads, peak_count); // 峰值应该保持不变
    EXPECT_LE(pool.GetCurrentThreadCount(), peak_count); // 当前线程数应该小于等于峰值
    EXPECT_GE(pool.GetCurrentThreadCount(), config.core_threads); // 不应少于核心线程数
}

// 测试线程回收的边界条件
TEST_F(DynamicThreadPoolTest, ThreadRecyclingBoundaryConditions) {
    auto config = CreateDynamicConfig();
    config.core_threads = 2;
    config.max_threads = 4;
    config.thread_idle_timeout = std::chrono::milliseconds(100);
    config.min_idle_time_for_removal = std::chrono::milliseconds(80);
    config.max_consecutive_idle_checks = 1;
    config.load_check_interval = std::chrono::milliseconds(30);
    
    ThreadPool pool(config);

    // 测试1：核心线程不会被回收
    std::vector<std::future<void>> futures;
    
    // 只提交少量任务，不触发额外线程创建
    for (int i = 0; i < 2; ++i) {
        futures.push_back(
            pool.SubmitWithResult([]() {
                std::this_thread::sleep_for(std::chrono::milliseconds(50));
            })
        );
    }
    
    for (auto& future : futures) {
        future.wait();
    }
    
    // 等待很长时间
    std::this_thread::sleep_for(std::chrono::milliseconds(500));
    
    // 核心线程数应该保持不变
    EXPECT_EQ(pool.GetCurrentThreadCount(), config.core_threads);

    // 测试2：非核心线程会被回收
    // 提交大量任务创建额外线程
    for (int i = 0; i < 8; ++i) {
        futures.push_back(
            pool.SubmitWithResult([]() {
                std::this_thread::sleep_for(std::chrono::milliseconds(60));
            })
        );
    }
    
    // 等待线程创建
    std::this_thread::sleep_for(std::chrono::milliseconds(150));
    
    size_t peak = pool.GetCurrentThreadCount();
    EXPECT_GT(peak, config.core_threads);
    
    // 等待任务完成
    for (auto& future : futures) {
        future.wait();
    }
    
    // 等待线程回收
    std::this_thread::sleep_for(std::chrono::milliseconds(400));
    
    size_t after_recycle = pool.GetCurrentThreadCount();
    
    // 应该回收到核心线程数
    EXPECT_LE(after_recycle, peak);
    EXPECT_GE(after_recycle, config.core_threads);
}

// 测试多轮线程创建和回收
TEST_F(DynamicThreadPoolTest, MultipleCreateRecycleCycles) {
    auto config = CreateDynamicConfig();
    config.thread_idle_timeout = std::chrono::milliseconds(150);
    config.min_idle_time_for_removal = std::chrono::milliseconds(100);
    config.max_consecutive_idle_checks = 2;
    config.load_check_interval = std::chrono::milliseconds(40);
    
    ThreadPool pool(config);
    
    std::vector<size_t> peak_threads;
    std::vector<size_t> recycle_threads;
    
    // 进行3轮创建和回收测试
    for (int cycle = 0; cycle < 3; ++cycle) {
        
        std::vector<std::future<void>> futures;
        
        // 创建负载
        for (int i = 0; i < 10; ++i) {
            futures.push_back(
                pool.SubmitWithResult([cycle, i]() {
                    std::this_thread::sleep_for(std::chrono::milliseconds(80));
                })
            );
        }
        
        // 等待线程创建
        std::this_thread::sleep_for(std::chrono::milliseconds(150));
        
        size_t peak = pool.GetCurrentThreadCount();
        peak_threads.push_back(peak);
        
        // 等待任务完成
        for (auto& future : futures) {
            future.wait();
        }
        
        // 等待线程回收
        std::this_thread::sleep_for(std::chrono::milliseconds(400));
        
        size_t after_recycle = pool.GetCurrentThreadCount();
        recycle_threads.push_back(after_recycle);
        
        // 验证每轮都能正确回收
        EXPECT_GT(peak, config.core_threads);
        EXPECT_LE(after_recycle, peak);
        EXPECT_GE(after_recycle, config.core_threads);
    }
    
    // 验证多轮测试的一致性
    for (size_t i = 1; i < peak_threads.size(); ++i) {
        // 每轮的峰值应该相似（允许一定差异）
        EXPECT_LE(std::abs(static_cast<int>(peak_threads[i]) - static_cast<int>(peak_threads[0])), 2);
    }
    
    for (size_t i = 0; i < recycle_threads.size(); ++i) {
        // 每轮回收后都应该接近核心线程数
        EXPECT_LE(recycle_threads[i], config.core_threads + 1);
    }

    auto final_stats = pool.GetStats();
    
    EXPECT_GT(final_stats.threads_destroyed, 0);
}

// 测试负载感知的线程调整
TEST_F(DynamicThreadPoolTest, LoadAwareThreadAdjustment) {
    auto config = CreateDynamicConfig();
    ThreadPool pool(config);
    
    // 测试低负载情况 - 应该保持核心线程数
    std::atomic<int> completed_tasks{0};
    
    auto future1 = pool.SubmitWithResult([&completed_tasks]() {
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
        completed_tasks.fetch_add(1);
    });
    
    future1.wait();
    std::this_thread::sleep_for(std::chrono::milliseconds(200));
    
    size_t low_load_threads = pool.GetCurrentThreadCount();
    EXPECT_EQ(low_load_threads, config.core_threads);
    
    // 测试高负载情况 - 应该创建更多线程
    std::vector<std::future<void>> high_load_futures;
    for (int i = 0; i < 12; ++i) {
        high_load_futures.push_back(
            pool.SubmitWithResult([&completed_tasks]() {
                std::this_thread::sleep_for(std::chrono::milliseconds(300));
                completed_tasks.fetch_add(1);
            })
        );
    }
    
    // 等待负载均衡器反应
    std::this_thread::sleep_for(std::chrono::milliseconds(400));
    
    size_t high_load_threads = pool.GetCurrentThreadCount();
    EXPECT_GT(high_load_threads, low_load_threads);
    EXPECT_LE(high_load_threads, config.max_threads);
    
    // 等待所有任务完成
    for (auto& future : high_load_futures) {
        future.wait();
    }
    
    // 验证负载因子计算
    auto stats = pool.GetStats();
    EXPECT_GE(stats.load_factor, 0.0);
    EXPECT_LE(stats.load_factor, 1.0);
    
    std::cout << "负载测试 - 低负载线程数: " << low_load_threads 
              << ", 高负载线程数: " << high_load_threads 
              << ", 负载因子: " << stats.load_factor << std::endl;
}

// 测试手动触发负载检查
TEST_F(DynamicThreadPoolTest, ManualLoadCheck) {
    auto config = CreateDynamicConfig();
    ThreadPool pool(config);
    
    size_t initial_threads = pool.GetCurrentThreadCount();
    
    // 提交一些任务但不等待完成
    std::vector<std::future<void>> futures;
    for (int i = 0; i < 6; ++i) {
        futures.push_back(
            pool.SubmitWithResult([]() {
                std::this_thread::sleep_for(std::chrono::milliseconds(200));
            })
        );
    }
    
    // 手动触发负载检查
    pool.TriggerLoadCheck();
    
    // 短暂等待让手动检查生效
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    
    size_t after_check_threads = pool.GetCurrentThreadCount();
    
    // 等待任务完成
    for (auto& future : futures) {
        future.wait();
    }
    
    // 验证手动检查是否有效果
    EXPECT_GE(after_check_threads, initial_threads);
    
    std::cout << "手动负载检查 - 初始: " << initial_threads 
              << ", 检查后: " << after_check_threads << std::endl;
}

// 测试线程池统计信息
TEST_F(DynamicThreadPoolTest, StatisticsCollection) {
    auto config = CreateDynamicConfig();
    ThreadPool pool(config);
    
    // 初始统计信息
    auto initial_stats = pool.GetStats();
    EXPECT_EQ(initial_stats.tasks_completed, 0);
    EXPECT_EQ(initial_stats.tasks_failed, 0);
    EXPECT_EQ(initial_stats.threads_created, config.core_threads);
    EXPECT_EQ(initial_stats.threads_destroyed, 0);
    EXPECT_EQ(initial_stats.peak_threads, config.core_threads);
    
    std::atomic<int> task_counter{0};
    std::vector<std::future<void>> futures;
    
    // 提交成功任务
    for (int i = 0; i < 5; ++i) {
        futures.push_back(
            pool.SubmitWithResult([&task_counter]() {
                std::this_thread::sleep_for(std::chrono::milliseconds(50));
                task_counter.fetch_add(1);
            })
        );
    }
    
    // 提交失败任务
    for (int i = 0; i < 2; ++i) {
        futures.push_back(
            pool.SubmitWithResult([&task_counter]() {
                task_counter.fetch_add(1);
                throw std::runtime_error("测试异常");
            })
        );
    }
    
    // 等待所有任务完成
    for (size_t i = 0; i < futures.size(); ++i) {
        try {
            futures[i].wait();
        } catch (...) {
            // 忽略异常，只关心统计
        }
    }
    
    // 等待统计信息更新
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    
    auto final_stats = pool.GetStats();
    
    EXPECT_EQ(final_stats.tasks_completed, 5);  // 成功的任务
    EXPECT_EQ(final_stats.tasks_failed, 2);    // 失败的任务
    EXPECT_GT(final_stats.avg_task_time_ms, 0); // 平均执行时间
    EXPECT_GE(final_stats.peak_threads, config.core_threads);
    
    std::cout << "统计信息 - 成功: " << final_stats.tasks_completed 
              << ", 失败: " << final_stats.tasks_failed 
              << ", 平均时间: " << final_stats.avg_task_time_ms << "ms"
              << ", 峰值线程: " << final_stats.peak_threads << std::endl;
}

// 测试动态线程管理与静态线程池的性能对比
TEST_F(DynamicThreadPoolTest, DynamicVsStaticPerformance) {
    const int num_tasks = 50;
    const int task_duration_ms = 100;
    
    // 测试动态线程池
    auto dynamic_config = CreateDynamicConfig();
    auto dynamic_start = std::chrono::high_resolution_clock::now();
    
    {
        ThreadPool dynamic_pool(dynamic_config);
        std::vector<std::future<void>> dynamic_futures;
        
        for (int i = 0; i < num_tasks; ++i) {
            dynamic_futures.push_back(
                dynamic_pool.SubmitWithResult([task_duration_ms]() {
                    std::this_thread::sleep_for(std::chrono::milliseconds(task_duration_ms));
                })
            );
        }
        
        for (auto& future : dynamic_futures) {
            future.wait();
        }
    }
    
    auto dynamic_end = std::chrono::high_resolution_clock::now();
    auto dynamic_duration = std::chrono::duration_cast<std::chrono::milliseconds>(dynamic_end - dynamic_start);
    
    // 测试静态线程池
    auto static_config = CreateStaticConfig();
    auto static_start = std::chrono::high_resolution_clock::now();
    
    {
        ThreadPool static_pool(static_config);
        std::vector<std::future<void>> static_futures;
        
        for (int i = 0; i < num_tasks; ++i) {
            static_futures.push_back(
                static_pool.SubmitWithResult([task_duration_ms]() {
                    std::this_thread::sleep_for(std::chrono::milliseconds(task_duration_ms));
                })
            );
        }
        
        for (auto& future : static_futures) {
            future.wait();
        }
    }
    
    auto static_end = std::chrono::high_resolution_clock::now();
    auto static_duration = std::chrono::duration_cast<std::chrono::milliseconds>(static_end - static_start);
    
    std::cout << "性能对比 - 动态线程池: " << dynamic_duration.count() << "ms, "
              << "静态线程池: " << static_duration.count() << "ms" << std::endl;
    
    // 动态线程池在高并发情况下应该表现更好或至少不差太多
    double performance_ratio = static_cast<double>(dynamic_duration.count()) / static_duration.count();
    EXPECT_LT(performance_ratio, 1.5); // 动态线程池的时间不应该超过静态的1.5倍
}

// 测试极限情况下的动态线程管理
TEST_F(DynamicThreadPoolTest, ExtremeLoadConditions) {
    auto config = CreateDynamicConfig();
    config.max_threads = 10; // 增加最大线程数
    ThreadPool pool(config);
    
    const int num_tasks = 100;
    std::atomic<int> completed_tasks{0};
    std::vector<std::future<void>> futures;
    
    // 提交大量短任务
    for (int i = 0; i < num_tasks; ++i) {
        futures.push_back(
            pool.SubmitWithResult([&completed_tasks]() {
                // 模拟非常短的任务
                for (int j = 0; j < 1000; ++j) {
                    volatile int temp = j * j;
                    (void)temp;
                }
                completed_tasks.fetch_add(1);
            })
        );
    }
    
    // 等待所有任务完成
    for (auto& future : futures) {
        auto status = future.wait_for(std::chrono::milliseconds(1000));
        EXPECT_EQ(status, std::future_status::ready);
    }
    
    // 给统计信息一些时间更新
    std::this_thread::sleep_for(std::chrono::milliseconds(10));
    
    EXPECT_EQ(completed_tasks.load(), num_tasks);

    auto stats = pool.GetStats();
    EXPECT_EQ(stats.tasks_completed, num_tasks);
    EXPECT_EQ(stats.tasks_failed, 0);
    EXPECT_LE(stats.peak_threads, config.max_threads);
    
    std::cout << "极限测试 - 完成任务: " << stats.tasks_completed 
              << ", 峰值线程: " << stats.peak_threads 
              << ", 平均时间: " << stats.avg_task_time_ms << "ms" << std::endl;
}

// ==================状态控制测试======================
class StateControlTest : public ::testing::Test {
protected:
    void SetUp() override {};
    void TearDown() override {};
};

// 测试暂停和恢复功能
TEST_F(StateControlTest, PauseAndResume) {
    ThreadPool pool(2);
    
    // 初始状态应该是RUNNING
    EXPECT_EQ(pool.GetState(), ThreadPoolState::RUNNING);
    EXPECT_FALSE(pool.IsPaused());
    
    std::atomic<int> completed_tasks{0};
    std::vector<std::future<void>> futures;
    
    // 提交一些任务
    for (int i = 0; i < 5; ++i) {
        futures.push_back(
            pool.SubmitWithResult([&completed_tasks, i]() {
                std::this_thread::sleep_for(std::chrono::milliseconds(100));
                completed_tasks.fetch_add(1);
            })
        );
    }
    
    // 等待一些任务开始执行
    std::this_thread::sleep_for(std::chrono::milliseconds(50));
    
    // 暂停线程池
    pool.Pause();
    EXPECT_EQ(pool.GetState(), ThreadPoolState::PAUSED);
    EXPECT_TRUE(pool.IsPaused());
    
    // 尝试提交新任务应该失败
    EXPECT_THROW(
        pool.SubmitWithResult([]() { return 42; }),
        std::runtime_error
    );
    
    // 记录暂停时的完成任务数
    std::this_thread::sleep_for(std::chrono::milliseconds(200));
    int paused_completed = completed_tasks.load();
    // 暂停时的完成任务数应该小于提交任务数
    EXPECT_LT(paused_completed, 5);
    std::cout << paused_completed << std::endl;
    
    // 恢复线程池
    pool.Resume();
    EXPECT_EQ(pool.GetState(), ThreadPoolState::RUNNING);
    EXPECT_FALSE(pool.IsPaused());
    
    // 等待所有任务完成
    for (auto& future : futures) {
        future.wait();
    }
    
    EXPECT_EQ(completed_tasks.load(), 5);
}

// 测试优雅关闭
TEST_F(StateControlTest, GracefulShutdown) {
    ThreadPool pool(2);
    
    std::atomic<int> completed_tasks{0};
    std::vector<std::future<void>> futures;
    
    // 提交一些长时间运行的任务
    for (int i = 0; i < 6; ++i) {
        futures.push_back(
            pool.SubmitWithResult([&completed_tasks, i]() {
                std::this_thread::sleep_for(std::chrono::milliseconds(150));
                completed_tasks.fetch_add(1);
            })
        );
    }
    
    // 等待一些任务开始执行
    std::this_thread::sleep_for(std::chrono::milliseconds(50));
    
    // 优雅关闭
    auto start_time = std::chrono::steady_clock::now();
    pool.Shutdown(ShutdownOption::GRACEFUL);
    auto end_time = std::chrono::steady_clock::now();
    
    // 验证所有任务都完成了
    EXPECT_EQ(completed_tasks.load(), 6);
    EXPECT_EQ(pool.GetState(), ThreadPoolState::STOPPED);
    
    // 关闭后提交任务应该失败
    EXPECT_THROW(
        pool.SubmitWithResult([]() { return 42; }),
        std::runtime_error
    );
    
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time);
    std::cout << duration.count() << std::endl;
    
    // 优雅关闭应该等待所有任务完成，所以时间应该合理
    EXPECT_GE(duration.count(), 100); // 至少等待一些时间
}

// 测试超时关闭
TEST_F(StateControlTest, TimeoutShutdown) {
    ThreadPool pool(2);
    
    std::atomic<int> completed_tasks{0};
    std::vector<std::future<void>> futures;
    
    // 提交一些长时间运行的任务
    for (int i = 0; i < 4; ++i) {
        futures.push_back(
            pool.SubmitWithResult([&completed_tasks, i]() {
                std::this_thread::sleep_for(std::chrono::milliseconds(500));
                completed_tasks.fetch_add(1);
            })
        );
    }
    
    // 等待任务开始执行
    std::this_thread::sleep_for(std::chrono::milliseconds(500));
    
    // 超时关闭（300ms超时）
    auto start_time = std::chrono::steady_clock::now();
    pool.Shutdown(ShutdownOption::TIMEOUT, std::chrono::milliseconds(300));
    auto end_time = std::chrono::steady_clock::now();
    
    EXPECT_EQ(pool.GetState(), ThreadPoolState::STOPPED);
    
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time);
    std::cout << duration.count() << std::endl;
    
    // 超时关闭应该在超时时间左右完成
    EXPECT_GE(duration.count(), 250);
    EXPECT_LT(duration.count(), 550);
    
    // 由于超时，可能有一些任务没有完成
    std::cout << completed_tasks.load() << std::endl;
    EXPECT_LE(completed_tasks.load(), 4);
}

// 测试强制停止
TEST_F(StateControlTest, ForceShutdown) {
    ThreadPool pool(2);
    
    std::atomic<int> completed_tasks{0};
    std::vector<std::future<void>> futures;
    
    // 提交一些长时间运行的任务
    for (int i = 0; i < 6; ++i) {
        futures.push_back(
            pool.SubmitWithResult([&completed_tasks, i]() {
                std::this_thread::sleep_for(std::chrono::milliseconds(500));
                completed_tasks.fetch_add(1);
            })
        );
    }
    
    // 等待一些任务开始执行
    std::this_thread::sleep_for(std::chrono::milliseconds(500));
    
    // 强制停止
    auto start_time = std::chrono::steady_clock::now();
    pool.Shutdown(ShutdownOption::FORCE);
    auto end_time = std::chrono::steady_clock::now();
    
    EXPECT_EQ(pool.GetState(), ThreadPoolState::STOPPED);
    
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time);
    std::cout << duration.count() << std::endl;
    
    // 强制停止应该很快完成
    EXPECT_LT(duration.count(), 550);
    
    // 可能有一些任务没有完成
    std::cout << completed_tasks.load() << std::endl;
    EXPECT_LE(completed_tasks.load(), 6);
}

// 测试暂停状态下的关闭
TEST_F(StateControlTest, ShutdownFromPausedState) {
    ThreadPool pool(2);
    
    std::atomic<int> completed_tasks{0};
    std::vector<std::future<void>> futures;
    
    // 提交一些任务
    for (int i = 0; i < 4; ++i) {
        futures.push_back(
            pool.SubmitWithResult([&completed_tasks, i]() {
                std::this_thread::sleep_for(std::chrono::milliseconds(100));
                completed_tasks.fetch_add(1);
            })
        );
    }

    // 等待一些任务开始执行
    std::this_thread::sleep_for(std::chrono::milliseconds(50));

    // 暂停线程池
    pool.Pause();
    EXPECT_EQ(pool.GetState(), ThreadPoolState::PAUSED);

    // 从暂停状态优雅关闭
    pool.Shutdown(ShutdownOption::GRACEFUL);
    EXPECT_EQ(pool.GetState(), ThreadPoolState::STOPPED);

    // 验证暂停的正确行为：
    // - 只有正在执行的任务会完成（最多2个，因为有2个工作线程）
    // - 队列中等待的任务不会被执行
    // - 至少会有1个任务完成（因为50ms等待时间足够至少启动1个任务）
    int completed = completed_tasks.load();
    EXPECT_GE(completed, 1);  // 至少1个任务完成
    EXPECT_LE(completed, 2);  // 最多2个任务完成（正在执行的任务）
}

// 测试状态查询接口
TEST_F(StateControlTest, StateQuery) {
    ThreadPool pool(2);
    
    // 测试初始状态
    EXPECT_EQ(pool.GetState(), ThreadPoolState::RUNNING);
    EXPECT_FALSE(pool.IsPaused());
    EXPECT_FALSE(pool.IsStopped());
    
    // 测试暂停状态
    pool.Pause();
    EXPECT_EQ(pool.GetState(), ThreadPoolState::PAUSED);
    EXPECT_TRUE(pool.IsPaused());
    EXPECT_FALSE(pool.IsStopped());
    
    // 测试恢复状态
    pool.Resume();
    EXPECT_EQ(pool.GetState(), ThreadPoolState::RUNNING);
    EXPECT_FALSE(pool.IsPaused());
    EXPECT_FALSE(pool.IsStopped());
    
    // 测试停止状态
    pool.Stop();
    EXPECT_EQ(pool.GetState(), ThreadPoolState::STOPPED);
    EXPECT_FALSE(pool.IsPaused());
    EXPECT_TRUE(pool.IsStopped());
}

// 测试重复操作的幂等性
TEST_F(StateControlTest, IdempotentOperations) {
    ThreadPool pool(2);
    
    // 重复暂停
    pool.Pause();
    EXPECT_EQ(pool.GetState(), ThreadPoolState::PAUSED);
    pool.Pause(); // 再次暂停
    EXPECT_EQ(pool.GetState(), ThreadPoolState::PAUSED);
    
    // 重复恢复
    pool.Resume();
    EXPECT_EQ(pool.GetState(), ThreadPoolState::RUNNING);
    pool.Resume(); // 再次恢复
    EXPECT_EQ(pool.GetState(), ThreadPoolState::RUNNING);
    
    // 重复关闭
    pool.Shutdown(ShutdownOption::GRACEFUL);
    EXPECT_EQ(pool.GetState(), ThreadPoolState::STOPPED);
    pool.Shutdown(ShutdownOption::GRACEFUL); // 再次关闭
    EXPECT_EQ(pool.GetState(), ThreadPoolState::STOPPED);
}



int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}