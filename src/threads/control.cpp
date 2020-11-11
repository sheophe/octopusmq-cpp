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

#define OCTOMQ_DEFAULT_CONTROL_PORT (18888)
#define OCTOMQ_DEFAULT_CONTROL_TOPIC "$SYS/config/octopusmq"

namespace octopus_mq {

control_settings::control_settings()
    : _phy(),
      _proto(protocol_type::mqtt),
      _transport(transport_type::tcp),
      _port(OCTOMQ_DEFAULT_CONTROL_PORT),
      _root_topic(OCTOMQ_DEFAULT_CONTROL_TOPIC) {}

void control_settings::phy(const class phy &phy) { _phy = phy; }

void control_settings::phy(class phy &&phy) { _phy = std::move(phy); }

void control_settings::phy(const string &phy) { _phy = octopus_mq::phy(phy); }

void control_settings::protocol(const protocol_type &proto) { _proto = proto; }

void control_settings::protocol(const string &proto) {
    if (auto iter = _ctrl_proto_map.find(proto); iter != _ctrl_proto_map.end())
        _proto = iter->second;
    else
        throw std::runtime_error("unknwon protocol: " + proto);
}

void control_settings::transport(const transport_type &transport) { _transport = transport; }

void control_settings::transport(const string &transport) {
    if (auto iter = _ctrl_transport_map.find(transport); iter != _ctrl_transport_map.end())
        _transport = iter->second;
    else
        throw std::runtime_error("unknwon transport: " + transport);
}

void control_settings::port(const port_int &port) { _port = port; }

void control_settings::root(const string &root) { _root_topic = root; }

void control_settings::root(string &&root) { _root_topic = std::move(root); }

const class phy &control_settings::phy() const { return _phy; }

const protocol_type &control_settings::protocol() const { return _proto; }

const transport_type &control_settings::transport() const { return _transport; }

const port_int &control_settings::port() const { return _port; }

const string &control_settings::root() const { return _root_topic; }

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
            adapter.second = adapter_interface_factory::from_settings(adapter.first, _message_pool);
        } catch (const std::runtime_error &re) {
            log::print(log_type::fatal, "adapter '" + adapter.first->name() + "': " + re.what());
            _should_stop = true;
            break;
        }
    }
    // Running adapters only after initialization was successful
    for (auto &adapter : _adapter_pool) adapter.second->run();
    _initialized = not _should_stop;
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
        log::print(log_type::more, adapter.first->name() + " (" + adapter.first->phy().ip_string() +
                                       ':' + std::to_string(adapter.first->port()) + ')');
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
        message_pool_manager();

        shutdown_adapters();
    }

    log::print_stopped(not _initialized);
}

void control::message_pool_manager() {
    while (not _should_stop) {
        try {
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
        } catch (const std::runtime_error &re) {
            log::print(log_type::fatal, re.what());
            break;
        }
    }
}

}  // namespace octopus_mq
