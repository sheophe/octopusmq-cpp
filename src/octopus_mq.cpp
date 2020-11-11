#include <signal.h>

#include <stdexcept>

#include "core/log.hpp"
#include "threads/control.hpp"

using namespace octopus_mq;

int main(const int argc, const char **argv) {
    // Configure signal handlers.
    control::init_signal_handlers();
    // Making current thread the control thread. It reads the settings and
    // spawns other threads as needed. Wrapping the worker function into
    // try...catch to handle fatal exceptions.
    try {
        control::run(argc, argv);
    } catch (const std::exception &e) {
        log::print(log_type::fatal, e.what());
        log::print_stopped(true);
        return 1;
    }
    return 0;
}
