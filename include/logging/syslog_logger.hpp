#pragma once

#include <syslog.h>
#include <string>

namespace logging {

class SyslogLogger {
public:
    SyslogLogger(const std::string& ident, int facility);

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