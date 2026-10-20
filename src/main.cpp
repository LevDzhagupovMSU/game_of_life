#include <csignal>     
#include <cstdlib>     
#include <fcntl.h>    
#include <mutex>
#include <syslog.h>    
#include <sys/stat.h>  
#include <unistd.h>    
#include <boost/asio/signal_set.hpp>
#include <iostream>

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
    std::cout << "  --port        Specify the port number (default: 8080)" << std::endl;
    std::cout << "  --address     Specify the address to bind (default: 0.0.0.0)" << std::endl;
    std::cout << "  --threads     Specify the number of threads (default: 4)" << std::endl;
    std::cout << "  --pool        Specify the size of the thread pool (default: 2)" << std::endl;
    std::cout << "  --cache       Specify the cache capacity (default: 1000)" << std::endl;
}

int main(int argc, char* argv[]){ 
    bool daemonize = false;
    bool default_mode = true;
    uint16_t port = 8080;
    std::string address = "0.0.0.0";
    size_t num_threads = 4;
    size_t pool_size = 2;
    size_t cache_capacity = 1000;
    for(int i = 1; i < argc; ++i){
        std::string arg = argv[i];
        if(arg == "--daemonize"){
            daemonize = true;
            default_mode = false;
        }else if(arg == "--default"){
            default_mode = true;
            daemonize = false;
        }else if(arg == "--port" && i + 1 < argc){
            try{
                port = static_cast<uint16_t>(std::stoi(argv[++i]));
            }catch(const std::exception& e){
                std::cerr << "Invalid port number: " << argv[i] << std::endl;
                return 1;
            }
        }else if(arg == "--address" && i + 1 < argc){
            address = argv[++i];
        }else if(arg == "--threads" && i + 1 < argc){
            try{
                num_threads = static_cast<size_t>(std::stoi(argv[++i]));
            }catch(const std::exception& e){
                std::cerr << "Invalid thread count: " << argv[i] << std::endl;
                return 1;
            }
        }else if(arg == "--pool" && i + 1 < argc){
            try{
                pool_size = static_cast<size_t>(std::stoi(argv[++i]));
            }catch(const std::exception& e){
                std::cerr << "Invalid pool size: " << argv[i] << std::endl;
                return 1;
            }
        }else if(arg == "--cache" && i + 1 < argc){
            try{
                cache_capacity = static_cast<size_t>(std::stoi(argv[++i]));
            }catch(const std::exception& e){
                std::cerr << "Invalid cache capacity: " << argv[i] << std::endl;
                return 1;
            }
        }else if(arg == "--help"){
            print_usage(argv[0]);
            return 0;
        }else{
            std::cerr << "Unknown argument: " << arg << std::endl;
            print_usage(argv[0]);
            return 1;
        }
    }

    if(daemonize){
        ::daemonize();
        openlog("task2", LOG_PID | LOG_CONS, LOG_DAEMON);
        syslog(LOG_INFO, "Daemon started, PID=%d", getpid());
        syslog(LOG_INFO, "Config: address=%s, port=%d, threads=%d, pool=%d, cache=%d",
            address.c_str(), port, static_cast<int>(num_threads), static_cast<int>(pool_size), static_cast<int>(cache_capacity));
    }else{
        openlog("task2", LOG_PID | LOG_CONS | LOG_PERROR, LOG_DAEMON);
        syslog(LOG_INFO, "Application started in default mode");
    }


    try{
        std::condition_variable stop_cv;
        std::mutex stop_mutex;
        bool stop_requested = false;
        boost::asio::io_context io;
        server::Server server(io, address, port, num_threads, pool_size, cache_capacity);
        auto signals = std::make_shared<boost::asio::signal_set>(io, SIGINT, SIGTERM, SIGHUP);
        signals->async_wait([&](const boost::system::error_code& ec, int signum) {
                if(ec)
                    return;

                {
                    std::unique_lock<std::mutex> lock(stop_mutex);
                    stop_requested = true;
                }
                stop_cv.notify_one();
            });
        
        server.Start();
        std::unique_lock<std::mutex> lock(stop_mutex);
        stop_cv.wait(lock, [&stop_requested](){ return stop_requested; });
        lock.unlock();
        server.Stop();
        if(default_mode){
            std::cout << "Server stopped." << std::endl;
            syslog(LOG_INFO, "Application stopped normally");
        }
        else
            syslog(LOG_INFO, "Daemon stopped.");
    }catch(const std::exception& e){
        if(default_mode)
            std::cerr << "Exception: " << e.what() << std::endl;
        else
            syslog(LOG_ERR, "Exception: %s", e.what());
    }catch(...){
        if(default_mode)
            std::cerr << "Unknown exception occurred." << std::endl;
        else
            syslog(LOG_ERR, "Unknown exception occurred.");
    }

    return 0;
}