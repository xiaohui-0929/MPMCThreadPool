#pragma once
/*
线程池实现类
*/
#include "mpmc_blocking_q.h"

#include <functional>
#include <future>
#include <vector>
#include <thread>

namespace thread_pool_improved {

// 任务基类(抽象类)
class TaskBase {
public:
    // 虚析构函数 保证子类资源正确释放
    virtual ~TaskBase() = default;

    // 任务执行 纯虚函数 子类重写
    virtual void Execute() = 0;

    // 获取任务执行状态 虚函数 子类重写
    virtual bool IsExecutionSuccessful() const { return true; }

protected:
    // 构造函数 访问类型protected 防止外部实例化
    TaskBase() = default;

    // 禁止拷贝构造
    TaskBase(const TaskBase&) = delete;
    // 禁止拷贝赋值
    TaskBase& operator=(const TaskBase&) = delete;

    // 默认移动构造
    TaskBase(TaskBase&&) = default;
    // 默认移动赋值
    TaskBase& operator=(TaskBase&&) = default;
};

// 带返回值的任务模板
template<typename T>
class FutureTase : public TaskBase {
public:
    // 含参构造函数 explicit防止隐式转换
    explicit FutureTase(std::function<T()> func) : func_(std::move(func)) {}

    // 重写基类虚函数 Execute
    void Execute() override {
        // 捕获异常 保证执行期间的异常可以正确传播
        try {
            // 编译期分支判断 对返回类型为void的任务特化处理
            if constexpr (std::is_void_v(T)) {
                func_();    // 任务执行
                promise_.set_value();
                execution_success_ = true;
            } else {
                promise_.set_value(func_());
            }
        } catch (...) {
            // promise_捕获异常
            promise_.set_exception(std::current_exception());
            execution_success_ = false;
        }
    }

    // 重写基类虚函数 IsExecutionSuccessful
    bool IsExecutionSuccessful() const override {
        return execution_success_;
    }

    // 获取任务执行结果
    std::future<T> GetFuture() {
        return promise_.get_future();
    }
    
private:
    std::function<T()> func_;           // 任务执行的函数对象
    std::promise<T> promise_;           // 用于设置返回值的promise
    bool execution_success_ = false;    // 函数执行状态
};

// 满队列处理策略
enum class QueueFullPolicy {
    BLOCK,          // 阻塞等待（默认）
    OVERWRITE,      // 覆盖最旧任务
    DISCARD         // 丢弃新任务
};

// 线程池相关配置
struct ThreadPoolStruct {
    size_t core_threads = std::thread::hardware_concurrency();      // 核心线程数
    size_t max_threads = std::thread::hardware_concurrency() * 2;   // 最大线程数
    size_t max_queue_size = 1000;                                   // 最大队列大小 0表示无限制
    QueueFullPolicy queue_full_policy = QueueFullPolicy::BLOCK;     // 满队列处理策略
};

// 线程池状态
enum class ThreadPoolState {
    RUNNING,        // 正常运行状态
    PAUSED,         // 暂停状态（不接受新任务，已有任务暂停执行）
    SHUTTING_DOWN,  // 优雅关闭中（不接受新任务，等待现有任务完成）
    FORCE_STOPPING, // 强制停止中（不接受新任务，尽快停止）
    STOPPED         // 已停止
};

// 线程池类
class ThreadPool {
public:

    // 定义任务队列别名
    // 元素使用TaskBase的独占智能指针 保证任务所有权归属于线程池
    using queue_type = MPMCBlockingQueue<std::unique_ptr<TaskBase>>;

    // 构造函数 指定线程数、最大队列大小、满队列处理策略
    ThreadPool(size_t thread_count = std::thread::hardware_concurrency(),
               size_t max_queue_size = 1000,
               QueueFullPolicy queue_policy = QueueFullPolicy::BLOCK);
    
    // 析构函数
    ~ThreadPool();

    // 任务提交（传入基类指针）
    // 返回值：true任务提交成功 false任务被丢弃（当满队列策略为DISCARD时）
    bool Submit(std::unique_ptr<TaskBase> task);

    // 任务提交（带返回值的任务）
    template<typename F, typename... Args>
    auto SubmitWithResult(F&& func, Args&&... args)
        -> std::future<typename std::invoke_result_t<F, Args...>>;

    // 停止线程池（立即停止）
    void Stop();

private:
    // 工作线程主循环函数
    void WorkerLoop();

    // 状态控制私有方法
    bool CanAcceptNewTasks() const;     // 检查是否可以接受新任务

private:
    // 线程管理
    std::vector<std::thread> workers_;      // 工作线程容器
    std::atomic<size_t> pending_tasks_{0};  // 待处理任务计数

    // 同步原语
    std::condition_variable queue_condition_;   // 任务队列条件变量
    
    // 配置参数
    size_t max_queue_size_;             // 最大任务队列大小
    queue_type task_queue_;             // 任务队列
    QueueFullPolicy queue_policy_;      // 满队列处理策略
    ThreadPoolStruct config_;           // 线程池配置信息

    // 线程池状态（原子变量）
    std::atomic<ThreadPoolState> state_{ThreadPoolState::RUNNING};

};

// 模板函数SubmitWithResult实现
template<typename F, typename... Args>
auto ThreadPool::SubmitWithResult(F&& func, Args&&... args)
    -> std::future<typename std::invoke_result_t<F, Args...>> {
        
        using ReturnType = typename std::invoke_result_t<F, Args...>;

        // 绑定函数和参数 完美转发保证类型不发生改变
        auto bound_func = std::bind(std::forward<F>(func), std::forward<Args>(args)...);
        // 创建任务 获取返回future
        // 此处使用move 减少可调用对象拷贝可能导致的效率问题
        auto task = std::make_unique<FutureTase<ReturnType>>(std::move(bound_func));
        auto future = task->GetFuture();
        
        // 提交任务
        // 此处使用move 将任务的所有权进行转移 保证生命周期安全
        bool submitted = Submit(std::move(task));
        if (!submitted) {
            // 新建一个promise返回异常
            std::promise<ReturnType> promise;
            promise.set_exception(
                std::make_exception_ptr(std::future_error(std::future_errc::broken_promise))
            );
            return promise.get_future();
        }
        return future;
    }

}