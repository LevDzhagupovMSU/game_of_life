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
                       size_t cache_capacity) : io_(io), address_(address), acceptor_(io, 
                                    boost::asio::ip::tcp::endpoint(
                                        boost::asio::ip::make_address(address),
                                    port)), port_(port), num_threads_(num_threads), 
                                    thread_pool(pool_size, worker_), scheduler_(thread_pool), worker_(cache_capacity) {};

server::Server::~Server() { Stop(); }

void server::Server::Start() {
    bool expected = false;
    if(!started_.compare_exchange_strong(expected, true)){
        std::cout << "Server already started" << std::endl;
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
    bool expected = true;
    if(!started_.compare_exchange_strong(expected, false)){
        std::cout << "Server already stoped" << std::endl;
        return;
    }

    boost::beast::error_code ec;
    acceptor_.close(ec);

    auto sessions = session_manager_.GetRunningSession();
    for(auto& session : sessions){
        session->Stop();
    }

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
        session_manager_.CreateSession(io_, std::move(socket), scheduler_);
        StartAccept();   
    });
}