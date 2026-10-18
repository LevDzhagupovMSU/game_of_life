#include "server/scheduler.hpp"
#include "server/thread_pool.hpp"

#include <cstdint>
#include <mutex>
#include <optional>
#include <thread>
#include <utility>

server::Task::Task(uint16_t session_id_, std::string payload_, ResultHandler on_result_) : 
                    session_id(session_id_), payload(std::move(payload_)), on_result(std::move(on_result_)) {};

server::Scheduler::Scheduler(server::ThreadPool& thread_pool) : 
                    thread_pool_(thread_pool), th_([this]{ DispatchLoop(); }) {};

void server::Scheduler::Submit(Task task){
    {
        std::lock_guard<std::mutex> lock(mutex_);
        uint16_t id = task.session_id;
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
        uint16_t session_id;
        std::optional<Task> task; // optional только потому что нет конструктора по умолчанию

        {
            std::unique_lock<std::mutex> lock(mutex_);

            ready_cv_.wait(lock, [this](){
                return stopping_ || !ready_sessions_.empty();
            });

            if(stopping_ && pending_tasks_ == 0)
                return;

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

        thread_pool_.Post({std::move(task.value()), [this, session_id](){
            release(session_id);
        }});
    }
}

void server::Scheduler::release(uint16_t session_id){
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