#include "threads/control.hpp"

#include <errno.h>

#include <chrono>
#include <filesystem>
#include <iostream>
#include <stdexcept>
#include <string>

#include "core/log.hpp"
#include "core/settings.hpp"
#include "network/adapter_factory.hpp"

namespace octopus_mq {

void control::arg_help() {
    log::print_help();
    exit(0);
}

void control::arg_daemon() { _daemon = true; }

void control::daemonize() {
    log::print(log_type::fatal, "daemonization is not supported in current version of octopusmq.");
    exit(1);
}

void control::initialize_adapters() {
    for (auto &adapter : _adapter_pool) {
        try {
            adapter.second =
                adapter_interface_factory::from_settings(adapter.first, _message_queue);
        } catch (const std::runtime_error &re) {
            log::print(log_type::fatal, "adapter '" + adapter.first->name() + "': " + re.what());
            _should_stop = true;
            _initialized = false;
            return;
        }
    }
    // Running adapters only after initialization was successful
    for (auto &adapter : _adapter_pool) adapter.second->run();
    _initialized = true;
}

void control::shutdown_adapters() {
    for (auto &adapter : _adapter_pool) {
        adapter.second->stop();
        adapter.second.reset();
    }
    _adapter_pool.clear();
}

void control::print_adapters() {
    size_t pool_size = _adapter_pool.size();
    log::print(log_type::info, "running %lu %s:", pool_size,
               (pool_size > 1) ? "adapters" : "adapter");
    for (auto &adapter : _adapter_pool)
        log::print(log_type::more, adapter.first->name() + OCTOMQ_WHITE + " listening on " +
                                       adapter.first->phy().ip_string() + ':' +
                                       std::to_string(adapter.first->port()) + OCTOMQ_RESET);
    log::print_empty_line();
}

static std::map<const int, const char *> supported_signals = {
    { SIGHUP, "hangup" }, { SIGINT, "interrupt" }, { SIGQUIT, "quit" }, { SIGABRT, "abort" }
};

void control::init_signal_handlers() {
    for (auto &sig : supported_signals) signal(sig.first, signal_handler);
}

void control::signal_handler(int sig) {
    log::print(log_type::info, "received %s signal, stopping...", supported_signals[sig]);
    _should_stop = true;
}

void control::run(const int argc, const char **argv) {
    string config_file_name;
    if (argc > 1)
        for (int i = 1; i < argc; ++i) {
            if (argv[i][0] == '-') {
                if (auto iter = _argument_map.find(argv[i]); iter != _argument_map.end())
                    iter->second();
                else
                    throw std::runtime_error("unknown option: " + string(argv[i]));
            } else if (config_file_name.empty()) {
                config_file_name = std::filesystem::absolute(argv[i]);
                if (config_file_name.empty())
                    throw std::runtime_error("not a valid file name: " + string(argv[i]));
                if (not std::filesystem::exists(config_file_name))
                    throw std::runtime_error("path does not exist: " + config_file_name);
                if (not std::filesystem::is_regular_file(config_file_name))
                    throw std::runtime_error("not a file: " + config_file_name);
                settings::load(config_file_name, _adapter_pool);
            } else
                throw std::runtime_error("misleading option: " + string(argv[i]));
        }
    if (argc <= 1 or config_file_name.empty()) {
        log::print(log_type::error, "configuration file argument is missing.");
        return log::print_help();
    }

    if (_daemon) daemonize();
    log::print_started(_daemon);

    initialize_adapters();

    if (_initialized) {
        print_adapters();
        // Following functions implements a loop of the main thread.
        // The loop is running as long as _should_stop == false.
        message_queue_manager();
        shutdown_adapters();
    }

    log::print_stopped(not _initialized);
}

void control::message_queue_manager() {
    while (not _should_stop) {
        try {
            // This is the only place where message_pool should be read.
            // All adapters strictly push to message_pool, but never read.
            // This function is responsible for reading and calling inject_publish on all adapters
            _message_queue.wait_and_pop_all(std::chrono::milliseconds(100), _adapter_pool);
        } catch (const std::runtime_error &re) {
            log::print(log_type::fatal, re.what());
            _initialized = false;  // To indicate an error in log::print_stopped()
            break;
        }
    }
}

}  // namespace octopus_mq
