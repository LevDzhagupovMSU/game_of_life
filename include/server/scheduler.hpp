#pragma once

#include "server/message.hpp"

#include <cstdint>
#include <functional>
#include <mutex>
#include <queue>
#include <thread>
#include <string>
#include <condition_variable>
#include <unordered_map>

namespace server{

class ThreadPool;

struct Task{
    using ResultHandler = std::function<void(WorkerResult)>;

    uint16_t session_id;
    std::string payload; 
    ResultHandler on_result;

    Task(uint16_t session_id_, std::string payload_, ResultHandler on_result_);
};

class Scheduler{

    struct SessionQueue{
        std::queue<Task> tasks;
        bool active = false;
        bool in_ready_queue = false;
    };

    std::unordered_map<uint16_t, SessionQueue> queues_;
    std::queue<uint16_t> ready_sessions_;
    std::mutex mutex_;
    std::condition_variable ready_cv_;
    std::thread th_;
    bool stopping_ = false;
    size_t pending_tasks_ = 0;

    ThreadPool& thread_pool_;

    void release(uint16_t session_id);
    void DispatchLoop();
public:
    Scheduler(ThreadPool& thread_pool);

    void Submit(Task task);
    void Stop();
};

}