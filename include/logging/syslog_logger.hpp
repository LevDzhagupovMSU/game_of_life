#pragma once

#include <syslog.h>
#include <string>

namespace logging {

class SyslogLogger {
    std::string ident_;
public:
    SyslogLogger(std::string ident, int facility, int options = LOG_PID | LOG_NDELAY);

    ~SyslogLogger();

    void info(const std::string& message);
    void warning(const std::string& message);
    void error(const std::string& message);
    void debug(const std::string& message);
    void critical(const std::string& message);

    SyslogLogger(const SyslogLogger&) = delete;
    SyslogLogger& operator=(const SyslogLogger&) = delete;
};

}
