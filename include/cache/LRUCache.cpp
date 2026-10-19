#include "cache/LRUCache.hpp"

#include <cstddef>
#include <cstdint>
#include <memory>
#include <iterator>


cache::LRUCache::LRUCache(size_t capacity) : capacity_(capacity) {};

size_t cache::LRUCache::size() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return order_.size();
}

void cache::LRUCache::Put(std::uint64_t key, std::shared_ptr<game::GameEngine> engine){
    std::lock_guard<std::mutex> lock(mutex_);
    if(capacity_ == 0)
        return;

    auto it = cache_.find(key);   
    if(it != cache_.end()){
        it->second->second = std::move(engine);
        order_.splice(order_.begin(), order_, it->second);

        return;
    }

    if(order_.size() >= capacity_){
        auto it_order = std::prev(order_.end());
        cache_.erase(it_order->first);
        order_.pop_back();
    }

    order_.push_front({key, std::move(engine)});
    cache_.insert({key, order_.begin()});
}

std::weak_ptr<game::GameEngine> cache::LRUCache::Get(std::uint64_t key){
    std::lock_guard<std::mutex> lock(mutex_);
    auto it = cache_.find(key);
    if(it != cache_.end()){
        order_.splice(order_.begin(), order_, it->second);
        return it->second->second;
    }   

    return {};
}
