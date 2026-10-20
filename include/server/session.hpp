#pragma once

#include "server/message.hpp"
#include "server/scheduler.hpp"
#include "logging/syslog_logger.hpp"

#include <boost/asio.hpp>
#include <boost/beast.hpp>
#include <atomic>
#include <deque>
#include <functional>
#include <memory>
#include <nlohmann/json.hpp>

namespace server {

enum class SessionState{                    // Connectng - состояние до и во время HandShake,
    Connecting, Running, Disconnecting      // Running - состояние после HandShake 
};

class Session : public std::enable_shared_from_this<Session> {
public:
    using StopCallback = std::function<void(SessionId id)>;
private:
    boost::beast::websocket::stream<boost::beast::tcp_stream> web_socket_;
    boost::asio::strand<boost::asio::io_context::executor_type> strand_; 
    boost::asio::steady_timer close_timer_;
    boost::asio::streambuf buffer_;

    Scheduler& scheduler_;
    logging::SyslogLogger& logger_;
    std::deque<std::string> message_queue_;
    bool write_in_progress_ = false;
    bool close_requested_ = false;
    bool close_in_progress_ = false;
    std::atomic<SessionState> state_{SessionState::Connecting};
    std::atomic<bool> finished_{false};
    StopCallback stop_callback_;
    SessionId id_;

    void Read();
    void Send(WorkerResult worker_result);
    void SendError(std::string error);
    void SendPayload(std::string payload);
    void SendNextMessage();
    void CloseWhenReady();
    void Finish();

    std::string Message();
public:
    using HandShakeCallBack = std::function<void(const boost::beast::error_code& ec)>;

    Session(boost::asio::io_context& io, 
            boost::asio::ip::tcp::socket socket, 
            Scheduler& scheduler, SessionId id,
            StopCallback stop_callback, logging::SyslogLogger& logger);

    ~Session() = default;

    SessionId get_id() const;
    const SessionState get_state() const;

    void DoHandShake(const HandShakeCallBack& HSCB);

    void Start();
    void Stop();
};

}
