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

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}