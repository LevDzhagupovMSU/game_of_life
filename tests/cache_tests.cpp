#include "cache/LRUCache.hpp"
#include "game/engine.hpp"
#include "game/grid.hpp"

#include <atomic>
#include <cstddef>
#include <cstdint>
#include <exception>
#include <iostream>
#include <memory>
#include <stdexcept>
#include <thread>
#include <vector>

namespace {

void require(bool condition, const char* message) {
    if (!condition) throw std::runtime_error(message);
}

std::shared_ptr<game::GameEngine> makeEngine() {
    return std::make_shared<game::GameEngine>(game::Grid(3, 3));
}

void testCapacityEvictsLeastRecentlyUsed() {
    cache::LRUCache cache(2);
    auto first = makeEngine();
    auto second = makeEngine();
    auto third = makeEngine();
    std::weak_ptr<game::GameEngine> first_weak = first;
    std::weak_ptr<game::GameEngine> second_weak = second;

    cache.Put(1, first);
    cache.Put(2, second);
    require(cache.Get(1).lock() == first, "Get must return the stored value");
    second.reset();
    cache.Put(3, third);

    require(cache.size() == 2, "Cache size must not exceed capacity");
    require(!second_weak.lock(), "Least recently used entry must be evicted");
    require(cache.Get(1).lock() == first, "Recently accessed entry must remain cached");
    require(cache.Get(3).lock() == third, "New entry must be cached");
    require(!first_weak.expired(), "Cache and caller must retain live entries");
}

void testUpdatingKeyDoesNotIncreaseSize() {
    cache::LRUCache cache(2);
    auto original = makeEngine();
    auto replacement = makeEngine();
    cache.Put(7, original);
    cache.Put(7, replacement);
    require(cache.size() == 1, "Updating a key must not add a duplicate");
    require(cache.Get(7).lock() == replacement, "Updated key must return replacement value");
}

void testZeroCapacityStoresNothing() {
    cache::LRUCache cache(0);
    auto engine = makeEngine();
    cache.Put(1, engine);
    require(cache.size() == 0, "Zero-capacity cache must remain empty");
    require(cache.Get(1).expired(), "Zero-capacity cache must not return inserted values");
}

void testConcurrentGetPutAndSize() {
    constexpr std::size_t capacity = 32;
    constexpr std::size_t thread_count = 8;
    constexpr std::size_t iterations = 10000;
    cache::LRUCache cache(capacity);
    std::vector<std::shared_ptr<game::GameEngine>> engines;
    engines.reserve(64);
    for (std::size_t i = 0; i < 64; ++i) engines.push_back(makeEngine());

    std::atomic<bool> invalid_size{false};
    std::vector<std::thread> threads;
    threads.reserve(thread_count);
    for (std::size_t t = 0; t < thread_count; ++t) {
        threads.emplace_back([&, t] {
            for (std::size_t i = 0; i < iterations; ++i) {
                const auto key = static_cast<std::uint16_t>((i * 17 + t * 13) % engines.size());
                cache.Put(key, engines[key]);
                (void)cache.Get(static_cast<std::uint16_t>((key + 7) % engines.size()));
                if (cache.size() > capacity) invalid_size.store(true, std::memory_order_relaxed);
            }
        });
    }
    for (auto& thread : threads) thread.join();

    require(!invalid_size.load(std::memory_order_relaxed), "Concurrent operations must preserve the capacity invariant");
    require(cache.size() <= capacity, "Concurrent operations must not exceed capacity");
}

}

int main() {
    try {
        testCapacityEvictsLeastRecentlyUsed();
        testUpdatingKeyDoesNotIncreaseSize();
        testZeroCapacityStoresNothing();
        testConcurrentGetPutAndSize();
    } catch (const std::exception& error) {
        std::cerr << "Cache test failed: " << error.what() << "\\n";
        return 1;
    }
    std::cout << "All cache tests passed\\n";
    return 0;
}
