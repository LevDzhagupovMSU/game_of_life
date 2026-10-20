#include "server/scheduler.hpp"
#include "server/thread_pool.hpp"

#include <condition_variable>
#include <mutex>
#include <optional>
#include <stdexcept>
#include <thread>
#include <utility>

server::Task::Task(SessionId session_id_, std::string payload_, ResultHandler on_result_) :
                    session_id(session_id_), payload(std::move(payload_)), on_result(std::move(on_result_)) {};

server::Scheduler::Scheduler(server::ThreadPool& thread_pool, logging::SyslogLogger& logger) : 
                    thread_pool_(thread_pool), logger_(logger), th_([this]{ DispatchLoop(); }) {};

server::Scheduler::~Scheduler(){
    Stop();
}

void server::Scheduler::Submit(Task task){
    {
        std::lock_guard<std::mutex> lock(mutex_);

        if(stopping_){
            logger_.error("Scheduler is stopping, cannot submit new tasks");
            throw std::runtime_error("Scheduler is stopping, cannot submit new tasks");
        }

        SessionId id = task.session_id;
        queues_[task.session_id].tasks.push(std::move(task));
    
        auto& q = queues_[task.session_id];
        if(!q.active && !q.in_ready_queue){
            q.in_ready_queue = true;
            ready_sessions_.push(id);
        }
        ++pending_tasks_;
    }
    ready_cv_.notify_one();
}

void server::Scheduler::DispatchLoop(){
    for(;;){
        SessionId session_id;
        std::optional<Task> task; // optional только потому что нет конструктора по умолчанию

        {
            std::unique_lock<std::mutex> lock(mutex_);

            ready_cv_.wait(lock, [this](){
                return (stopping_ && pending_tasks_ == 0) || !ready_sessions_.empty();
            });

            if(stopping_ && pending_tasks_ == 0){
                logger_.info("Scheduler stopping, no pending tasks left.");
                return;
            }

            session_id = ready_sessions_.front();
            ready_sessions_.pop();

            auto it = queues_.find(session_id);

            auto& session_queue = it->second;
            session_queue.in_ready_queue = false;
            if(session_queue.active || session_queue.tasks.empty())
                continue;

            session_queue.active = true;
            task.emplace(std::move(session_queue.tasks.front()));
            session_queue.tasks.pop();
        }

        auto on_result = task->on_result;
        try{
            thread_pool_.Post({std::move(task.value()), [this, session_id](){
                release(session_id);
            }});
        }catch(const std::exception& e){
            try { on_result(std::string(e.what())); } catch(...) {}
            release(session_id);
        }catch(...){
            try { on_result(std::string("Task dispatch failed")); } catch(...) {}
            release(session_id);
        }
    }
}

void server::Scheduler::release(SessionId session_id){
    {
        std::lock_guard<std::mutex> lock(mutex_);
        queues_[session_id].active = false;
        --pending_tasks_;
        if(!queues_[session_id].tasks.empty()){
            ready_sessions_.push(session_id);
            queues_[session_id].in_ready_queue = true;
        }
    }
    
    ready_cv_.notify_one();
}


void server::Scheduler::Stop(){
    {
        std::lock_guard<std::mutex> lock(mutex_);
        stopping_ = true;
    }

    ready_cv_.notify_all();

    if(th_.joinable())
        th_.join();
}

size_t server::Scheduler::GetPendingTasks(){
    std::lock_guard<std::mutex> lock(mutex_);
    return pending_tasks_;
}
