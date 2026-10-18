#pragma once

#include "server/session_manager.hpp"
#include "server/scheduler.hpp"
#include "server/thread_pool.hpp"
#include "server/worker.hpp"

#include <boost/asio.hpp>
#include <cstddef>
#include <vector>
#include <thread>
#include <atomic>
#include <cstdint>

namespace server{

class Server{
    std::atomic<bool> started_ = false;
    boost::asio::io_context& io_;
    boost::asio::ip::tcp::acceptor acceptor_;
    std::string address_;
    uint16_t port_;

    SessionManager session_manager_;
    Worker worker_;
    ThreadPool thread_pool;
    Scheduler scheduler_;

    std::vector<std::thread> threads_;
    size_t num_threads_;

    void StartAccept();
public:
    Server(boost::asio::io_context& io, 
           const std::string& address,
           uint16_t port = 5050,
           size_t num_threads = 4,
           size_t pool_size = 2,
           size_t cache_capacity = 1000);

    ~Server();

    Server(const Server&) = delete;
    Server& operator=(const Server&) = delete;

    Server(Server&&);            // ради интереса
    Server& operator=(Server&&); // ради интереса

    void Start();
    void Stop();
};

}