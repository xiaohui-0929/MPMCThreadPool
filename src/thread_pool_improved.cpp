/*
线程池函数实现
*/
#include "thread_pool_improved.h"

namespace thread_pool_improved {

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

    // 初始化默认配置
    config_.core_threads = thread_count;
    config_.max_threads = thread_count;
    config_.max_queue_size = max_queue_size;
    config_.queue_full_policy = queue_policy;


    // 创建工作线程
    for (size_t i = 0; i < thread_count; ++i) {
        workers_.emplace_back(&ThreadPool::WorkerLoop, this);
    }
}

ThreadPool::~ThreadPool() {
    Stop();
}

bool ThreadPool::Submit(std::unique_ptr<TaskBase> task) {
    // 先检查是否可以接受新任务
    if (!CanAcceptNewTasks()) {
        ThreadPoolState current_state = state_.load();
        std::string state_str;
        switch (current_state) {
            case ThreadPoolState::PAUSED:
                state_str = "暂停";
                break;
            case ThreadPoolState::SHUTTING_DOWN:
                state_str = "关闭中";
                break;
            case ThreadPoolState::FORCE_STOPPING:
                state_str = "强制关闭中";
                break;
            case ThreadPoolState::STOPPED:
                state_str = "已停止";
                break;
            default:
                state_str = "未知";
        }
        throw std::runtime_error("Cannot submit task to thread pool in current state: " + state_str);
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
        // 更新待处理任务数
        pending_tasks_++;
        // 任务队列条件变量（生产者）发起通知 告知线程池（消费者）进行处理
        queue_condition_.notify_one();
    }

    return submitted;
}

bool ThreadPool::CanAcceptNewTasks() const {
    ThreadPoolState current_state = state_.load();
    return current_state == ThreadPoolState::RUNNING;
}

}