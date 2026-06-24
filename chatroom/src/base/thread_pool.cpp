#include "base/thread_pool.h"

namespace chat {

ThreadPool::ThreadPool(std::size_t threads, std::size_t max_queue)
    : max_queue_(max_queue) {
    if (threads == 0) threads = 1;
    for (std::size_t i = 0; i < threads; ++i) {
        workers_.emplace_back([this] { worker_loop(); });
    }
}

ThreadPool::~ThreadPool() {
    stop();
}

bool ThreadPool::submit(std::function<void()> task) {
    {
        std::lock_guard<std::mutex> lock(mu_);
        if (stopped_ || tasks_.size() >= max_queue_) return false;
        tasks_.push(std::move(task));
    }
    cv_.notify_one();
    return true;
}

void ThreadPool::stop() {
    {
        std::lock_guard<std::mutex> lock(mu_);
        if (stopped_) return;
        stopped_ = true;
    }
    cv_.notify_all();
    for (auto& t : workers_) {
        if (t.joinable()) t.join();
    }
}

std::size_t ThreadPool::queued() const {
    std::lock_guard<std::mutex> lock(mu_);
    return tasks_.size();
}

void ThreadPool::worker_loop() {
    for (;;) {
        std::function<void()> task;
        {
            std::unique_lock<std::mutex> lock(mu_);
            cv_.wait(lock, [this] { return stopped_ || !tasks_.empty(); });
            if (stopped_ && tasks_.empty()) return;
            task = std::move(tasks_.front());
            tasks_.pop();
        }
        task();
    }
}

}  // namespace chat

