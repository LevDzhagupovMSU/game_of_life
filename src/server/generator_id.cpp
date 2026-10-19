#include "server/generator_id.hpp"
#include <cstdint>

std::uint64_t server::IDGenerator::next(){
    return counter.fetch_add(1, std::memory_order_relaxed);
}
