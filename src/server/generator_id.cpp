#include "server/generator_id.hpp"
#include <cstdint>

uint16_t server::IDGenerator::next(){
    return mix(counter.fetch_add(1));
}

uint16_t server::IDGenerator::mix(uint16_t x){
    x += 0x9E3779B97F4A7C15ULL;
    x = (x ^ (x >> 30)) * 0xBF58476D1CE4E5B9ULL;
    x = (x ^ (x >> 27)) * 0x94D049BB133111EBULL;
    return x ^ (x >> 31);
}