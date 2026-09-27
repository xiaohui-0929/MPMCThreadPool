#pragma once
/*
使用CircularQueue实现MPMC阻塞队列
*/
#include "circular_queue.h"

#include <mutex>
#include <condition_variable>
#include <atomic>
#include <chrono>

namespace thread_pool_improved {
template <typename T>
class MPMCBlockingQueue {
public:
    // 构造函数
    explicit MPMCBlockingQueue(size_t max_items);

    // 入队操作
    // 阻塞入队 如果队列已满 则阻塞
    void Enqueue(T&& item);
    // 非阻塞入队 若队列已满 则覆盖最旧元素
    void EnqueueNowait(T&& item);
    // 非阻塞入队 若队列已满 则丢弃入队元素
    bool EnqueueIfHaveRoom(T&& item);

    // 出队操作
    // 阻塞出队 无超时限制
    void Dequeue(T& popped_item);
    // 阻塞出队 带超时限制 成功出队返回true 超时返回false
    bool DequeueFor(T& popped_item, std::chrono::milliseconds wait_duration);

    // 状态查询
    // 获取当前队列大小
    size_t Size() const;
    // 获取溢出计数器值
    size_t OverrunCounter() const;
    // 获取丢弃计数器值
    size_t DiscardCounter() const;
    // 重置溢出计数器
    void ResetOverrunCounter();
    // 重置丢弃计数器
    void ResetDiscardCounter();

private:
    std::mutex queue_mutex_;                    // 队列互斥锁
    std::conditional_variable push_cv_;         // 入队条件变量
    std::conditional_variable pop_cv_;          // 出队条件变量
    CircularQueue<T> circular_queue_;           // 底层循环队列
    std::atomic<size_t> discard_counter_{0};    // 丢弃计数器
};
}

