#include "server/worker.hpp"
#include "game/engine.hpp"
#include "server/message.hpp"
#include "server/scheduler.hpp"

#include <cstdint>
#include <functional>
#include <memory>
#include <mutex>

server::Worker::Worker(size_t cache_capacity) : cache_(cache_capacity) {};

void server::Worker::Work(server::ThreadPool::Job job){
    SessionId session_id = job.task.session_id;
    std::weak_ptr<game::GameEngine> engine_ptr;

    {
        std::lock_guard<std::mutex> lock(mutex_);
        engine_ptr = cache_.Get(session_id);
    }
    
    auto engine_shared = engine_ptr.lock();
    if(engine_shared){
        engine_shared->step();
        WorkerResult result = Parse::from_grid(engine_shared->grid(), session_id);

        job.task.on_result(std::move(result));
        job.on_callback();
    }else{
        ClientMessage client_message = Parse::from_payload(job.task.payload);
        std::shared_ptr<game::GameEngine> engine_shared = std::make_shared<game::GameEngine>(client_message);
        engine_shared->step();
        WorkerResult result = Parse::from_grid(engine_shared->grid(), session_id);
        {
            std::lock_guard<std::mutex> lock(mutex_);
            cache_.Put(session_id, std::move(engine_shared));
        }

        job.task.on_result(std::move(result));
        job.on_callback();
    }
}
