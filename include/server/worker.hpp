#pragma once

#include "cache/LRUCache.hpp"
#include "server/thread_pool.hpp"

#include <cstddef>
#include <mutex>

namespace server {

class Worker{
    cache::LRUCache cache_;
    std::mutex mutex_;
public:
    explicit Worker(size_t cache_capacity);

    void Work(ThreadPool::Job job);
};

}