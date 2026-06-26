#include "dpc/concurrency/BoundedQueue.hpp"

#include <gtest/gtest.h>
#include <thread>

TEST(BoundedQueueTest, PushPop) {
    dpc::BoundedQueue<int> queue(2);
    EXPECT_TRUE(queue.push(1));
    int value = 0;
    EXPECT_TRUE(queue.pop(value));
    EXPECT_EQ(value, 1);
}

TEST(BoundedQueueTest, ShutdownWakesPop) {
    dpc::BoundedQueue<int> queue(1);
    bool popped = true;
    std::thread worker([&] {
        int value = 0;
        popped = queue.pop(value);
    });
    queue.shutdown();
    worker.join();
    EXPECT_FALSE(popped);
    EXPECT_FALSE(queue.push(2));
}
