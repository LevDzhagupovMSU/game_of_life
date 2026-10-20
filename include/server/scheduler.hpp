#pragma once

#include "server/message.hpp"
#include "logging/syslog_logger.hpp"

#include <functional>
#include <mutex>
#include <queue>
#include <thread>
#include <string>
#include <condition_variable>
#include <unordered_map>
#include <variant>

namespace server{

class ThreadPool;

struct Task{
    using ResultHandler = std::function<void(std::variant<WorkerResult, std::string>)>;

    SessionId session_id;
    std::string payload; 
    ResultHandler on_result;

    Task(SessionId session_id_, std::string payload_, ResultHandler on_result_);
};

class Scheduler{

    struct SessionQueue{
        std::queue<Task> tasks;
        bool active = false;
        bool in_ready_queue = false;
    };

    std::unordered_map<SessionId, SessionQueue> queues_;
    std::queue<SessionId> ready_sessions_;
    std::mutex mutex_;
    std::condition_variable ready_cv_;
    std::thread th_;

    bool stopping_ = false;
    size_t pending_tasks_ = 0;

    ThreadPool& thread_pool_;
    logging::SyslogLogger& logger_;
    void release(SessionId session_id);
    void DispatchLoop();
public:
    Scheduler(ThreadPool& thread_pool, logging::SyslogLogger& logger);
    ~Scheduler();

    void Submit(Task task);
    void Stop();
    size_t GetPendingTasks();
};

}
