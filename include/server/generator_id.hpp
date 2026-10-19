#pragma once

#include <atomic>
#include <cstdint>

namespace server {

class IDGenerator{
    std::atomic<std::uint64_t> counter{1};
public:
    std::uint64_t next();
};

}
