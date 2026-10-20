#include "logging/syslog_logger.hpp"

namespace logging {

SyslogLogger::SyslogLogger(const std::string& ident, int facility) {
    openlog(ident.c_str(), LOG_PID | LOG_NDELAY, facility);
}

SyslogLogger::~SyslogLogger() {
    closelog();
}

void SyslogLogger::info(const std::string& message) {
    syslog(LOG_INFO, "%s", message.c_str());
}

void SyslogLogger::warning(const std::string& message) {
    syslog(LOG_WARNING, "%s", message.c_str());
}

void SyslogLogger::error(const std::string& message) {
    syslog(LOG_ERR, "%s", message.c_str());
}

void SyslogLogger::debug(const std::string& message) {
    syslog(LOG_DEBUG, "%s", message.c_str());
}

void SyslogLogger::critical(const std::string& message) {
    syslog(LOG_CRIT, "%s", message.c_str());
}

} // namespace logging