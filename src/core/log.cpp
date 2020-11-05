#include "core/log.hpp"

#include <chrono>
#include <cstdarg>
#include <iomanip>
#include <iostream>

#include "core/topic.hpp"

namespace octopus_mq {

using std::chrono::duration_cast, std::chrono::milliseconds, std::chrono::system_clock;

std::mutex log::_mutex = std::mutex();

char log::_buffer[OCTOMQ_MAX_LOG_LINE_LENGTH] = { 0 };

const char *log::_version_string = OCTOMQ_VERSION_STRING;

long long log::_start_timestamp = 0;

bool log::_relative_timestamp = false;

void log::print_time(const log_type &type) {
    if (type != log_type::more) {
        auto timestamp =
            duration_cast<milliseconds>(system_clock::now().time_since_epoch()).count();
        if (_relative_timestamp) {
            if (_start_timestamp == 0) {
                _start_timestamp = timestamp;
                timestamp = 0;
            } else
                timestamp -= _start_timestamp;
        }
        std::cout << std::dec << std::right << std::setw(14) << std::setfill('0')
                  << std::to_string(timestamp).insert(10, ".") << ": ";
    } else
        std::cout << std::setw(16) << std::setfill(' ') << " ";
}

void log::print_started(const bool daemon) {
    log::print(log_type::info, "%s%soctopusMQ %s started%s.%s", OCTOMQ_ICON, OCTOMQ_BOLD,
               log::version_string(), (daemon ? " as a daemon" : ""), OCTOMQ_RESET);
}

void log::print_stopped(const bool error) {
    if (error)
        log::print(log_type::info, "%s%sstopped due to an error.%s", OCTOMQ_RED, OCTOMQ_BOLD,
                   OCTOMQ_RESET);
    else
        log::print(log_type::info, "stopped.");
}

void log::print(const log_type &type, const char *format, ...) {
    if ((format != nullptr) and (*format != '\0')) {
        std::lock_guard<std::mutex> log_lock(_mutex);
        print_time(type);
        va_list argptr;
        va_start(argptr, format);
        vsnprintf(_buffer, OCTOMQ_MAX_LOG_LINE_LENGTH, format, argptr);
        va_end(argptr);
        std::cout << _log_prefix.find(type)->second << _buffer << OCTOMQ_RESET << std::endl;
    }
}

void log::print(const log_type &type, const string &message) {
    if (not message.empty()) {
        std::lock_guard<std::mutex> log_lock(_mutex);
        std::cout << _log_prefix.find(type)->second << message << OCTOMQ_RESET << std::endl;
    }
}

void log::print_action_left(const network_event_type &event_type, const string &action,
                            const uint32_t &length, const uint32_t &hash) {
    std::cout << std::right << std::hex << std::setw(8) << std::setfill('0') << hash;
    std::cout << std::right << std::setfill(' ') << std::setw(10)
              << string(std::to_string(length) + "B") << OCTOMQ_BOLD << std::right << std::setw(15)
              << action << OCTOMQ_RESET;
    std::cout << OCTOMQ_BOLD << (event_type == network_event_type::receive ? " <-- " : " --> ")
              << OCTOMQ_RESET;
}

void log::print_action(const network_event_type &event_type, const string &action,
                       const uint32_t &length, const uint32_t &hash, const class topic &topic) {
    print_time(log_type::info);
    print_action_left(event_type, action, length, hash);
    if (!topic.name().empty())
        std::cout << topic.name();
    else if (topic.id() != OCTOMQ_NULL_TOPICID)
        std::cout << "topic.id=" << std::dec << topic.id();
    else
        std::cout << OCTOMQ_RED << "(unknown topic)" << OCTOMQ_RESET;
    std::cout << std::endl;
}

void log::print_action(const network_event_type &event_type, const string &action,
                       const uint32_t &length, const uint32_t &hash, const class address &remote) {
    print_time(log_type::info);
    print_action_left(event_type, action, length, hash);
    std::cout << OCTOMQ_WHITE << remote.to_string() << OCTOMQ_RESET << std::endl;
}

void log::print_event(const phy &phy, const address &remote_address, const string &protocol_name,
                      const port_int &local_port, const string &adapter_role,
                      const network_event_type &event_type, const string &action,
                      const uint32_t &length, const uint32_t &hash, const class topic &topic) {
    std::lock_guard<std::mutex> log_lock(_mutex);
    print_time(log_type::more);
    std::cout << OCTOMQ_BOLD << std::left << std::setw(35) << std::setfill(' ')
              << string('[' + phy.name() + ':' + std::to_string(local_port) + "] " + protocol_name +
                        ' ' + adapter_role + ':')
              << OCTOMQ_RESET << '\n';
    if (topic.empty())
        print_action(event_type, action, length, hash, remote_address);
    else
        print_action(event_type, action, length, hash, topic);
}

void log::print_help() {
    std::lock_guard<std::mutex> log_lock(_mutex);
    std::cout << OCTOMQ_BOLD << "octopusmq" << OCTOMQ_RESET
              << " /path/to/settings.json [--option [value]]" << '\n';
    std::cout << "options:" << std::endl << std::setfill(' ');
    std::cout << std::left << std::setw(16) << "    --daemon" << std::setw(0)
              << "daemonize the process. useful when running from systemd." << '\n';
    std::cout << std::left << std::setw(16) << "    --help" << std::setw(0)
              << "print this help message and exit." << std::endl;
}

const char *log::version_string() { return _version_string; }

unsigned int log::build_number() { return _build_number; }

}  // namespace octopus_mq
