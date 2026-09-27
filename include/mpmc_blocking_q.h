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
    explicit MPMCBlockingQueue(size_t max_items) 
        : circular_queue_(max_items) {}

    // 入队操作
    // 阻塞入队 如果队列已满 则阻塞
    void Enqueue(T&& item){
        {
            std::unique_lock<std::mutex> lock(queue_mutex_);
            // 队列已满则等待 
            // 此处使用 pop_cv_ 而不是 push_cv_ 是因为我们希望在队列满时等待出队操作来腾出空间
            pop_cv_.wait(lock, [this] { return !circular_queue_.Full(); });
            circular_queue_.PushBack(std::move(item));
        }
        // 通知进行了入队操作 唤醒一个正在 push_cv_ 上等待的消费者线程
        push_cv_.notify_one();
    }
    // 非阻塞入队 若队列已满 则覆盖最旧元素
    void EnqueueNowait(T&& item) {
        {
            std::unique_lock<std::mutex> lock(queue_mutex_);
            circular_queue_.PushBack(std::move(item));
        }
        push_cv_.notify_one();
    }
    // 非阻塞入队 若队列已满 则丢弃入队元素
    bool EnqueueIfHaveRoom(T&& item) {
        bool enqueued = false;
        {
            std::unique_lock<std::mutex> lock(queue_mutex_);
            if (!circular_queue_.Full()) {
                circular_queue_.PushBack(std::move(item));
                enqueued = true;
            }
        }
        if (enqueued) {
            push_cv_.notify_one();
        } else {
            // 队列已满 丢弃入队元素
            ++discard_counter_;
        }
        return enqueued;
    }

    // 出队操作
    // 阻塞出队 无超时限制
    void Dequeue(T& popped_item) {
        {
            std::unique_lock<std::mutex> lock(queue_mutex_);
            // 队列为空则等待
            push_cv_.wait(lock, [this] { return !circular_queue_.Empty(); });
            popped_item = std::move(circular_queue_.Front());
            circular_queue_.PopFront();
        }
        // 通知进行了出队操作 唤醒一个正在 pop_cv_ 上等待的生产者线程
        pop_cv_.notify_one();
    }
    // 阻塞出队 带超时限制 成功出队返回true 超时返回false
    bool DequeueFor(T& popped_item, std::chrono::milliseconds wait_duration) {
        {
            std::unique_lock<std::mutex> lock(queue_mutex_);
            // 队列为空则等待
            if (!push_cv_.wait_for(lock, wait_duration, [this] { return !circular_queue_.Empty();})) {
                // 超时
                return false;
            }
            popped_item = std::move(circular_queue_.Front());
            circular_queue_.PopFront();
        }
        // 通知进行了出队操作 唤醒一个正在 pop_cv_ 上等待的生产者线程
        pop_cv_.notify_one();
        return true;
    }

    // 状态查询
    // 获取当前队列大小
    size_t Size() const {
        std::unique_lock<std::mutex> lock(queue_mutex_);
        return circular_queue_.Size();
    }
    // 获取溢出计数器值
    size_t OverrunCounter() const {
        std::unique_lock<std::mutex> lock(queue_mutex_);
        return circular_queue_.OverrunCounter();
    }
    // 获取丢弃计数器值
    size_t DiscardCounter() const {
        return discard_counter_.load();
    }
    // 重置溢出计数器
    void ResetOverrunCounter() {
        std::unique_lock<std::mutex> lock(queue_mutex_);
        circular_queue_.ResetOverrunCounter();
    }
    // 重置丢弃计数器
    void ResetDiscardCounter() {
        discard_counter_.store(0);
    }

private:
    std::mutex queue_mutex_;                    // 队列互斥锁
    std::conditional_variable push_cv_;         // 入队条件变量
    std::conditional_variable pop_cv_;          // 出队条件变量
    CircularQueue<T> circular_queue_;           // 底层循环队列
    std::atomic<size_t> discard_counter_{0};    // 丢弃计数器
};
}

