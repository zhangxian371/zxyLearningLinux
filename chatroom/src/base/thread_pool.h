#pragma once

#include <condition_variable>
#include <cstddef>
#include <functional>
#include <mutex>
#include <queue>
#include <thread>
#include <vector>

namespace chat {

class ThreadPool {
public:
    ThreadPool(std::size_t threads, std::size_t max_queue);
    ~ThreadPool();

    ThreadPool(const ThreadPool&) = delete;
    ThreadPool& operator=(const ThreadPool&) = delete;

    bool submit(std::function<void()> task);
    void stop();
    std::size_t queued() const;

private:
    void worker_loop();

    mutable std::mutex mu_;
    std::condition_variable cv_;
    std::vector<std::thread> workers_;
    std::queue<std::function<void()>> tasks_;
    std::size_t max_queue_;
    bool stopped_{false};
};

}  // namespace chat

