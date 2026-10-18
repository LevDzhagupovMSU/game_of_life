cmake_minimum_required(VERSION 3.16)
project(game_of_life LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

find_package(Boost REQUIRED COMPONENTS system)
find_package(Threads REQUIRED)
find_package(nlohmann_json REQUIRED)

add_executable(game_of_life
    src/main.cpp
    src/game/engine.cpp
    src/game/grid.cpp
    src/game/rules.cpp
    src/server/server.cpp
    src/server/session.cpp
    src/server/session_manager.cpp
    src/server/scheduler.cpp
    src/server/thread_pool.cpp
    src/server/worker.cpp
    include/cache/LRUCache.cpp
    src/server/parse.cpp
    src/server/generator_id.cpp
)

target_include_directories(game_of_life PRIVATE include)
target_link_libraries(game_of_life PRIVATE
    Boost::system
    Threads::Threads
    nlohmann_json::nlohmann_json
)

enable_testing()

add_executable(game_of_life_tests
    tests/game_tests.cpp
    src/game/engine.cpp
    src/game/grid.cpp
    src/game/rules.cpp
    src/server/parse.cpp
)

target_include_directories(game_of_life_tests PRIVATE include)
target_link_libraries(game_of_life_tests PRIVATE nlohmann_json::nlohmann_json)
add_test(NAME game_of_life_tests COMMAND game_of_life_tests)

add_executable(lru_cache_tests
    tests/cache_tests.cpp
    include/cache/LRUCache.cpp
    src/game/engine.cpp
    src/game/grid.cpp
    src/game/rules.cpp
)
target_include_directories(lru_cache_tests PRIVATE include)
add_test(NAME lru_cache_tests COMMAND lru_cache_tests)

option(GAME_OF_LIFE_ENABLE_TSAN "Build LRU cache tests with ThreadSanitizer" OFF)
if(GAME_OF_LIFE_ENABLE_TSAN AND CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    target_compile_options(lru_cache_tests PRIVATE -fsanitize=thread -g -O1)
    target_link_options(lru_cache_tests PRIVATE -fsanitize=thread)
endif()
