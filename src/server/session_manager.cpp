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
    uint16_t session_id = generator_id_.next();
    std::shared_ptr<Session> session = std::make_shared<Session>(io, 
        std::move(socket), scheduler, session_id);

    session->DoHandShake([this, session](const boost::beast::error_code& ec){
        if(ec){
            std::cerr << "Handshake error session: " << session->get_id() << std::endl;
            std::cerr << "Error code: " << ec.what() << std::endl;
            
            return;
        }
        std::cout << "New session was successfully created." << std::endl;
        std::lock_guard<std::mutex> lg(mutex_);
        sessions_.insert({session->get_id(), session});
        session->Start();
    });
}


void server::SessionManager::DeleteSession(uint16_t session_id, server::SessionState state){
    if(state != server::SessionState::Disconnecting){
        std::cerr << "Session Manager: DeleteSession was called with wrong state" << std::endl;

        return;
    }   

    std::lock_guard<std::mutex> lg(mutex_);
    auto it_session = sessions_.find(session_id);
    it_session->second->set_state(state);
    if(it_session == sessions_.end()){
        std::cerr << "Session Manager: no found this session: " << session_id << std::endl;

        return;
    }
    sessions_.erase(it_session);
}

std::vector<std::shared_ptr<server::Session>> server::SessionManager::GetRunningSession() const{
    std::vector<std::shared_ptr<Session>> result;
    result.reserve(sessions_.size() / 2);
    std::lock_guard<std::mutex> lg(mutex_);
    for(const auto& pair : sessions_){
        if(pair.second->get_state() == server::SessionState::Running)
            result.push_back(pair.second);
    }

    return result;
}