#pragma once
/*
线程池实现类
*/

#include <functional>
#include <future>

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

}