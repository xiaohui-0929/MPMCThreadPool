/*
CircularQueue测试用例
使用Google Test框架
*/

#include "circular_queue.h"

#include <gtest/gtest.h>

// 引入CircularQueue
using thread_pool_improved::CircularQueue;

// 测试环境的初始化和清理
class CircularQueueTest : public ::testing::Test {
protected:
    void SetUp() override {};
    void TearDown() override {};
};

// 测试构造函数
TEST_F(CircularQueueTest, Constructors) {
    // 默认构造
    CircularQueue<int> q1;
    EXPECT_TRUE(q1.Empty());
    EXPECT_EQ(q1.Size(), 0);
    EXPECT_EQ(q1.Capacity(), 0);

    // 带参构造
    CircularQueue<int> q2(5);
    EXPECT_TRUE(q2.Empty());
    EXPECT_EQ(q2.Size(), 0);
    EXPECT_EQ(q2.Capacity(), 5);
    EXPECT_FALSE(q2.Full());

    // 容量为0的特殊情况
    CircularQueue<int> q3(0);
    EXPECT_TRUE(q3.Empty());
    EXPECT_EQ(q3.Capacity(), 0);
}

// 测试基本插入和弹出操作
TEST_F(CircularQueueTest, BasicOperations) {
    CircularQueue<int> q(3);

    // 测试插入
    int item = 10;
    q.PushBack(item);
    EXPECT_FALSE(q.Empty());
    EXPECT_EQ(q.Size(), 1);
    EXPECT_EQ(q.Front(), 10);
    
    q.PushBack(20);
    q.PushBack(30);
    EXPECT_EQ(q.Size(), 3);
    EXPECT_TRUE(q.Full());
    
    // 测试弹出
    q.PopFront();
    EXPECT_EQ(q.Size(), 2);
    EXPECT_EQ(q.Front(), 20);
    EXPECT_FALSE(q.Full());
    
    q.PopFront();
    EXPECT_EQ(q.Size(), 1);
    EXPECT_EQ(q.Front(), 30);
    
    q.PopFront();
    EXPECT_TRUE(q.Empty());
    EXPECT_EQ(q.Size(), 0);
}

// 测试溢出功能
TEST_F(CircularQueueTest, OverrunBehavior) {
    CircularQueue<int> q(3);
    
    // 填满队列
    q.PushBack(1);
    q.PushBack(2);
    q.PushBack(3);
    EXPECT_TRUE(q.Full());
    EXPECT_EQ(q.OverrunCounter(), 0);
    
    // 继续插入，应该覆盖最早的元素
    q.PushBack(4);  // 覆盖1
    EXPECT_EQ(q.OverrunCounter(), 1);
    EXPECT_EQ(q.Front(), 2);
    EXPECT_EQ(q.Size(), 3);
    
    q.PushBack(5);  // 覆盖2
    EXPECT_EQ(q.OverrunCounter(), 2);
    EXPECT_EQ(q.Front(), 3);
    
    // 重置溢出计数器
    q.ResetOverrunCounter();
    EXPECT_EQ(q.OverrunCounter(), 0);
}

// 测试异常情况
TEST_F(CircularQueueTest, ExceptionHandling) {
    CircularQueue<int> q(2);
    
    // 空队列访问应该抛出异常
    EXPECT_THROW(q.Front(), std::runtime_error);
    EXPECT_THROW(q.PopFront(), std::runtime_error);
    
    q.PushBack(1);
    
    // 索引越界应该抛出异常
    EXPECT_THROW(q.At(1), std::out_of_range);
    
    // 正常访问不应该抛出异常
    EXPECT_NO_THROW({
        int val = q.Front();
        int val2 = q.At(0);
        EXPECT_EQ(val, 1);
        EXPECT_EQ(val2, 1);
    });
}

// 测试索引访问
TEST_F(CircularQueueTest, IndexAccess) {
    CircularQueue<int> q(5);
    
    for (int i = 0; i < 3; ++i) {
        q.PushBack(i * 10);
    }
    
    // 测试At方法
    EXPECT_EQ(q.At(0), 0);
    EXPECT_EQ(q.At(1), 10);
    EXPECT_EQ(q.At(2), 20);
    
    // 测试operator[]
    EXPECT_EQ(q[0], 0);
    EXPECT_EQ(q[1], 10);
    EXPECT_EQ(q[2], 20);
    
    // 测试修改
    q.At(1) = 15;
    EXPECT_EQ(q.At(1), 15);
    
    q[2] = 25;
    EXPECT_EQ(q[2], 25);
}

// 测试移动语义
TEST_F(CircularQueueTest, MoveSemantics) {
    CircularQueue<std::string> q(3);
    
    // 测试移动插入
    std::string str1 = "hello";
    q.PushBack(std::move(str1));
    EXPECT_TRUE(str1.empty());  // 原字符串应该被移动
    
    // 测试移动构造
    CircularQueue<std::string> q2(std::move(q));
    EXPECT_EQ(q2.Size(), 1);
    EXPECT_EQ(q2.Front(), "hello");
    EXPECT_TRUE(q.Empty());  // 原队列应该为空
    
    // 测试移动赋值
    CircularQueue<std::string> q3;
    q3 = std::move(q2);
    EXPECT_EQ(q3.Size(), 1);
    EXPECT_EQ(q3.Front(), "hello");
    EXPECT_TRUE(q2.Empty());
}

// 测试EmplaceBack功能
TEST_F(CircularQueueTest, EmplaceBack) {
    CircularQueue<std::string> q(3);
    
    // 测试就地构造
    q.EmplaceBack("hello", 5);  // 使用string(const char*, size_t)构造函数
    EXPECT_EQ(q.Size(), 1);
    EXPECT_EQ(q.Front(), "hello");
    
    q.EmplaceBack(3, 'x');  // 使用string(size_t, char)构造函数
    EXPECT_EQ(q.Size(), 2);
    EXPECT_EQ(q.At(1), "xxx");
}

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}