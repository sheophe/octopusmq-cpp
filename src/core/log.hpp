#ifndef OCTOMQ_LOG_H_
#define OCTOMQ_LOG_H_

#include <chrono>
#include <map>
#include <mutex>
#include <string>

#include "core/topic.hpp"
#include "network/adapter.hpp"
#include "network/network.hpp"

#define OCTOMQ_MAX_LOG_LINE_LENGTH (256)
#define OCTOMQ_VERSION_STRING "1.2.0"
#define OCTOMQ_BUILD_NUMBER (4)

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

using std::string;

enum class log_type { info, note, warning, error, fatal, more };

class log {
    static std::mutex _mutex;
    static char _buffer[OCTOMQ_MAX_LOG_LINE_LENGTH];
    static const char *_version_string;
    static const unsigned int _build_number = OCTOMQ_BUILD_NUMBER;
    static long long _start_timestamp;
    static bool _relative_timestamp;
    static string _last_adapter_name;

    static inline const std::map<log_type, string> _log_prefix = {
        { log_type::info, "" },
        { log_type::note, OCTOMQ_RESET OCTOMQ_BOLD "note: " OCTOMQ_RESET OCTOMQ_BOLD },
        { log_type::warning, OCTOMQ_YELLOW OCTOMQ_BOLD "warning: " OCTOMQ_RESET OCTOMQ_BOLD },
        { log_type::error, OCTOMQ_RED OCTOMQ_BOLD "error: " OCTOMQ_RESET OCTOMQ_BOLD },
        { log_type::fatal, OCTOMQ_RED OCTOMQ_BOLD "fatal: " OCTOMQ_RESET OCTOMQ_BOLD },
        { log_type::more, "" }
    };

    static void print_time(const log_type &type);
    static void print_action_left(const network_event_type &event_type, const string &action);

   public:
    static void print_started(const bool daemon = false);
    static void print_stopped(const bool error = false);
    static void print(const log_type &type, const char *format, ...);
    static void print(const log_type &type, const string &message);
    static void print_event(const phy &phy, const address &remote_address, const string &client_id,
                            const string &protocol_name, const port_int &local_port,
                            const string &adapter_role, const network_event_type &event_type,
                            const string &action);
    static void print_action(const network_event_type &event_type, const string &action);
    static void print_action(const network_event_type &event_type, const string &action,
                             const class address &remote, const string &client_id);
    static void print_help();
    static const char *version_string();
    static unsigned int build_number();
};

}  // namespace octopus_mq

#endif
