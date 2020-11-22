#ifndef OCTOMQ_LOG_H_
#define OCTOMQ_LOG_H_

#include <chrono>
#include <cinttypes>
#include <iostream>
#include <map>
#include <mutex>
#include <string>

#include "network/adapter.hpp"
#include "network/network.hpp"

#define OCTOMQ_MAX_LOG_LINE_LENGTH (256)
#define OCTOMQ_VERSION_STRING "1.2.0"
#define OCTOMQ_USE_EMOJI_START_MESSAGE

#define OCTOMQ_BLACK "\u001b[30m"
#define OCTOMQ_RED "\u001b[31m"
#define OCTOMQ_GREEN "\u001b[32m"
#define OCTOMQ_YELLOW "\u001b[33m"
#define OCTOMQ_BLUE "\u001b[34m"
#define OCTOMQ_MAGENTA "\u001b[35m"
#define OCTOMQ_CYAN "\u001b[36m"
#define OCTOMQ_WHITE "\u001b[37m"
#define OCTOMQ_BOLD "\u001b[1m"
#define OCTOMQ_RESET "\u001b[0m"
#define OCTOMQ_LINE_BEGIN '\r'

#if defined(__APPLE__) && defined(OCTOMQ_USE_EMOJI_START_MESSAGE)
#define OCTOMQ_ICON ("🐙 ")
#else
#define OCTOMQ_ICON ("")
#endif

namespace octopus_mq {

enum class log_type : std::uint8_t {
    more = 0x0,
    info = 0x1,
    note = 0x2,
    warning = 0x3,
    error = 0x4,
    fatal = 0x5
};

class log {
    static inline std::mutex _mutex;
    static inline char _buffer[OCTOMQ_MAX_LOG_LINE_LENGTH] = { 0 };
    static constexpr char _version_string[] = OCTOMQ_VERSION_STRING;
    static inline long long _start_timestamp = 0;
    static inline bool _relative_timestamp = false;
    static inline std::string _last_adapter_name;

    static long long get_timestamp();

    static inline const std::map<log_type, std::string> _log_prefix = {
        { log_type::info, std::string() },
        { log_type::note, OCTOMQ_CYAN OCTOMQ_BOLD "note: " OCTOMQ_RESET OCTOMQ_BOLD },
        { log_type::warning, OCTOMQ_YELLOW OCTOMQ_BOLD "warning: " OCTOMQ_RESET OCTOMQ_BOLD },
        { log_type::error, OCTOMQ_RED OCTOMQ_BOLD "error: " OCTOMQ_RESET OCTOMQ_BOLD },
        { log_type::fatal, OCTOMQ_RED OCTOMQ_BOLD "fatal: " OCTOMQ_RESET OCTOMQ_BOLD },
        { log_type::more, std::string() }
    };

    static void print_time(std::ostream &sout, const log_type &type,
                           const long long &timestamp = 0);

   public:
    static void print_started(const bool daemon = false);
    static void print_stopped();
    static void print_failed();
    static void print_empty_line();
    static void printf(const log_type &type, const char *format, ...);
    static void print(const log_type &type, const std::string &message,
                      const std::string &adapter_name = std::string());
    static void print_event(const std::string &adapter_name, const std::string &remote_address,
                            const std::string &client_id, const network_event_type &event_type,
                            const std::string &action);
    static void print_action(std::ostream &sout, const long long &timestamp,
                             const network_event_type &event_type, const std::string &action,
                             const std::string &remote, const std::string &client_id);
    static void print_help();
    static const char *version_string();
};

}  // namespace octopus_mq

#endif
