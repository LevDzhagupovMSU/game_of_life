#include "server/server.hpp"
#include "server/scheduler.hpp"
#include "server/thread_pool.hpp"
#include "server/worker.hpp"

#include <boost/asio/error.hpp>
#include <boost/asio/ip/address.hpp>
#include <boost/asio/ip/impl/network_v6.ipp>
#include <boost/asio/ip/tcp.hpp>
#include <boost/beast/core/error.hpp>
#include <boost/system/detail/error_code.hpp>
#include <iostream>

server::Server::Server(boost::asio::io_context& io, 
                       const std::string& address,
                       uint16_t port,
                       size_t num_threads,
                       size_t pool_size,
                       size_t cache_capacity) : io_(io), acceptor_(io,
                                    boost::asio::ip::tcp::endpoint(
                                        boost::asio::ip::make_address(address),
                                    port)), address_(address), port_(port), worker_(cache_capacity),
                                    thread_pool(pool_size, worker_), scheduler_(thread_pool),
                                    num_threads_(num_threads) {};

server::Server::~Server() { Stop(); }

void server::Server::Start() {
    State expected = State::Created;
    if(!state_.compare_exchange_strong(expected, State::Running)){
        std::cout << (expected == State::Running ? "Server already started" : "Server cannot be restarted") << std::endl;
        return;
    }

    StartAccept();

    for(int i = 0; i < num_threads_; ++i){
        threads_.emplace_back([this](){
            io_.run();
        });
    }
}

void server::Server::Stop() {
    State expected = State::Running;
    if(!state_.compare_exchange_strong(expected, State::Stopped)){
        return;
    }
    std::cout << "Stopping server..." << std::endl;

    boost::beast::error_code ec;
    std::cout << "Closing acceptor..." << std::endl;
    acceptor_.close(ec);

    std::cout << "Stopping all sessions..." << std::endl;
    auto sessions = session_manager_.StopAll();
    for(auto& session : sessions){
        session->Stop();
    }
    session_manager_.WaitForAllSessionsToStop();

    std::cout << "Stopping scheduler..." << std::endl;
    scheduler_.Stop();

    std::cout << "Stopping thread pool..." << std::endl;
    thread_pool.Stop();

    io_.stop();
    for(auto& thread : threads_){
        if (thread.joinable()) {
            thread.join();
        }
    }
}


void server::Server::StartAccept() {
    acceptor_.async_accept([this](const boost::system::error_code& ec,
                                        boost::asio::ip::tcp::socket socket){
        if(ec == boost::asio::error::operation_aborted)
            return;
        
        if(ec){
            std::cerr << "StartAccept: Accept error: " << ec.what() << std::endl;
            return;
        }

        if(state_.load() != State::Running)
            return;

        session_manager_.CreateSession(io_, std::move(socket), scheduler_);
        if(state_.load() == State::Running)
            StartAccept();
    });
}
