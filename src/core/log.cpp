#include "core/log.hpp"

#include <algorithm>
#include <cstdarg>
#include <iomanip>

namespace octopus_mq {

long long log::get_timestamp() {
    long long timestamp = std::chrono::duration_cast<std::chrono::milliseconds>(
                              std::chrono::system_clock::now().time_since_epoch())
                              .count();
    if (_relative_timestamp) {
        if (_start_timestamp == 0) {
            _start_timestamp = timestamp;
            timestamp = 0;
        } else
            timestamp -= _start_timestamp;
    }
    return timestamp;
}

void log::print_time(std::ostream &sout, const log_type &type, const long long &timestamp) {
    if (type != log_type::more) {
        sout << OCTOMQ_LINE_BEGIN << std::dec << std::right << std::setw(14) << std::setfill('0')
             << std::to_string(timestamp).insert(10, ".") << ": ";
    } else
        sout << OCTOMQ_LINE_BEGIN << std::setw(16) << std::setfill(' ') << " ";
}

void log::print_started(const bool daemon) {
    log::printf(log_type::info, "%s%soctopusMQ %s started%s.%s", OCTOMQ_ICON, OCTOMQ_BOLD,
                log::version_string(), (daemon ? " as a daemon" : ""), OCTOMQ_RESET);
}

void log::print_stopped() { log::print(log_type::info, "stopped."); }

void log::print_failed() {
    log::printf(log_type::info, "%s%sstopped due to an error.%s", OCTOMQ_RED, OCTOMQ_BOLD,
                OCTOMQ_RESET);
}

void log::print_empty_line() {
    std::lock_guard<std::mutex> log_lock(_mutex);
    std::cout << std::endl;
}

void log::printf(const log_type &type, const char *format, ...) {
    if ((format != nullptr) and (*format != '\0')) {
        const long long timestamp = get_timestamp();
        std::ostream &sout = (type >= log_type::warning) ? std::cerr : std::cout;
        std::lock_guard<std::mutex> log_lock(_mutex);
        print_time(sout, type, timestamp);
        va_list argptr;
        va_start(argptr, format);
        vsnprintf(_buffer, OCTOMQ_MAX_LOG_LINE_LENGTH, format, argptr);
        va_end(argptr);
        std::cout << _log_prefix.find(type)->second << _buffer << OCTOMQ_RESET << std::endl;
    }
}

void log::print(const log_type &type, const std::string &message, const std::string &adapter_name) {
    if (not message.empty()) {
        const long long timestamp = get_timestamp();
        std::ostream &sout = (type >= log_type::warning) ? std::cerr : std::cout;
        std::lock_guard<std::mutex> log_lock(_mutex);
        if (not adapter_name.empty() and _last_adapter_name != adapter_name) {
            print_time(sout, log_type::more);
            std::cout << OCTOMQ_BOLD << std::left << std::setw(35) << std::setfill(' ')
                      << adapter_name << OCTOMQ_RESET << std::endl;
            _last_adapter_name = adapter_name;
        }
        print_time(sout, type, timestamp);
        std::cout << _log_prefix.find(type)->second << message << OCTOMQ_RESET << std::endl;
    }
}

void log::print_action(std::ostream &sout, const long long &timestamp,
                       const network_event_type &event_type, const std::string &action,
                       const std::string &remote, const std::string &client_id) {
    print_time(sout, log_type::info, timestamp);
    std::cout << OCTOMQ_RESET << std::right << std::setw(18) << std::setfill(' ') << action
              << (event_type == network_event_type::receive ? " <-- " : " --> ") << remote;
    if (client_id.empty())
        std::cout << std::endl;
    else
        std::cout << OCTOMQ_WHITE << " (" << client_id << ')' << OCTOMQ_RESET << std::endl;
}

void log::print_event(const std::string &adapter_name, const std::string &remote_address,
                      const std::string &client_id, const network_event_type &event_type,
                      const std::string &action) {
    const long long timestamp = get_timestamp();
    std::lock_guard<std::mutex> log_lock(_mutex);
    if (_last_adapter_name != adapter_name) {
        print_time(std::cout, log_type::more);
        std::cout << OCTOMQ_BOLD << std::left << std::setw(35) << std::setfill(' ') << adapter_name
                  << OCTOMQ_RESET << std::endl;
        _last_adapter_name = adapter_name;
    }
    print_action(std::cout, timestamp, event_type, action, remote_address, client_id);
}

void log::print_help() {
    std::lock_guard<std::mutex> log_lock(_mutex);
    std::cout << OCTOMQ_BOLD << "octopusmq" << OCTOMQ_RESET
              << " /path/to/settings.json [--option [value]]" << std::endl
              << "options:" << std::endl
              << std::setfill(' ') << std::left << std::setw(16) << "    --help" << std::setw(0)
              << "print this help message and exit." << std::endl;
}

const char *log::version_string() { return _version_string; }

}  // namespace octopus_mq
