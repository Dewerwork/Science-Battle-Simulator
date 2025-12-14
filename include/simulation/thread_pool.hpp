#pragma once

#include <thread>
#include <vector>
#include <queue>
#include <mutex>
#include <condition_variable>
#include <functional>
#include <future>
#include <atomic>

namespace battle {

// ==============================================================================
// High-Performance Thread Pool
// Optimized for batch processing of simulation tasks
// ==============================================================================

class ThreadPool {
public:
    explicit ThreadPool(size_t num_threads = 0) {
        if (num_threads == 0) {
            num_threads = std::thread::hardware_concurrency();
            if (num_threads == 0) num_threads = 4; // Fallback
        }

        workers_.reserve(num_threads);
        for (size_t i = 0; i < num_threads; ++i) {
            workers_.emplace_back([this] { worker_loop(); });
        }
    }

    ~ThreadPool() {
        {
            std::unique_lock<std::mutex> lock(mutex_);
            stop_ = true;
        }
        condition_.notify_all();
        for (auto& worker : workers_) {
            if (worker.joinable()) {
                worker.join();
            }
        }
    }

    // Non-copyable
    ThreadPool(const ThreadPool&) = delete;
    ThreadPool& operator=(const ThreadPool&) = delete;

    // Submit a task and get a future
    template<typename F, typename... Args>
    auto submit(F&& f, Args&&... args) -> std::future<std::invoke_result_t<F, Args...>> {
        using return_type = std::invoke_result_t<F, Args...>;

        auto task = std::make_shared<std::packaged_task<return_type()>>(
            std::bind(std::forward<F>(f), std::forward<Args>(args)...)
        );

        std::future<return_type> result = task->get_future();

        {
            std::unique_lock<std::mutex> lock(mutex_);
            if (stop_) {
                throw std::runtime_error("ThreadPool is stopped");
            }
            tasks_.emplace([task]() { (*task)(); });
        }

        condition_.notify_one();
        return result;
    }

    // Submit without waiting for result (fire-and-forget)
    template<typename F>
    void submit_detached(F&& f) {
        {
            std::unique_lock<std::mutex> lock(mutex_);
            if (stop_) return;
            tasks_.emplace(std::forward<F>(f));
        }
        condition_.notify_one();
    }

    // Wait for all tasks to complete
    void wait_all() {
        std::unique_lock<std::mutex> lock(mutex_);
        finished_condition_.wait(lock, [this] {
            return tasks_.empty() && active_tasks_ == 0;
        });
    }

    size_t thread_count() const { return workers_.size(); }
    size_t pending_tasks() const { return tasks_.size(); }
    size_t active_tasks() const { return active_tasks_.load(); }

private:
    std::vector<std::thread> workers_;
    std::queue<std::function<void()>> tasks_;
    std::mutex mutex_;
    std::condition_variable condition_;
    std::condition_variable finished_condition_;
    std::atomic<size_t> active_tasks_{0};
    bool stop_ = false;

    void worker_loop() {
        while (true) {
            std::function<void()> task;

            {
                std::unique_lock<std::mutex> lock(mutex_);
                condition_.wait(lock, [this] {
                    return stop_ || !tasks_.empty();
                });

                if (stop_ && tasks_.empty()) return;

                task = std::move(tasks_.front());
                tasks_.pop();
                ++active_tasks_;
            }

            task();

            {
                std::unique_lock<std::mutex> lock(mutex_);
                --active_tasks_;
            }
            finished_condition_.notify_all();
        }
    }
};

// ==============================================================================
// Global Thread Pool Access
// ==============================================================================

inline ThreadPool& get_thread_pool() {
    static ThreadPool pool;
    return pool;
}

} // namespace battle
