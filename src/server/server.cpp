#include "server/server.hpp"
#include "server/scheduler.hpp"
#include "server/thread_pool.hpp"
#include "server/worker.hpp"

#include <boost/asio.hpp>

server::Server::Server(boost::asio::io_context& io, 
                       const std::string& address,
                       uint16_t port,
                       size_t num_threads,
                       size_t pool_size,
                       size_t cache_capacity) : io_(io), acceptor_(io,
                                    boost::asio::ip::tcp::endpoint(
                                        boost::asio::ip::make_address(address),
                                    port)), address_(address), port_(port), worker_(cache_capacity),
                                    thread_pool(pool_size, worker_, logger_), scheduler_(thread_pool, logger_),
                                    num_threads_(num_threads), logger_("game_of_life", LOG_DAEMON),
                                    session_manager_(scheduler_,logger_) {};

server::Server::~Server() { Stop(); }

void server::Server::Start() {
    State expected = State::Created;
    if(!state_.compare_exchange_strong(expected, State::Running)){
        logger_.warning("Server is already running or has been stopped.");
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
    logger_.info("Stopping server...");

    boost::beast::error_code ec;
    logger_.info("Closing acceptor...");
    acceptor_.close(ec);

    logger_.info("Stopping all sessions...");
    auto sessions = session_manager_.StopAll();
    for(auto& session : sessions){
        session->Stop();
    }
    session_manager_.WaitForAllSessionsToStop();

    logger_.info("Stopping scheduler...");
    scheduler_.Stop();

    logger_.info("Stopping thread pool...");
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
            logger_.error("StartAccept: Accept error: " + ec.what());
            return;
        }

        if(state_.load() != State::Running){
            logger_.warning("StartAccept: Server is not running.");
            return;
        }
        bool session_created = false;
        try{
            
            session_manager_.CreateSession(io_, std::move(socket));
            session_created = true;
        } catch(const std::exception& e){
            logger_.error("StartAccept: Failed to create session: " + std::string((e.what())));
        }

        if(session_created){
            logger_.info("StartAccept: New session created.");
        }
        if(state_.load() == State::Running)
            StartAccept();
    });
}
