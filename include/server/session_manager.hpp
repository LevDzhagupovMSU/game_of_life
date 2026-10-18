#pragma once

#include <boost/asio/io_context.hpp>
#include <boost/asio/ip/tcp.hpp>
#include <unordered_map>
#include <cstdint>
#include <mutex>

#include "server/generator_id.hpp"
#include "server/scheduler.hpp"
#include "server/session.hpp"

namespace server {

class Session;

class SessionManager{
    std::unordered_map<uint16_t, std::shared_ptr<Session>> sessions_; // В sessions_ находяться сессии, которые по крайней мере произевли успешный HandShake
    IDGenerator generator_id_;
    mutable std::mutex mutex_;
public:
    void CreateSession(boost::asio::io_context& io, 
                       boost::asio::ip::tcp::socket socket,
                       Scheduler& scheduler);
    void DeleteSession(uint16_t session_id, server::SessionState state);

    std::vector<std::shared_ptr<server::Session>> GetRunningSession() const;
};

}
