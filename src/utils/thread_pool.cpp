#include "utils/thread_pool.hpp"
#include <iostream>

namespace bot {

ThreadPool::ThreadPool(size_t num_threads) : stop_(false) {
    for (size_t i = 0; i < num_threads; ++i) {
        workers_.emplace_back([this] {
            while (true) {
                std::function<void()> task;
                {
                    std::unique_lock<std::mutex> lock(queue_mutex_);
                    condition_.wait(lock, [this] {
                        return stop_ || !tasks_.empty();
                    });
                    if (stop_ && tasks_.empty()) {
                        return;
                    }
                    task = std::move(tasks_.front());
                    tasks_.pop();
                }
                try {
                    task();
                } catch (const std::exception& e) {
                    std::cerr << "ThreadPool task exception: " << e.what() << std::endl;
                } catch (...) {
                    std::cerr << "ThreadPool task unknown exception" << std::endl;
                }
            }
        });
    }
}

ThreadPool::~ThreadPool() {
    shutdown();
}

void ThreadPool::enqueue(std::function<void()> task) {
    {
        std::unique_lock<std::mutex> lock(queue_mutex_);
        if (stop_) {
            throw std::runtime_error("ThreadPool is stopped");
        }
        tasks_.emplace(std::move(task));
    }
    condition_.notify_one();
}

size_t ThreadPool::pending() const {
    std::unique_lock<std::mutex> lock(queue_mutex_);
    return tasks_.size();
}

void ThreadPool::shutdown() {
    {
        std::unique_lock<std::mutex> lock(queue_mutex_);
        if (stop_) return;
        stop_ = true;
    }
    condition_.notify_all();
    for (std::thread& worker : workers_) {
        if (worker.joinable()) {
            worker.join();
        }
    }
}

// Global thread pool instance
static std::unique_ptr<ThreadPool> g_thread_pool;
static std::once_flag g_thread_pool_init;

ThreadPool& get_thread_pool() {
    std::call_once(g_thread_pool_init, []() {
        g_thread_pool = std::make_unique<ThreadPool>(4);
    });
    return *g_thread_pool;
}

} // namespace bot
