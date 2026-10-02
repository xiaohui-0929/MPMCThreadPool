/*
ThreadPool测试用例
使用Google Test框架
*/

#include "thread_pool_improved.h"

#include <gtest/gtest.h>

// 声明
using thread_pool_improved::ThreadPool;
using thread_pool_improved::QueueFullPolicy;

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

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}