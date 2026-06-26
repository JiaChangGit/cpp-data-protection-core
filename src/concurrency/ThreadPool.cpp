#include "dpc/concurrency/ThreadPool.hpp"

namespace dpc {

ThreadPool::ThreadPool(std::size_t workers, std::size_t queue_capacity)
    : queue_(queue_capacity) {
    if (workers == 0) {
        workers = 1;
    }
    workers_.reserve(workers);
    for (std::size_t i = 0; i < workers; ++i) {
        workers_.emplace_back([this] { workerLoop(); });
    }
}

ThreadPool::~ThreadPool() {
    shutdown();
}

bool ThreadPool::submit(std::function<void()> task) {
    if (stopped_.load()) {
        return false;
    }
    return queue_.push(std::move(task));
}

void ThreadPool::shutdown() {
    bool expected = false;
    if (!stopped_.compare_exchange_strong(expected, true)) {
        return;
    }
    queue_.shutdown();
    for (auto& worker : workers_) {
        if (worker.joinable()) {
            worker.join();
        }
    }
}

void ThreadPool::workerLoop() {
    std::function<void()> task;
    while (queue_.pop(task)) {
        task();
    }
}

}  // namespace dpc
