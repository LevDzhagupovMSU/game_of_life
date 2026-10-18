#include "server/thread_pool.hpp"
#include "server/scheduler.hpp"
#include "server/worker.hpp"

#include <mutex>
#include <optional>

server::ThreadPool::ThreadPool(size_t thread_count, Worker& worker) : worker_(worker){
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

        if (stopping_)
            throw std::runtime_error("ThreadPool is stopping");

        jobs_.push(std::move(job));
    }

    ready_cv_.notify_one();
}

void server::ThreadPool::Run(){
    for(;;){
        std::optional<Job> job;
        {
            std::unique_lock<std::mutex> lock(mutex_);

            ready_cv_.wait(lock, [this]{
                return !jobs_.empty() || stopping_;
            });

            if(stopping_ && jobs_.empty())
                return;

            job.emplace(std::move(jobs_.front()));
            jobs_.pop();
        }

        try{
            worker_.Work(std::move(job.value()));
        }catch(...) {
            job->on_callback();
        }
    }
}