#ifndef OCTOMQ_CONTROL_THREAD_H_
#define OCTOMQ_CONTROL_THREAD_H_

#include <signal.h>

#include <cstdint>
#include <map>
#include <string>
#include <thread>
#include <tuple>
#include <vector>

#include "network/adapter.hpp"
#include "network/message.hpp"
#include "network/network.hpp"

namespace octopus_mq {

using std::string;
using arg_handler = void (*)();

class control {
    static inline bool _initialized = false;
    static inline bool _daemon = false;
    static inline bool _should_stop = false;

    static inline message_queue _message_queue = message_queue();
    static inline adapter_pool _adapter_pool = adapter_pool();

    static void arg_daemon();
    static void arg_help();
    static void daemonize();
    static void initialize_adapters();
    static void shutdown_adapters();
    static void print_adapters();

    static void message_queue_manager();  // Main thread routine

    static inline std::map<string, arg_handler> _argument_map = { { "--daemon", arg_daemon },
                                                                  { "--help", arg_help } };

   public:
    static void init_signal_handlers();
    static void signal_handler(int sig);

    static void run(const int argc, const char **argv);
};

}  // namespace octopus_mq

#endif
