#pragma once

#include "dpc/concurrency/BoundedQueue.hpp"

#include <atomic>
#include <functional>
#include <thread>
#include <vector>

namespace dpc {

class ThreadPool {
public:
    ThreadPool(std::size_t workers, std::size_t queue_capacity);
    ThreadPool(const ThreadPool&) = delete;
    ThreadPool& operator=(const ThreadPool&) = delete;
    ~ThreadPool();

    bool submit(std::function<void()> task);
    void shutdown();

private:
    void workerLoop();

    BoundedQueue<std::function<void()>> queue_;
    std::vector<std::thread> workers_;
    std::atomic<bool> stopped_ {false};
};

}  // namespace dpc
