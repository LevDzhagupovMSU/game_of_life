#pragma once

#include <boost/asio/io_context.hpp>
#include <boost/asio/ip/tcp.hpp>
#include <condition_variable>
#include <memory>
#include <unordered_map>
#include <cstdint>
#include <mutex>
#include <vector>

#include "server/generator_id.hpp"
#include "server/scheduler.hpp"
#include "server/session.hpp"

namespace server {

class Session;

class SessionManager{
    std::unordered_map<SessionId, std::shared_ptr<Session>> sessions_;
    IDGenerator generator_id_;
    std::condition_variable cv_;
    mutable std::mutex mutex_;
    bool stopping_ = false;

public:
    void CreateSession(boost::asio::io_context& io, 
                       boost::asio::ip::tcp::socket socket,
                       Scheduler& scheduler);
    void DeleteSession(SessionId session_id, server::SessionState state);

    void WaitForAllSessionsToStop();
    std::vector<std::shared_ptr<server::Session>> StopAll();
};

}
