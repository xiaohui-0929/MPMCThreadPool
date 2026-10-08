/*
线程池函数实现
*/
#include "thread_pool_improved.h"

namespace thread_pool_improved {

// 基础含参构造函数
ThreadPool::ThreadPool(
    size_t thread_count, 
    size_t max_queue_size, 
    QueueFullPolicy queue_policy)
    : max_queue_size_(max_queue_size)
    , task_queue_(max_queue_size)
    , queue_policy_(queue_policy) {
    
    // 验证参数
    if (thread_count == 0) {
        throw std::invalid_argument("Thread count must be greater than 0");
    }

    // 创建工作线程
    for (size_t i = 0; i < thread_count; ++i) {
        workers_.emplace_back(&ThreadPool::WorkerLoop, this);
    }

    current_threads_ = thread_count;
}

// 析构函数
ThreadPool::~ThreadPool() {
    Stop();
}

// 停止线程池（立即停止）
void ThreadPool::Stop() {
    // 防止重复停止
    bool expected = false;  // 期望是false
    if (!stop_.compare_exchange_strong(expected, true)) {
        return;
    }

    // 唤醒所有等待的线程 让他们处理完剩余任务
    queue_condition_.notify_all();

    // 等待所有工作线程结束
    size_t joined_count = 0;
    for (auto& worker : workers_) {
        if (worker.joinable()) {
            worker.join();
            joined_count++;
        }
    }
}

// 任务提交函数
bool ThreadPool::Submit(std::unique_ptr<TaskBase> task) {

    // 停止状态下抛出超时异常
    if (stop_) {
        throw std::runtime_error("Cannot submit task to thread pool");
    }

    bool submitted = false;
    // 根据配置的不同策略 选择不同入队方式
    switch (queue_policy_) {
        case QueueFullPolicy::BLOCK:
            task_queue_.Enqueue(std::move(task));
            submitted = true;
            break;
        case QueueFullPolicy::OVERWRITE:
            task_queue_.EnqueueNowait(std::move(task));
            submitted = true;
            break;
        case QueueFullPolicy::DISCARD:
            submitted = task_queue_.EnqueueIfHaveRoom(std::move(task));
            break;
    }

    // 提交任务成功
    if (submitted) {
        pending_tasks_++;
        // 任务队列条件变量（生产者）发起通知 告知线程池（消费者）进行处理
        queue_condition_.notify_one();
    }

    return submitted;
}

// 获取当前队列中的任务数量
size_t ThreadPool::QueueSize() {
    std::lock_guard<std::mutex> lock(queue_mutex_);
    return task_queue_.Size();
}

// 获取当前活跃线程数
size_t ThreadPool::ActiveThreads() {
    return current_threads_.load();
}

// 等待所有线程执行结束
void ThreadPool::WaitAll() {
    // 执行完成的唯一条件: 待执行任务数归零
    std::unique_lock<std::mutex> lock(queue_mutex_);
    wait_condition_.wait(lock, [this] {
        return pending_tasks_ == 0;
    });
}

// 核心循环函数 每个线程独立运行 负责获取和执行任务
void ThreadPool::WorkerLoop() {
    
    // 核心循环
    while (true) {
        std::unique_ptr<TaskBase> task;
        bool has_task = false;
        {
            std::unique_lock<std::mutex> lock(queue_mutex_);

            // 等待任务或停止信号
            queue_condition_.wait(lock, [this] {
               return stop_ || task_queue_.Size() > 0;  // 当任务队列里有任务或终止时 才可执行后续
            });

            // 停止信号为true 且 没有任务待执行 直接返回线程
            if (stop_ && task_queue_.Size() == 0) {
                return;
            }

            // 获取任务
            if (task_queue_.Size() > 0) {
                task_queue_.Dequeue(task);
                has_task = true;
            }
        }
        // 执行任务
        if (has_task) {
            try {
                task->Execute();
            } catch (const std::exception& e) {
                // 捕获异常    
            } catch (...) {
                // 捕获未知异常
            }
            // 任务执行完成 更新计数
            pending_tasks_--;
            // 通知 有等待的地方 继续执行
            wait_condition_.notify_all();
        }
    }
}

// 判断是否可以添加新任务
bool ThreadPool::CanAcceptNewTasks() const {
    ThreadPoolState current_state = state_.load();
    return current_state == ThreadPoolState::RUNNING;
}

}