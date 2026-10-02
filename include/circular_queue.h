#pragma once
/*
使用std::vector实现循环队列
*/
#include <vector>
#include <stdexcept>
#include <cassert>

namespace thread_pool_improved {
template <typename T>
class CircularQueue {
public:
    using value_type = T;
    using size_type = std::size_t;
    using reference = T&;
    using const_reference = const T&;

    // 默认构造
    CircularQueue() = default;
    // 含参构造
    explicit CircularQueue(size_type capacity) 
        : capacity_(capacity > 0 ? capacity + 1 : 1)
        , items_(capacity_) {}

    // 拷贝构造和赋值
    CircularQueue(const CircularQueue&) = default;
    CircularQueue& operator=(const CircularQueue&) = default;

    // 移动构造
    CircularQueue(CircularQueue&& other) noexcept 
        : capacity_(other.capacity_)
        , head_(other.head_)
        , tail_(other.tail_)
        , overrun_counter_(other.overrun_counter_)
        , items_(std::move(other.items_)) {
        // 重置other状态
        other.capacity_ = 1;
        other.head_ = 0;
        other.tail_ = 0;
        other.overrun_counter_ = 0;
    }
    // 移动赋值
    CircularQueue& operator=(CircularQueue&& other) noexcept {
        if (this != &other) {
            capacity_ = other.capacity_;
            head_ = other.head_;
            tail_ = other.tail_;
            overrun_counter_ = other.overrun_counter_;
            items_ = std::move(other.items_);
            // 重置other状态
            other.capacity_ = 1;
            other.head_ = 0;
            other.tail_ = 0;
            other.overrun_counter_ = 0;
        }
        return *this;
    }

    // 核心操作
    // 入队（移动语义）
    void PushBack(T&& item) {
        if (capacity_ <= 1) return;

        // 使用移动语义避免拷贝
        items_[tail_] = std::move(item);
        tail_ = (tail_ + 1) % capacity_;    // 循环
        
        // 若队列已满，覆盖最旧数据
        if (tail_ == head_) {
            items_[head_] = T{};    // 主动覆盖最旧元素 避免外界卡死
            head_ = (head_ + 1) % capacity_;
            ++overrun_counter_;
        }
    }
    // 入队（拷贝语义）
    void PushBack(const T& item) {
        if (capacity_ <= 1) return;

        // 拷贝赋值
        items_[tail_] = item;
        tail_ = (tail_ + 1) % capacity_;    // 循环

        // 若队列已满，覆盖最旧数据
        if (tail_ == head_) {
            head_ = (head_ + 1) % capacity_;
            ++overrun_counter_;
        }
    }
    // 原地构造
    template<typename... Args>
    void EmplaceBack(Args&&... args) {
        if (capacity_ <= 1) return;

        // 直接在目标位置构建对象
        items_[tail_] = T(std::forward<Args>(args)...);
        tail_ = (tail_ + 1) % capacity_;    // 循环

        // 若队列已满，覆盖最旧数据
        if (tail_ == head_) {
            head_ = (head_ + 1) % capacity_;
            ++overrun_counter_;
        }
    }

    // 出队
    void PopFront() {
        if (Empty()) {
            throw std::runtime_error("Cannot pop form empty CircularQueue");
        }
        // 头指针移动覆盖
        head_ = (head_ + 1) % capacity_;
    }
    // 获得队首
    reference Front() {
        if (Empty()) {
            throw std::runtime_error("CircularQueue is empty");
        }
        return items_[head_];
    }
    // 获得队首 const
    const_reference Front() const {
        if (Empty()) {
            throw std::runtime_error("CircularQueue is empty");
        }
        return items_[head_];
    }

    // 状态查询
    bool Empty() const noexcept {
        // 头尾指针相同，表示为空
        return tail_ == head_;
    }
    bool Full() const noexcept {
        // 尾指针+1后下一个位置为头指针，表示已满
        return capacity_ > 1 && ((tail_ + 1) % capacity_) == head_;
    }
    size_type Size() const noexcept {
        if (tail_ >= head_) {
            // 尾指针在头指针之后或相等
            return tail_ - head_;
        } else {
            // 尾指针回绕
            return capacity_ - (head_ - tail_);
        }
    }
    size_type Capacity() const noexcept {
        return capacity_ > 1 ? capacity_ - 1 : 0;
    }

    // 随机访问
    // 含边界检查
    reference At(size_type index) {
        if (index >= Size()) {
            throw std::out_of_range("Index out of range");
        }
        return items_[(head_ + index) % capacity_];
    }
    const_reference At(size_type index) const {
        if (index >= Size()) {
            throw std::out_of_range("Index out of range");
        }
        return items_[(head_ + index) % capacity_];
    }
    // 不含边界检查
    reference operator[](size_type index) noexcept {
        assert(index < Size()); // 调试模式下检查
        return items_[(head_ + index) % capacity_];
    }
    const_reference operator[](size_type index) const noexcept {
        assert(index < Size()); // 调试模式下检查
        return items_[(head_ + index) % capacity_];
    }

    // 清空队列
    void Clear() noexcept {
        head_ = tail_ = 0;
        overrun_counter_ = 0;
    }

    // 获取溢出计数
    size_type OverrunCounter() const noexcept {
        return overrun_counter_;
    }

    // 重置溢出计数
    void ResetOverrunCounter() noexcept {
        overrun_counter_ = 0;
    }

private:
    size_type capacity_ = 1;        // 实际容量（+1用于区分满和空）
    size_type head_ = 0;            // 头指针
    size_type tail_ = 0;            // 尾指针
    size_type overrun_counter_ = 0; // 溢出计数
    std::vector<T> items_;          // 存储容器
};
}