/*
MPMCBlockingQueue测试用例
使用Google Test框架
*/

#include "mpmc_blocking_q.h"

#include <thread>

#include <gtest/gtest.h>

// 引入MPMCBlockingQueue
using thread_pool_improved::MPMCBlockingQueue;

// 测试环境的初始化和清理
class MPMCBlockingQueueTest : public ::testing::Test {
protected:
    void SetUp() override {};
    void TearDown() override {};
};

// 测试构造函数
TEST_F(MPMCBlockingQueueTest, Constructors) {
    // 带容量构造
    MPMCBlockingQueue<int> q(5);
    EXPECT_EQ(q.Size(), 0);
}

// 测试基本入队和出队（单线程）
TEST_F(MPMCBlockingQueueTest, BasicOperations) {
    MPMCBlockingQueue<int> q(3);

    // 测试入队
    q.Enqueue(10);
    EXPECT_EQ(q.Size(), 1);

    q.Enqueue(20);
    q.Enqueue(30);
    EXPECT_EQ(q.Size(), 3);

    // 测试出队（FIFO顺序）
    int item;
    q.Dequeue(item);
    EXPECT_EQ(item, 10);
    EXPECT_EQ(q.Size(), 2);

    q.Dequeue(item);
    EXPECT_EQ(item, 20);
    q.Dequeue(item);
    EXPECT_EQ(item, 30);
    EXPECT_EQ(q.Size(), 0);
}

// 测试非阻塞入队（EnqueueIfHaveRoom）
TEST_F(MPMCBlockingQueueTest, EnqueueIfHaveRoom) {
    MPMCBlockingQueue<int> q(2);

    // 队列未满时，应该成功入队并返回 true
    EXPECT_TRUE(q.EnqueueIfHaveRoom(1));
    EXPECT_TRUE(q.EnqueueIfHaveRoom(2));
    EXPECT_EQ(q.Size(), 2);
    EXPECT_EQ(q.DiscardCounter(), 0);

    // 队列已满时，应该失败返回 false，且丢弃计数增加
    EXPECT_FALSE(q.EnqueueIfHaveRoom(3));
    EXPECT_EQ(q.Size(), 2);               // 大小不变
    EXPECT_EQ(q.DiscardCounter(), 1);     // 丢弃计数增加
    int item;
    q.Dequeue(item);
    EXPECT_EQ(item, 1);            // 队列内容未被覆盖，仍是 1 和 2
    q.Dequeue(item);
    EXPECT_EQ(item, 2);
}

// 测试丢弃计数器的重置
TEST_F(MPMCBlockingQueueTest, DiscardCounterReset) {
    MPMCBlockingQueue<int> q(1);

    q.EnqueueIfHaveRoom(1);       // 成功
    q.EnqueueIfHaveRoom(2);       // 失败，丢弃计数 +1
    q.EnqueueIfHaveRoom(3);       // 失败，丢弃计数 +1
    EXPECT_EQ(q.DiscardCounter(), 2);

    q.ResetDiscardCounter();
    EXPECT_EQ(q.DiscardCounter(), 0);
}

// 测试阻塞出队（消费者等待生产者）
TEST_F(MPMCBlockingQueueTest, BlockingDequeue) {
    MPMCBlockingQueue<int> q(3);
    std::atomic<bool> consumed{false};
    int result = 0;

    // 消费者线程：队列为空，应该阻塞等待
    std::thread consumer([&]() {
        q.Dequeue(result);       // 会阻塞，直到有元素
        consumed.store(true);
    });

    // 主线程稍等片刻，确保消费者已经进入等待状态
    std::this_thread::sleep_for(std::chrono::milliseconds(50));
    EXPECT_FALSE(consumed.load());  // 消费者应该还在阻塞

    // 生产者入队，唤醒消费者
    q.Enqueue(42);

    consumer.join();
    EXPECT_TRUE(consumed.load());
    EXPECT_EQ(result, 42);
}

// 测试阻塞入队（生产者等待消费者）
TEST_F(MPMCBlockingQueueTest, BlockingEnqueue) {
    MPMCBlockingQueue<int> q(1);
    std::atomic<bool> produced{false};
    int result = 0;

    // 先填满队列
    q.Enqueue(1);

    // 生产者线程：队列已满，应该阻塞等待
    std::thread producer([&]() {
        q.Enqueue(3);       // 会阻塞，直到有空间
        produced.store(true);
    });

    // 主线程稍等片刻，确保生产者已经进入等待状态
    std::this_thread::sleep_for(std::chrono::milliseconds(50));
    EXPECT_FALSE(produced.load());  // 生产者应该还在阻塞

    // 消费者出队，腾出空间，唤醒生产者
    q.Dequeue(result);
    EXPECT_EQ(result, 1);

    producer.join();
    EXPECT_TRUE(produced.load());  // 生产者应该已经完成入队

    q.Dequeue(result);
    EXPECT_EQ(result, 3);           // 确认生产者入队的元素
}

// 测试多生产者-多消费者（MPMC 核心测试）
TEST_F(MPMCBlockingQueueTest, MultiProducerMultiConsumer) {
    const size_t num_producers = 4;
    const size_t num_consumers = 4;
    const size_t items_per_producer = 5000;
    const size_t total_items = num_producers * items_per_producer;

    MPMCBlockingQueue<int> q(128);  // 队列容量为128
    std::atomic<size_t> produced{0};        // 生产计数器
    std::atomic<size_t> consumed{0};        // 消费计数器
    std::atomic<size_t> sum_produced{0};    // 生产总和
    std::atomic<size_t> sum_consumed{0};    // 消费总和

    // 生产者线程 负责生产数据并入队
    std::vector<std::thread> producers;
    for (size_t i = 0; i < num_producers; ++i) {
        producers.emplace_back([&, i]() {
            for (size_t j = 0; j < items_per_producer; ++j) {
                int value = i * items_per_producer + j;
                q.Enqueue(std::move(value));
                sum_produced.fetch_add(value, std::memory_order_relaxed);   // 累加生产总和
                produced.fetch_add(1, std::memory_order_relaxed);           // 累加生产计数
            }
        });
    }

    // 消费者线程 负责从队列中出队数据并进行处理
    std::vector<std::thread> consumers;
    for (size_t i = 0; i < num_consumers; ++i) {
        consumers.emplace_back([&]() {
            while (consumed.load(std::memory_order_relaxed) < total_items) {
                int value;
                q.Dequeue(value);  // 阻塞出队
                if (value == -1) {
                    // 收到哨兵，说明生产者已全部结束，本消费者退出
                    break;
                }
                sum_consumed.fetch_add(value, std::memory_order_relaxed);   // 累加消费总和
                consumed.fetch_add(1, std::memory_order_relaxed);           // 累加消费计数
            }
        });
    }

    // 等待所有生产者线程完成
    for (auto& p : producers) {
        p.join();
    }

    // 等所有生产者完成后，注入哨兵
    // 注意：哨兵数量 = 消费者数量，确保每个消费者都能收到一个
    for (size_t i = 0; i < num_consumers; ++i) {
        q.Enqueue(-1);  // 注入哨兵，表示生产结束
    }

    // 等待所有消费者线程完成
    for (auto& c : consumers) {
        c.join();
    }

    // 验证生产和消费的总数是否一致
    EXPECT_EQ(produced.load(), total_items);
    EXPECT_EQ(consumed.load(), total_items);
    EXPECT_EQ(sum_produced.load(), sum_consumed.load());
}

// 测试超时出队（DequeueFor）
TEST_F(MPMCBlockingQueueTest, DequeueForTimeout) {
    MPMCBlockingQueue<int> q(3);

    // 队列为空，等待 50ms 应该超时返回
    auto start = std::chrono::steady_clock::now();
    int item;
    bool success = q.DequeueFor(item, std::chrono::milliseconds(50));  // 假设返回 bool 表示是否成功
    auto end = std::chrono::steady_clock::now();

    auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count();
    EXPECT_FALSE(success);
    EXPECT_GE(elapsed, 50);  // 至少等待了 50ms

    // 队列里有元素时，应该立即成功
    q.Enqueue(1);
    start = std::chrono::steady_clock::now();
    success = q.DequeueFor(item, std::chrono::milliseconds(50));
    end = std::chrono::steady_clock::now();
    elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count();

    EXPECT_TRUE(success);
    EXPECT_LT(elapsed, 50);  // 应该远小于 50ms
}

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}