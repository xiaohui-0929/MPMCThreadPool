/*
线程池函数实现
*/
#include "thread_pool_improved.h"

#include <iostream>

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

    // 初始化默认配置（默认关闭动态线程管理）
    config_.core_threads = thread_count;
    config_.max_threads = thread_count;
    config_.max_queue_size = max_queue_size;
    config_.queue_full_policy = queue_policy;
    config_.enable_dynamic_threads = false;

    // 初始化动态线程管理相关数据结构
    thread_last_active_.resize(thread_count);
    thread_idle_count_.reserve(thread_count);
    thread_should_exit_.reserve(thread_count);

    for (size_t i = 0; i < thread_count; ++i) {
        thread_idle_count_.emplace_back(std::make_unique<std::atomic<size_t>>(0));
        thread_should_exit_.emplace_back(std::make_unique<std::atomic<bool>>(false));
    }

    // 创建工作线程
    for (size_t i = 0; i < thread_count; ++i) {
        workers_.emplace_back(&ThreadPool::WorkerLoop, this);
        thread_last_active_[i] = std::chrono::steady_clock::now();
    }

    current_threads_ = thread_count;
    stats_.peak_threads = thread_count;
    stats_.threads_created = thread_count;
}

ThreadPool::ThreadPool(const ThreadPoolStruct& config) 
    : max_queue_size_(config.max_queue_size)
    , task_queue_(config.max_queue_size) 
    , queue_policy_(config.queue_full_policy)
    , config_(config) {
        
    // 验证参数
    if (config.core_threads  == 0) {
        throw std::invalid_argument("Thread count must be greater than 0");
    }

    // 初始化动态线程管理相关数据结构
    thread_last_active_.resize(config.max_threads);
    thread_idle_count_.reserve(config.max_threads);
    thread_should_exit_.reserve(config.max_threads);

    for (size_t i = 0; i < config.max_threads; ++i) {
        thread_idle_count_.emplace_back(std::make_unique<std::atomic<size_t>>(0));
        thread_should_exit_.emplace_back(std::make_unique<std::atomic<bool>>(false));
    }

    // 创建工作线程
    for (size_t i = 0; i < config.core_threads; ++i) {
        workers_.emplace_back(&ThreadPool::WorkerLoop, this);
        thread_last_active_[i] = std::chrono::steady_clock::now();
    }

    current_threads_ = config_.core_threads;
    stats_.peak_threads = config_.core_threads;
    stats_.threads_created = config_.core_threads;

    // 启动负载均衡线程
    if (config_.enable_dynamic_threads) {
        load_balancer_thread_ = std::thread(&ThreadPool::LoadBalancingLoop, this);
    }
}

// 析构函数
ThreadPool::~ThreadPool() {
    Stop();
}

// 停止线程池
void ThreadPool::Stop(bool drain_queue) {
    // 防止重复停止
    bool expected = false;  // 期望是false
    if (!stop_.compare_exchange_strong(expected, true)) {
        return;
    }

    // 将状态设置为优雅关闭 处理完剩余任务
    {
        std::lock_guard<std::mutex> lock(state_mutex_);
        
        if (drain_queue) {
            SetState(ThreadPoolState::SHUTTING_DOWN);        // 消费队列
        } else {
            SetState(ThreadPoolState::PAUSED_SHUTTING_DOWN); // 不消费队列
        }
    }

    // 停止负责均衡检查线程
    if (config_.enable_dynamic_threads) {
        load_balancer_stop_ = true;
    }

    // 唤醒所有等待的线程 让他们处理完剩余任务
    queue_condition_.notify_all();
    wait_condition_.notify_all();
    pause_condition_.notify_all();

    // 等待负载均衡线程结束
    if (config_.enable_dynamic_threads && load_balancer_thread_.joinable()) {
        load_balancer_thread_.join();
    }

    // 等待所有工作线程结束
    size_t joined_count = 0;
    for (auto& worker : workers_) {
        if (worker.joinable()) {
            worker.join();
            joined_count++;
        }
    }

    // 设置状态为终止
    {
        std::lock_guard<std::mutex> lock(state_mutex_);
        SetState(ThreadPoolState::STOPPED);
    }
}

 // 优雅关闭线程池
void ThreadPool::Shutdown(ShutdownOption option, std::chrono::milliseconds timeout) {
    bool should_proceed = false;
    ThreadPoolState original_state;

    bool original_paused = false;   // 初始状态为暂停状态

    {
        std::lock_guard<std::mutex> lock(state_mutex_);
        original_state = state_.load();

        // 以下状态直接返回
        if (original_state == ThreadPoolState::STOPPED ||
            original_state == ThreadPoolState::SHUTTING_DOWN ||
            original_state == ThreadPoolState::PAUSED_SHUTTING_DOWN ||
            original_state == ThreadPoolState::FORCE_STOPPING) {
            return;
        }

        should_proceed = true;

        // 通过入参option 设置线程池状态
        switch (option) {
            case ShutdownOption::GRACEFUL:
                SetState(ThreadPoolState::SHUTTING_DOWN);
                break;
            case ShutdownOption::FORCE:
                SetState(ThreadPoolState::FORCE_STOPPING);
                break;
            case ShutdownOption::TIMEOUT:
                SetState(ThreadPoolState::SHUTTING_DOWN);
                break;
            default:
                break;
        }

        // 若初始状态为暂停 则设置状态为PAUSED_SHUTTING_DOWN 唤醒阻塞线程
        if (original_state == ThreadPoolState::PAUSED) {
            original_paused = true;
            SetState(ThreadPoolState::PAUSED_SHUTTING_DOWN);
            pause_condition_.notify_all();
            queue_condition_.notify_all();
        }
    }

    if (!should_proceed) {
        return;
    }

    if (option == ShutdownOption::FORCE) {
        // 强制停止
        ForceStop();
        return;
    }

    // 超时关闭
    if (option == ShutdownOption::TIMEOUT) {
        if (!WaitAll(timeout)) {
            // 强制停止
            ForceStop();
            return;
        }
    } else if (option == ShutdownOption::GRACEFUL) {
        // 优雅关闭
        if (!original_paused) {
            WaitAll();
        } else {
            // 处理完正在执行的任务
            WaitForRunningTasks();
            // 从暂停状态转为优雅关闭 不执行完任务队列
            Stop(false);
            return;
        }
    }

    // 设置最终停止状态
    Stop();
}

// 任务提交函数
bool ThreadPool::Submit(std::unique_ptr<TaskBase> task) {
    // 先判断当前状态
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
                state_str = "强制停止中";
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
        pending_tasks_++;
        // 任务队列条件变量（生产者）发起通知 告知线程池（消费者）进行处理
        queue_condition_.notify_one();
        // 检查是否需要创建新线程
        // 可以考虑将多扩容策略封装 此处直接调用
        if (config_.enable_dynamic_threads && 
            pending_tasks_.load() >= config_.thread_creation_threshold && 
            current_threads_.load() < config_.max_threads) {
            
            double load_factor = CalculateLoadFactor();
            if (load_factor > config_.scale_up_threshold) {
                std::lock_guard<std::mutex> lock(thread_management_mutex_);
                TryCreateNewThread();
            }
        }
    }

    return submitted;
}

// 暂停线程池
void ThreadPool::Pause() {
    std::lock_guard<std::mutex> lock(state_mutex_);

    ThreadPoolState current_state = state_.load();
    if (current_state == ThreadPoolState::RUNNING) {
        SetState(ThreadPoolState::PAUSED);
    }
}

// 恢复线程池
void ThreadPool::Resume() {
    std::lock_guard<std::mutex> lock(state_mutex_);

    ThreadPoolState current_state = state_.load();
    if (current_state == ThreadPoolState::PAUSED) {
        SetState(ThreadPoolState::RUNNING);
        pause_condition_.notify_all();  // 唤醒等待的线程
    }
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

// 等待所有线程执行结束（带限时）
bool ThreadPool::WaitAll(std::chrono::milliseconds timeout) {
    std::unique_lock<std::mutex> lock(queue_mutex_);
    return wait_condition_.wait_for(lock, timeout, [this] {
        return pending_tasks_.load() == 0;
    });
}

// 等待正在执行的任务执行结束
void ThreadPool::WaitForRunningTasks() {
    std::unique_lock<std::mutex> lock(queue_mutex_);
    wait_condition_.wait(lock, [this] {
        return running_tasks_ == 0;
    });
}

// 触发负载检查
void ThreadPool::TriggerLoadCheck() {
    if (!config_.enable_dynamic_threads) {
        return;
    }

    std::lock_guard<std::mutex> lock(thread_management_mutex_);

    // 计算负载因子
    double load_factor = CalculateLoadFactor();
    // 获取当前线程数
    size_t current_count = current_threads_.load();
    // 获取待处理任务数
    size_t pending = pending_tasks_.load();

    // 检查是否需要扩容
    // 扩容策略
    // 1. 负载因子策略
    // 2. 任务积压策略
    // 3. 任务密度策略
    bool should_expand = false;

    // 判断是否需要扩容
    if (current_count < config_.max_threads) {
        // 策略1 负载因子过高
        if (load_factor > config_.scale_up_threshold && pending >= config_.thread_creation_threshold) {
            // 负载因子高于额定扩容阈值 且 待处理任务数高于额定阈值
            should_expand = true;
        } 
        // 策略2 积压任务过多
        else if (pending >= config_.thread_creation_threshold * 2) {
            // 待处理任务数大于等于额定阈值的二倍
            should_expand = true;
        }
        // 策略3 任务密度过高
        else if (current_count > 0 && static_cast<double>(pending) / current_count > 2.0) {
            // 每个线程平均待处理任务数超过2个
            should_expand = true;
        }

        if (should_expand) {
            TryCreateNewThread();
        }
    }
    // 判断是否需要缩容
    else if (load_factor < config_.scale_down_threshold && current_count > config_.core_threads) {
        TryRemoveIdleThread();
    }

    return;
}

// 核心循环函数 每个线程独立运行 负责获取和执行任务
void ThreadPool::WorkerLoop() {
    // 获取当前线程id
    std::thread::id thread_id = std::this_thread::get_id();

    // 查找当前线程的索引
    size_t thread_index = SIZE_MAX;
    {
        std::lock_guard<std::mutex> lock(thread_management_mutex_);
        for (size_t i = 0; i < workers_.size(); ++i) {
            if (workers_[i].get_id() == thread_id) {
                thread_index = i;
                break;
            }
        }
    }

    // 核心循环
    while (true) {
        std::unique_ptr<TaskBase> task;
        bool has_task = false;
        {
            std::unique_lock<std::mutex> lock(queue_mutex_);

            // 检查线程退出标志
            if (thread_index != SIZE_MAX && thread_should_exit_[thread_index]->load()) {
                return;
            }

            // 检查是否处于暂停状态
            ThreadPoolState current_pause_state = state_.load();
            if (current_pause_state == ThreadPoolState::PAUSED) {
                // 释放队列锁
                lock.unlock();
                // 通过pause_condition_阻塞线程
                {
                    std::unique_lock<std::mutex> pause_lock(state_mutex_);
                    pause_condition_.wait(pause_lock, [this] {
                        return stop_ || state_.load() != ThreadPoolState::PAUSED;
                    });
                }
                // 获取队列锁
                lock.lock();
                ThreadPoolState resumed_state = state_.load();
                // 检查恢复后的状态
                if (resumed_state == ThreadPoolState::SHUTTING_DOWN) {
                    // 优雅关闭状态下 不再执行后续任务
                    return;
                } else if (resumed_state == ThreadPoolState::RUNNING) {
                    // 恢复到运行状态 继续处理队列任务
                    if (task_queue_.Size() > 0) {
                        continue;   // 跳过等待，直接进入下一轮循环获取任务
                    }
                }
            }

            // 等待任务或停止信号
            if (config_.enable_dynamic_threads && !stop_) {
                // 启用动态线程管理 且 不处于停止时
                queue_condition_.wait_for(lock, config_.thread_idle_timeout, 
                    [this, &thread_index] {
                        // 任务队列有任务时才继续，否则超时等待
                        return stop_ || (task_queue_.Size() > 0) 
                        || (thread_index != SIZE_MAX && thread_should_exit_[thread_index]->load());
                });
            } else {
                // 不启用动态线程管理 或 处于停止时
                queue_condition_.wait(lock, [this] {
                    return stop_ || task_queue_.Size() > 0;  // 当任务队列里有任务或终止时 才可执行后续
                });
            }

            // 检查关闭状态
            ThreadPoolState current_state = state_.load();

            // 强制停止 / 暂停关闭：直接退出，不消费队列
            if (current_state == ThreadPoolState::FORCE_STOPPING ||
                current_state == ThreadPoolState::PAUSED_SHUTTING_DOWN) {
                return;
            }

            // 正常优雅关闭：队列空了才退出
            if (current_state == ThreadPoolState::SHUTTING_DOWN || stop_) {
                if (task_queue_.Size() == 0) {
                    return;
                }
                // 否则继续往下取任务
            }

            // 获取任务
            if (task_queue_.Size() > 0) {
                task_queue_.Dequeue(task);
                has_task = true;
                active_threads_++;  // 活动线程加一

                // 更新线程活跃度
                if (thread_index != SIZE_MAX) {
                    UpdateThreadActivity(thread_index);
                }
            } else {
                // 没有任务 增加空闲计数
                if (thread_index != SIZE_MAX && config_.enable_dynamic_threads) {
                    thread_idle_count_[thread_index]->fetch_add(1);

                    // 非核心线程 检查是否应该退出
                    if (thread_index >= config_.core_threads) {
                        auto now = std::chrono::steady_clock::now();
                        auto duration = now - thread_last_active_[thread_index];
                        if (duration >= config_.thread_idle_timeout &&
                            thread_idle_count_[thread_index]->load() >= config_.max_consecutive_idle_checks) {
                            thread_should_exit_[thread_index]->store(true);
                            return;
                        }
                    }
                }
            }
        }
        // 执行任务
        if (has_task) {
            running_tasks_++;   // 增加正在执行的任务计数

            // 强制停止状态下，跳过任务执行
            ThreadPoolState current_exec_state = state_.load();
            if (current_exec_state == ThreadPoolState::FORCE_STOPPING) {
                running_tasks_--;
                pending_tasks_--;
                wait_condition_.notify_all();   // 通知 有等待的地方 继续执行
                return;
            }

            try {
                auto start_time = std::chrono::steady_clock::now();
                task->Execute();
                auto end_time = std::chrono::steady_clock::now();

                // 更新统计信息
                {
                    std::lock_guard<std::mutex> stats_lock(stats_mutex_);
                    auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end_time - start_time);
                    double task_time_ms = duration.count() / 1000.0;

                    // 检查任务执行是否成功
                    if (task->IsExecutionSuccessful()) {
                        stats_.tasks_completed++;
                        stats_.avg_task_time_ms = 
                            (stats_.avg_task_time_ms * (stats_.tasks_completed - 1) + task_time_ms) / stats_.tasks_completed;
                    } else {
                        stats_.tasks_failed++;
                    }
                }
            } catch (const std::exception& e) {
                // 捕获异常 
                {
                    std::lock_guard<std::mutex> stats_lock(stats_mutex_);
                    stats_.tasks_failed++;
                }   
            } catch (...) {
                // 捕获未知异常
                {
                    std::lock_guard<std::mutex> stats_lock(stats_mutex_);
                    stats_.tasks_failed++;
                }  
            }
            // 任务执行完成 更新计数
            running_tasks_--;
            pending_tasks_--;
            active_threads_--;
            // 通知 有等待的地方 继续执行
            wait_condition_.notify_all();
        }
    }
}

// 获取线程池统计信息
ThreadPool::Stats ThreadPool::GetStats() const {
    std::lock_guard<std::mutex> lock(stats_mutex_);

    Stats stats = stats_;
    stats.active_threads = active_threads_.load();
    stats.load_factor = CalculateLoadFactor();

    stats.current_queue_size = task_queue_.Size();
    stats.max_queue_size = max_queue_size_;

    if (max_queue_size_ > 0) {
        stats.queue_usage_rate = static_cast<double>(stats.current_queue_size) / max_queue_size_;
    } else {
        stats.queue_usage_rate = 0.0;   // 无限制队列
    }

    // 更新峰值队列大小
    if (stats.current_queue_size > stats_.peak_queue_size) {
        // 不是很建议用const_cast
        const_cast<ThreadPool*>(this)->stats_.peak_queue_size = stats.current_queue_size;
        if (max_queue_size_ > 0) {
            const_cast<ThreadPool*>(this)->stats_.peak_queue_usage_rate = 
                static_cast<double>(stats.current_queue_size) / max_queue_size_;
        }
    }
    stats.peak_queue_size = stats_.peak_queue_size;
    stats.peak_queue_usage_rate = stats_.peak_queue_usage_rate;

    stats.tasks_discarded = task_queue_.DiscardCounter();
    stats.tasks_overwritten = task_queue_.OverrunCounter();

    return stats;
}

// 负载均衡循环函数 负责周期性检查线程状态
void ThreadPool::LoadBalancingLoop() {
    while (!load_balancer_stop_.load()) {
        try {
            // 先休眠一段间隔
            std::this_thread::sleep_for(config_.load_check_interval);

            // 如果状态改变则退出
            if (load_balancer_stop_.load()) {
                break;
            }

            // 触发负责均衡检查
            TriggerLoadCheck();
            // 清理线程
            CleanupFinishedThreads();

        } catch (const std::exception& e) {
            // 明确问题的异常
        } catch (...) {
            // 未知原因的异常
        }
    }
}

// 清理已结束线程
void ThreadPool::CleanupFinishedThreads() {
    std::lock_guard<std::mutex> lock(thread_management_mutex_);

    // 遍历当前线程池
    for (size_t i = 0; i < workers_.size(); ++i) {
        // 检查当前线程是否可被清理 且 当前线程可join
        if (thread_should_exit_[i]->load() && workers_[i].joinable()) {
            // 等待线程结束
            try {
                workers_[i].join();
                current_threads_--;

                {
                    std::lock_guard<std::mutex> stats_lock(stats_mutex_);
                    stats_.threads_destroyed++;
                }

            } catch (const std::exception& e) {
                // 线程回收时发生异常
            } catch (...) {
                // 出现未知异常
            }
        }
    }
    return;
}

// 计算负载因子
double ThreadPool::CalculateLoadFactor() const {
    size_t current_count = current_threads_.load();
    if (current_count == 0) return 0.0;

    size_t active_count = active_threads_.load();
    return static_cast<double>(active_count) / static_cast<double>(current_count);
}

// 尝试创建新线程
bool ThreadPool::TryCreateNewThread() {
    size_t current_count = current_threads_.load();

    if (current_count >= config_.max_threads) {
        return false;   // 达到最大线程限制
    }

    try {
        // 查找可用的线程索引
        size_t thread_index = current_count;

        // 判断线程索引的状态 处理策略不同
        if (thread_index < workers_.size()) {
            // 1. 索引未超过当前线程容器总数
            // 将对应位置的线程执行完毕
            if (workers_[thread_index].joinable()) {
                workers_[thread_index].join();
            }
            // 重新替换为新线程
            workers_[thread_index] = std::thread(&ThreadPool::WorkerLoop, this);
        } else {
            // 2. 索引已经超过了当前线程容器总数
            // 在线程容器内追加一个新线程
            workers_.emplace_back(&ThreadPool::WorkerLoop, this);
        }

        // 将对应位置并行数组状态调整
        thread_last_active_[thread_index] = std::chrono::steady_clock::now();
        thread_idle_count_[thread_index]->store(0);
        thread_should_exit_[thread_index]->store(false);

        current_threads_++;

        {
            std::lock_guard<std::mutex> stats_lock(stats_mutex_);
            stats_.threads_created++;
            if (current_threads_.load() > stats_.peak_threads) {
                stats_.peak_threads = current_threads_.load();
            }
        }

        return true;

    } catch (const std::exception& e) {
        // 出现异常
        return false;
    } catch (...) {
        // 出现未知异常
        return false;
    }
}

// 尝试回收空闲线程
bool ThreadPool::TryRemoveIdleThread() {
    size_t current_count = current_threads_.load();

    if (current_count <= config_.core_threads) {
        return false;   // 不可小于核心线程数
    }

    // 开始回收时的基准时间
    auto now = std::chrono::steady_clock::now();

    // 查找可回收的空闲线程
    for (size_t i = config_.core_threads; i < current_count; ++i) {
        auto idle_duration = now - thread_last_active_[i];  // 计算当前线程的空闲时间
        if (idle_duration >= config_.min_idle_time_for_removal && 
            thread_idle_count_[i]->load() >= config_.max_consecutive_idle_checks) {

            // 标记线程被回收
            thread_should_exit_[i]->store(true);
            // 唤醒线程 让其检查退出标准
            queue_condition_.notify_all();

            return true;
        }
    }

    return false;
}

// 更新线程活跃度
void ThreadPool::UpdateThreadActivity(size_t thread_index) {
    if (thread_index < thread_last_active_.size()) {
        thread_last_active_[thread_index] = std::chrono::steady_clock::now();
        thread_idle_count_[thread_index]->store(0);
        thread_should_exit_[thread_index]->store(false);
    }
}

// 判断是否可以添加新任务
bool ThreadPool::CanAcceptNewTasks() const {
    ThreadPoolState current_state = state_.load();
    return current_state == ThreadPoolState::RUNNING;
}

// 设置线程池状态
void ThreadPool::SetState(ThreadPoolState new_state) {
    state_.store(new_state);
}

// 强制停止
void ThreadPool::ForceStop() {
    // 防止重复停止
    bool expected = false;
    if (!stop_.compare_exchange_strong(expected, true)) {
        return;
    }

    // 保险起见 设置状态为强制关闭
    {
        std::lock_guard<std::mutex> lock(state_mutex_);
        SetState(ThreadPoolState::FORCE_STOPPING);
    }

    // 停止负载均衡线程
    if (config_.enable_dynamic_threads) {
        load_balancer_stop_ = true;
    }
    
    {
        std::lock_guard<std::mutex> lock(queue_mutex_);
        // 所有线程 设置 退出标志
        for (auto& should_exit : thread_should_exit_) {
            should_exit->store(true);
        }
    }

    queue_condition_.notify_all();
    wait_condition_.notify_all();
    pause_condition_.notify_all();

    // 等待负载均衡线程结束
    if (config_.enable_dynamic_threads && load_balancer_thread_.joinable()) {
        load_balancer_thread_.join();
    }

    // 等待所有工作线程结束
    size_t joined_count = 0;
    for (auto& worker : workers_) {
        if (worker.joinable()) {
            try {
                // 使用join
                worker.join();
                joined_count++;
            } catch (const std::exception& e) {
                // 日志记录异常
            }
        }
    }

    // 设置状态为终止
    {
        std::lock_guard<std::mutex> lock(state_mutex_);
        SetState(ThreadPoolState::STOPPED);
    }
}

}