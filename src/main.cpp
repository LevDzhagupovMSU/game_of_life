#include <boost/asio/signal_set.hpp>
#include <condition_variable>
#include <csignal>
#include <mutex>
#include <iostream>
#include "server/server.hpp"

int main() {
    try{
        boost::asio::io_context io;
        server::Server server(io, "0.0.0.0", 8080);

        std::mutex stop_mutex;
        std::condition_variable stop_cv;
        bool stop_requested = false;
        boost::asio::signal_set signals(io, SIGINT, SIGTERM);
        signals.async_wait([&](const boost::system::error_code& ec, int){
            if(ec)
                return;
            {
                std::lock_guard lock(stop_mutex);
                stop_requested = true;
            }
            stop_cv.notify_one();
        });

        server.Start();
        std::unique_lock lock(stop_mutex);
        stop_cv.wait(lock, [&]{ return stop_requested; });
        lock.unlock();
        server.Stop();
    }catch(std::exception& e){
            std::cerr << "Exception: " << e.what() << std::endl;

            return 1;
        }
    return 0;
}
