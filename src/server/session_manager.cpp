#include <boost/beast/core/error.hpp>
#include <cstdint>
#include <iostream>
#include <memory>
#include <mutex>
#include <vector>

#include "server/session_manager.hpp"
#include "server/session.hpp"

void server::SessionManager::CreateSession(boost::asio::io_context& io, 
                                          boost::asio::ip::tcp::socket socket,
                                          Scheduler& scheduler){
    std::shared_ptr<Session> session;
    {
        std::lock_guard<std::mutex> lg(mutex_);
        if(stopping_)
            return;

        SessionId session_id = generator_id_.next();
        session = std::make_shared<Session>(io, std::move(socket), scheduler, session_id,
            [this](SessionId id){
                DeleteSession(id, server::SessionState::Disconnecting);
            });
        if(!sessions_.emplace(session_id, session).second)
            throw std::runtime_error("Session ID collision");
    }

    session->DoHandShake([this, session](const boost::beast::error_code& ec){
        if(ec){
            std::cerr << "Handshake error session: " << session->get_id() << std::endl;
            std::cerr << "Error code: " << ec.what() << std::endl;
            session->Stop();
            return;
        }
        std::cout << "New session was successfully created." << std::endl;
        session->Start();
    });
}


void server::SessionManager::DeleteSession(SessionId session_id, server::SessionState state){
    if(state != server::SessionState::Disconnecting){
        std::cerr << "Session Manager: DeleteSession was called with wrong state" << std::endl;

        return;
    }   

    std::lock_guard<std::mutex> lg(mutex_);
    auto it_session = sessions_.find(session_id);
    if(it_session == sessions_.end()){
        std::cerr << "Session Manager: no found this session: " << session_id << std::endl;

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
