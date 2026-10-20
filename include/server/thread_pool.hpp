#pragma once

#include "server/scheduler.hpp"
#include "logging/syslog_logger.hpp"

#include <cstddef>
#include <functional>
#include <queue>
#include <vector>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <utility>

namespace server {

class Worker;

class ThreadPool{
public:
    struct Job{
        using callback_for_scheduler = std::function<void()>;
        Task task;
        callback_for_scheduler on_callback;

        Job(Task task_, callback_for_scheduler on_callback_) : task(std::move(task_)), 
                                                               on_callback(std::move(on_callback_)) {};
    };
private:
    std::vector<std::thread> threads_;
    std::queue<Job> jobs_;
    std::mutex mutex_;
    std::condition_variable ready_cv_;
    Worker& worker_;
    logging::SyslogLogger& logger_;
    bool stopping_ = false;

    void Run();
public:
    explicit ThreadPool(size_t thread_count, Worker& worker, logging::SyslogLogger& logger);

    ThreadPool(const ThreadPool&) = delete;
    ThreadPool& operator=(const ThreadPool&) = delete;
    ThreadPool& operator=(ThreadPool&&) = delete;
    ThreadPool(ThreadPool&&) =delete;

    ~ThreadPool();

    void Post(Job job);
    void Stop();
};

}