#pragma once

#include <cstddef>
#include <list>
#include <unordered_map>
#include <cstdint>
#include <utility>
#include <memory>
#include <mutex>

#include "game/engine.hpp"

namespace cache {

class LRUCache{
    size_t capacity_;
    mutable std::mutex mutex_;
    std::unordered_map<uint16_t, 
            std::list<std::pair<uint16_t, std::shared_ptr<game::GameEngine>>>::iterator> cache_;
    std::list<std::pair<uint16_t, std::shared_ptr<game::GameEngine>>> order_;
public:
    LRUCache(size_t capacity);

    size_t size() const;
    void Put(uint16_t key, std::shared_ptr<game::GameEngine> engine);
    std::weak_ptr<game::GameEngine> Get(uint16_t key);
};

}
