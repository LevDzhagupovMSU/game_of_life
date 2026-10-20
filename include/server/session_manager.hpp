#pragma once

#include <boost/asio/io_context.hpp>
#include <boost/asio/ip/tcp.hpp>
#include <condition_variable>
#include <memory>
#include <unordered_map>
#include <mutex>
#include <vector>

#include "server/generator_id.hpp"
#include "server/scheduler.hpp"
#include "server/session.hpp"
#include "logging/syslog_logger.hpp"

namespace server {

class Session;

class SessionManager{
    std::unordered_map<SessionId, std::shared_ptr<Session>> sessions_;
    IDGenerator generator_id_;
    std::condition_variable cv_;
    mutable std::mutex mutex_;
    bool stopping_ = false;
    logging::SyslogLogger& logger_;
    Scheduler& scheduler_;

public:
    SessionManager(Scheduler& scheduler, logging::SyslogLogger& logger);
    void CreateSession(boost::asio::io_context& io, 
                       boost::asio::ip::tcp::socket socket);
    void DeleteSession(SessionId session_id, server::SessionState state);

    void WaitForAllSessionsToStop();
    std::vector<std::shared_ptr<server::Session>> StopAll();
};

}
