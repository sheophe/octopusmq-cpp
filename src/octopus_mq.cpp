#include <signal.h>

#include <stdexcept>

#include "core/log.hpp"
#include "threads/control.hpp"

using namespace octopus_mq;

int main(const int argc, const char **argv) {
    // Configure signal handlers.
    signal(SIGINT, thread::control::signal_handler);
    signal(SIGABRT, thread::control::signal_handler);
    signal(SIGHUP, thread::control::signal_handler);
    // Making current thread the control thread. It reads the settings and
    // spawns other threads as needed. Wrapping the worker function into
    // try...catch to handle fatal exceptions.
    try {
        thread::control::init(argc, argv);
        thread::control::run();
    } catch (const std::exception &e) {
        log::print(log_type::fatal, e.what());
        log::print_stopped(true);
        return 1;
    }
    if (thread::control::initialized()) log::print_stopped();
    return 0;
}
