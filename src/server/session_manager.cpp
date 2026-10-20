#include <boost/beast/core/error.hpp>
#include <iostream>
#include <memory>
#include <mutex>
#include <vector>

#include "server/session_manager.hpp"
#include "server/session.hpp"

server::SessionManager::SessionManager(server::Scheduler& scheduler, logging::SyslogLogger& logger) : scheduler_(scheduler), logger_(logger) {}

void server::SessionManager::CreateSession(boost::asio::io_context& io, 
                                          boost::asio::ip::tcp::socket socket){
    std::shared_ptr<Session> session;
    {
        std::lock_guard<std::mutex> lg(mutex_);
        if(stopping_)
            return;

        SessionId session_id = generator_id_.next();
        session = std::make_shared<Session>(io, std::move(socket), scheduler_, session_id,
            [this](SessionId id){
                DeleteSession(id, server::SessionState::Disconnecting);
            }, logger_);
        if(!sessions_.emplace(session_id, session).second)
            throw std::runtime_error("Session ID collision");
    }

    session->DoHandShake([this, session](const boost::beast::error_code& ec){
        if(ec){
            logger_.error("Handshake error session: " + std::to_string(session->get_id()) + " error: " + ec.what());
            logger_.error("Error code: " + ec.what());
            session->Stop();
            return;
        }
        session->Start();
    });
}


void server::SessionManager::DeleteSession(SessionId session_id, server::SessionState state){
    if(state != server::SessionState::Disconnecting){
        logger_.error("Session Manager: session " + std::to_string(session_id) + " is not in Disconnecting state, cannot delete it.");
        return;
    }   

    std::lock_guard<std::mutex> lg(mutex_);
    auto it_session = sessions_.find(session_id);
    if(it_session == sessions_.end()){
        logger_.warning("Session Manager: no found this session: " + std::to_string(session_id));
        return;
    }
    sessions_.erase(it_session);
    cv_.notify_all();
}

std::vector<std::shared_ptr<server::Session>> server::SessionManager::StopAll(){
    std::vector<std::shared_ptr<Session>> result;
    std::lock_guard<std::mutex> lg(mutex_);
    stopping_ = true;
    result.reserve(sessions_.size());
    for(const auto& pair : sessions_)
        result.push_back(pair.second);

    return result;
}

void server::SessionManager::WaitForAllSessionsToStop(){
    std::unique_lock<std::mutex> lock(mutex_);
    cv_.wait(lock, [this](){
        return sessions_.empty();
    });
}
