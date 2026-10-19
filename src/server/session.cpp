#include "server/session.hpp"
#include "server/message.hpp"
#include "server/scheduler.hpp"

#include <boost/asio/bind_executor.hpp>
#include <boost/asio/impl/read.hpp>
#include <boost/asio/post.hpp>
#include <boost/asio/strand.hpp>
#include <boost/beast/core/error.hpp>
#include <boost/system/detail/error_code.hpp>
#include <chrono>
#include <cstdint>
#include <iostream>

server::Session::Session(boost::asio::io_context& io, 
                         boost::asio::ip::tcp::socket socket,
                         Scheduler& scheduler, SessionId id,
                         StopCallback stop_callback) : web_socket_(std::move(socket)),
                                                       strand_(boost::asio::make_strand(io)),
                                                       close_timer_(io),
                                                       scheduler_(scheduler),
                                                       id_(id), stop_callback_(std::move(stop_callback)) {};

server::SessionId server::Session::get_id() const{ return id_; }

const server::SessionState server::Session::get_state() const{
    return state_.load(std::memory_order_acquire);
}

void server::Session::DoHandShake(const HandShakeCallBack& HSCB){
    boost::asio::post(strand_, [self = shared_from_this(), HSCB = std::move(HSCB)](){
        self->web_socket_.set_option(boost::beast::websocket::stream_base::timeout::suggested
                            (boost::beast::role_type::server));
        self->web_socket_.async_accept(boost::asio::bind_executor(self->strand_,
                            [self, HSCB = std::move(HSCB)](const boost::beast::error_code& ec){
            if(!ec)
                self->web_socket_.text(true);
            HSCB(ec);
        }));
    });
}

void server::Session::Start(){
    auto expected = server::SessionState::Connecting;
    if(!state_.compare_exchange_strong(expected, server::SessionState::Running,
                                       std::memory_order_acq_rel))
        return;
    Read();
}

void server::Session::Stop(){
    auto previous = state_.exchange(server::SessionState::Disconnecting,
                                    std::memory_order_acq_rel);
    if(previous == server::SessionState::Disconnecting)
        return;
    boost::asio::post(strand_, [self = shared_from_this(), previous](){
        if(self->finished_.load(std::memory_order_acquire))
            return;

        self->close_requested_ = true;
        if(previous != server::SessionState::Running || !self->web_socket_.is_open()){
            self->Finish();
            return;
        }

        while(self->message_queue_.size() > (self->write_in_progress_ ? 1u : 0u))
            self->message_queue_.pop_back();

        if(!self->write_in_progress_)
            self->CloseWhenReady();
    });
}

void server::Session::Finish(){
    if(finished_.exchange(true, std::memory_order_acq_rel))
        return;

    state_.store(server::SessionState::Disconnecting, std::memory_order_release);
    close_timer_.cancel();
    while(message_queue_.size() > (write_in_progress_ ? 1u : 0u))
        message_queue_.pop_back();
    boost::beast::error_code ignored;
    boost::beast::get_lowest_layer(web_socket_).socket().shutdown(
        boost::asio::ip::tcp::socket::shutdown_both, ignored);
    boost::beast::get_lowest_layer(web_socket_).socket().close(ignored);
    stop_callback_(id_);
}

void server::Session::Read(){
    web_socket_.async_read(buffer_, boost::asio::bind_executor(strand_, [self = shared_from_this()]
            (const boost::beast::error_code& ec, size_t){
        if(self->get_state() != server::SessionState::Running)
            return;
        
        if(ec){
            if(ec != boost::beast::websocket::error::closed)
                std::cerr << "Session read error: " << ec.what() << std::endl;
            self->Finish();
            return;
        }
        
        std::string payload = self->Message();

        try{
            self->scheduler_.Submit({self->id_, payload, [self](std::variant<WorkerResult, std::string> result){
                if(std::holds_alternative<WorkerResult>(result))
                    self->Send(std::move(std::get<WorkerResult>(result)));
                else
                    self->SendError(std::move(std::get<std::string>(result)));
            }});
        }catch(const std::exception& e){
            self->SendError(e.what());
        }

        self->Read();
    }));
}

void server::Session::Send(server::WorkerResult worker_result){
    if(get_state() != server::SessionState::Running)
        return;

    SendPayload(Parse::to_payload(worker_result));
}

void server::Session::SendError(std::string error){
    if(get_state() != server::SessionState::Running)
        return;
    SendPayload(nlohmann::json{{"error", std::move(error)}}.dump());
}

void server::Session::SendPayload(std::string payload){
    boost::asio::post(strand_, [self = shared_from_this(), payload = std::move(payload)]() mutable {
        if(self->get_state() != server::SessionState::Running ||
           self->finished_.load(std::memory_order_acquire))
            return;

        self->message_queue_.push_back(std::move(payload));
        if(!self->write_in_progress_)
            self->SendNextMessage();
    });
}

void server::Session::SendNextMessage(){
    if(write_in_progress_ || message_queue_.empty() ||
       get_state() != server::SessionState::Running ||
       finished_.load(std::memory_order_acquire))
        return;

    write_in_progress_ = true;
    web_socket_.async_write(boost::asio::buffer(message_queue_.front()),
        boost::asio::bind_executor(strand_, [self = shared_from_this()]
            (const boost::beast::error_code& ec, size_t){
                self->write_in_progress_ = false;
                if(!self->message_queue_.empty())
                    self->message_queue_.pop_front();

                if(ec){
                    if(self->get_state() == server::SessionState::Running)
                        std::cerr << "Session write error: " << ec.what() << std::endl;
                    self->Finish();
                    return;
                }

                if(self->get_state() != server::SessionState::Running){
                    self->message_queue_.clear();
                    self->close_requested_ = true;
                    self->CloseWhenReady();
                    return;
                }

                if(!self->message_queue_.empty())
                    self->SendNextMessage();
                else if(self->close_requested_)
                    self->CloseWhenReady();
            }));
}

void server::Session::CloseWhenReady(){
    if(write_in_progress_ || close_in_progress_ ||
       finished_.load(std::memory_order_acquire))
        return;

    if(!web_socket_.is_open()){
        Finish();
        return;
    }

    close_in_progress_ = true;
    close_timer_.expires_after(std::chrono::seconds(2));
    close_timer_.async_wait(boost::asio::bind_executor(strand_,
        [self = shared_from_this()](const boost::beast::error_code& ec){
            if(!ec)
                self->Finish();
        }));
    web_socket_.async_close(boost::beast::websocket::close_code::normal,
        boost::asio::bind_executor(strand_, [self = shared_from_this()]
            (const boost::beast::error_code& ec){
                if(ec && !self->finished_.load(std::memory_order_acquire))
                    std::cerr << "Session close error: " << ec.what() << std::endl;
                self->Finish();
            }));
}

std::string server::Session::Message(){
    auto begin = boost::asio::buffers_begin(buffer_.data());
    auto end   = boost::asio::buffers_end(buffer_.data());
    std::string payload(begin, end);

    buffer_.consume(buffer_.size());
    return payload;
}
