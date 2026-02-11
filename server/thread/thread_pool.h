#pragma once
#include <vector>
#include <thread>
#include <queue>
#include <functional>
#include <mutex>
#include <condition_variable>
#include <future>
#include <atomic>
#include <stdexcept>

class ThreadPool {
public:
    explicit ThreadPool(size_t numThreads = std::thread::hardware_concurrency());

    template<class F>
    auto submit(F&& f) -> std::future<decltype(f())> {
        auto task = std::make_shared<std::packaged_task<decltype(f())()>>(std::forward<F>(f));
        std::future<decltype(f())> res = task->get_future();
        {
            std::lock_guard<std::mutex> lock(queueMutex);
            if (stop) throw std::runtime_error("submit on stopped ThreadPool");
            tasks.emplace([task]() { (*task)(); });
        }
        condition.notify_one();
        return res;
    }

    ~ThreadPool();

private:
    std::vector<std::thread> workers;
    std::queue<std::function<void()>> tasks;
    std::mutex queueMutex;
    std::condition_variable condition;
    std::atomic<bool> stop{false};
};

class SingletonThreadPool {
public:
    static SingletonThreadPool& getInstance();

    template<class F>
    auto submit(F&& f) -> std::future<decltype(f())> {
        return pool.submit(std::forward<F>(f));
    }

private:
    SingletonThreadPool();
    ThreadPool pool;
};
