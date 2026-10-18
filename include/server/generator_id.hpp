#pragma once

#include <atomic>
#include <cstdint>

namespace server {

class IDGenerator{
    std::atomic<uint16_t> counter{0};
    
    static uint16_t mix(uint16_t x);
public:
    uint16_t next();
};

}