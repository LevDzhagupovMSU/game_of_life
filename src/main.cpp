#include <charconv>
#include <cstddef>
#include <condition_variable>
#include <csignal>
#include <cstdint>
#include <cstdlib>
#include <fcntl.h>
#include <limits>
#include <mutex>
#include <stdexcept>
#include <string>
#include <string_view>
#include <system_error>
#include <syslog.h>
#include <sys/stat.h>
#include <unistd.h>
#include <boost/asio/signal_set.hpp>
#include <iostream>

#include "logging/syslog_logger.hpp"
#include "server/server.hpp"

void daemonize(){
    pid_t pid = fork();
    
    if(pid < 0){
        exit(EXIT_FAILURE);
    }
    if(pid > 0){
        exit(EXIT_SUCCESS);
    }
    if(setsid() < 0){
        exit(EXIT_FAILURE);
    }

    pid = fork();
    if (pid < 0) {
        exit(EXIT_FAILURE);
    }
    if (pid > 0) {
        exit(EXIT_SUCCESS);
    }
    if (chdir("/") < 0) exit(EXIT_FAILURE);
    umask(0);

    close(STDIN_FILENO);
    close(STDOUT_FILENO);
    close(STDERR_FILENO);

    open("/dev/null", O_RDWR);
    dup2(0, 1);               
    dup2(0, 2);      
}

void print_usage(const std::string& program_name){
    std::cout << "Usage: " << program_name << " [OPTIONS]" << std::endl;
    std::cout << "Options:" << std::endl;
    std::cout << "  --help        Show this help message" << std::endl;
    std::cout << "  --daemonize   Run the server as a daemon" << std::endl;
    std::cout << "  --default     Run the server in the default mode (not as a daemon)" << std::endl;
    std::cout << "  --port N      Specify the port number (default: 8080)" << std::endl;
    std::cout << "  --address IP  Specify the address to bind (default: 0.0.0.0)" << std::endl;
    std::cout << "  --threads N   Specify the number of threads (default: 4)" << std::endl;
    std::cout << "  --pool N      Specify the size of the thread pool (default: 2)" << std::endl;
    std::cout << "  --cache N     Specify the cache capacity (default: 1000)" << std::endl;
}

std::size_t parse_positive(std::string_view value, const std::string& option) {
    std::size_t result = 0;
    const auto [end, error] = std::from_chars(value.data(), value.data() + value.size(), result);
    if (error != std::errc{} || end != value.data() + value.size() || result == 0)
        throw std::invalid_argument("Invalid value for " + option + ": " + std::string(value));
    return result;
}

int main(int argc, char* argv[]){ 
    bool daemon_mode = false;
    uint16_t port = 8080;
    std::string address = "0.0.0.0";
    size_t num_threads = 4;
    size_t pool_size = 2;
    size_t cache_capacity = 1000;
    try {
        for(int i = 1; i < argc; ++i){
            const std::string arg = argv[i];
            auto next_value = [&]() -> std::string_view {
                if (i + 1 >= argc)
                    throw std::invalid_argument("Missing value for " + arg);
                return argv[++i];
            };

            if(arg == "--daemonize"){
                daemon_mode = true;
            }else if(arg == "--default"){
                daemon_mode = false;
            }else if(arg == "--port"){
                const auto value = parse_positive(next_value(), arg);
                if (value > std::numeric_limits<uint16_t>::max())
                    throw std::invalid_argument("Port must be in the range 1..65535");
                port = static_cast<uint16_t>(value);
            }else if(arg == "--address"){
                address = next_value();
                if (address.empty())
                    throw std::invalid_argument("Address cannot be empty");
            }else if(arg == "--threads"){
                num_threads = parse_positive(next_value(), arg);
            }else if(arg == "--pool"){
                pool_size = parse_positive(next_value(), arg);
            }else if(arg == "--cache"){
                cache_capacity = parse_positive(next_value(), arg);
            }else if(arg == "--help"){
                print_usage(argv[0]);
                return EXIT_SUCCESS;
            }else{
                throw std::invalid_argument("Unknown argument: " + arg);
            }
        }
    } catch (const std::exception& error) {
        std::cerr << error.what() << std::endl;
        print_usage(argv[0]);
        return EXIT_FAILURE;
    }

    if (daemon_mode)
        ::daemonize();

    const int log_options = LOG_PID | LOG_NDELAY | (daemon_mode ? 0 : LOG_PERROR);
    logging::SyslogLogger logger("game_of_life", LOG_DAEMON, log_options);

    try{
        std::condition_variable stop_cv;
        std::mutex stop_mutex;
        bool stop_requested = false;
        boost::asio::io_context io;
        server::Server server(io, address, port, logger, num_threads, pool_size, cache_capacity);
        boost::asio::signal_set signals(io, SIGINT, SIGTERM, SIGHUP);
        signals.async_wait([&](const boost::system::error_code& ec, int) {
                if(ec)
                    return;

                {
                    std::lock_guard lock(stop_mutex);
                    stop_requested = true;
                }
                stop_cv.notify_one();
            });

        logger.info("Server starting: address=" + address +
                    " port=" + std::to_string(port) +
                    " io_threads=" + std::to_string(num_threads) +
                    " worker_threads=" + std::to_string(pool_size) +
                    " cache_capacity=" + std::to_string(cache_capacity));
        server.Start();
        std::unique_lock<std::mutex> lock(stop_mutex);
        stop_cv.wait(lock, [&stop_requested](){ return stop_requested; });
        lock.unlock();
        server.Stop();
        logger.info("Application stopped normally");
    }catch(const std::exception& e){
        logger.error("Exception: " + std::string(e.what()));
        return EXIT_FAILURE;
    }catch(...){
        logger.critical("Unknown exception occurred");
        return EXIT_FAILURE;
    }

    return EXIT_SUCCESS;
}
