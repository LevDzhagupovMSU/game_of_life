#include "server/session.hpp"
#include "server/message.hpp"
#include "server/scheduler.hpp"

#include <boost/asio/bind_executor.hpp>
#include <boost/asio/impl/read.hpp>
#include <boost/asio/post.hpp>
#include <boost/asio/strand.hpp>
#include <boost/beast/core/error.hpp>
#include <cstdint>
#include <iostream>

// Может ли быть гонка в пределах одной сессии?

server::Session::Session(boost::asio::io_context& io, 
                         boost::asio::ip::tcp::socket socket,
                         Scheduler& scheduler, uint16_t id) : strand_(boost::asio::make_strand(io)),
                                                              web_socket_(std::move(socket)),
                                                              scheduler_(scheduler),
                                                              id_(id) {};

const uint16_t server::Session::get_id() const{ return id_; }

const server::SessionState server::Session::get_state() const{ return state_;}

void server::Session::set_state(server::SessionState new_state){ state_ = new_state; }

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
    state_ = server::SessionState::Running;
    Read();
}

void server::Session::Read(){
    web_socket_.async_read(buffer_, boost::asio::bind_executor(strand_, [self = shared_from_this()]
            (const boost::beast::error_code& ec, size_t){
        if(self->state_ != server::SessionState::Running)
            return;
        
        if(ec){
            std::cerr << "Session read error: " << ec.what() << std::endl; 
            // затычка.
        }
        
        std::string payload = self->Message();
        self->scheduler_.Submit({self->id_, payload, [self](WorkerResult worker_result){
            self->Send(std::move(worker_result));
        }});

        self->Read();
    }));
}

void server::Session::Send(server::WorkerResult){}

std::string server::Session::Message(){
    auto begin = boost::asio::buffers_begin(buffer_.data());
    auto end   = boost::asio::buffers_end(buffer_.data());
    std::string payload(begin, end);

    buffer_.consume(buffer_.size());
    return payload;
}