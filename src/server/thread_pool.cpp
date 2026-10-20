#include "server/thread_pool.hpp"
#include "server/scheduler.hpp"
#include "server/worker.hpp"

#include <functional>
#include <mutex>
#include <optional>

server::ThreadPool::ThreadPool(size_t thread_count, Worker& worker, logging::SyslogLogger& logger) : worker_(worker), logger_(logger){
    if(thread_count == 0){
        logger_.error("Thread count must be greater than zero");
        throw std::invalid_argument("Thread count must be greater than zero");
    }

    for(int i = 0; i < thread_count; ++i){
        threads_.emplace_back([this]{
            Run();
        });
    }
}

server::ThreadPool::~ThreadPool(){
    Stop();
}

void server::ThreadPool::Stop(){
    {
        std::lock_guard lock(mutex_);
        stopping_ = true;
    }

    ready_cv_.notify_all();

    for (auto& thread : threads_) {
        if (thread.joinable())
            thread.join();
    }
}


void server::ThreadPool::Post(Job job) {
    {
        std::lock_guard lock(mutex_);

        if (stopping_){
            logger_.warning("ThreadPool is stopping");
            throw std::runtime_error("ThreadPool is stopping");
        }

        jobs_.push(std::move(job));
    }

    ready_cv_.notify_one();
}

void server::ThreadPool::Run(){
    for(;;){
        std::optional<Job> job;
        std::function<void()> job_callback;
        Task::ResultHandler on_result;
        {
            std::unique_lock<std::mutex> lock(mutex_);

            ready_cv_.wait(lock, [this]{
                return !jobs_.empty() || stopping_;
            });

            if(stopping_ && jobs_.empty())
                return;

            job.emplace(std::move(jobs_.front()));
            job_callback = job->on_callback;
            on_result = job->task.on_result;
            jobs_.pop();
        }

        try{
            worker_.Work(std::move(job.value()));
        }catch(const std::exception& e) {
            try { on_result(std::string(e.what())); } catch(...) {}
            job_callback();
        }catch(...) {
            try { on_result(std::string("Task processing failed")); } catch(...) {}
            job_callback();
        }
    }
}
