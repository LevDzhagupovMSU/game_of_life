#pragma once

#include "server/message.hpp"
#include "server/scheduler.hpp"

#include <boost/asio.hpp>
#include <boost/beast.hpp>
#include <cstdint>
#include <functional>
#include <memory>
#include <nlohmann/json.hpp>

namespace server {

enum class SessionState{                    // Connectng - состояние до и во время HandShake,
    Connecting, Running, Disconnecting      // Running - состояние после HandShake 
};

class Session : public std::enable_shared_from_this<Session> {
    boost::beast::websocket::stream<boost::beast::tcp_stream> web_socket_;
    boost::asio::strand<boost::asio::io_context::executor_type> strand_; 
    boost::asio::streambuf buffer_;

    Scheduler& scheduler_;
    SessionState state_ = SessionState::Connecting;
    uint16_t id_;

    void Read();
    void Send(WorkerResult worker_result);

    std::string Message();
public:
    using HandShakeCallBack = std::function<void(const boost::beast::error_code& ec)>;

    Session(boost::asio::io_context& io, 
            boost::asio::ip::tcp::socket socket, 
            Scheduler& scheduler, uint16_t id);

    const uint16_t get_id() const;
    const SessionState get_state() const;
    void set_state(server::SessionState new_state);

    void DoHandShake(const HandShakeCallBack& HSCB);

    void Start();
    void Stop();
};

}
