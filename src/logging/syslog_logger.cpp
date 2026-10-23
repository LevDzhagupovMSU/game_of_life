#include "logging/syslog_logger.hpp"

#include <utility>

namespace logging {

SyslogLogger::SyslogLogger(std::string ident, int facility, int options)
    : ident_(std::move(ident)) {
    openlog(ident_.c_str(), options, facility);
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
