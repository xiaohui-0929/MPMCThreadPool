#pragma once

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
    explicit CircularQueue(size_type capacity);

    // 核心操作
    // 入队（右值）
    void PushBack(T&& item);
    // 入队（左值）
    void PushBack(const T& item);
    // 原地构造
    template<typename... Args>
    void EmplaceBack(Args&&... args);

    // 出队
    void PopFront();
    // 获得队首
    reference Front();
    // 获得队首 const
    const_reference Front() const;

    // 状态查询
    bool Empty() const noexcept;
    bool Full() const noexcept;
    size_type Size() const noexcept;
    size_type Capacity() const noexcept;

    // 随机访问
    reference At(size_type index);
    const_reference At(size_type index) const;
    reference operator[](size_type index) noexcept;
    const_reference operator[](size_type index) const noexcept;

private:
    size_type capacity_ = 1;        // 实际容量（+1用于区分满和空）
    size_type head_ = 0;            // 头指针
    size_type tail_ = 0;            // 尾指针
    size_type overrun_counter_ = 0; // 溢出计数
    std::vector<T> items_;          // 存储容器
};
