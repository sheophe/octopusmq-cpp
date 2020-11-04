#include "threads/control.hpp"

#include <errno.h>

#include <filesystem>
#include <iostream>
#include <stdexcept>
#include <string>

#include "core/log.hpp"
#include "core/settings.hpp"

#define OCTOMQ_DEFAULT_CONTROL_PORT (18888)
#define OCTOMQ_DEFAULT_CONTROL_TOPIC "$SYS/config/octopusmq"

namespace octopus_mq::thread {

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

void control::init(const int argc, const char **argv) {
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
                settings::load(config_file_name);
            } else
                throw std::runtime_error("misleading option: " + string(argv[i]));
        }
    if (argc <= 1 or config_file_name.empty()) {
        log::print(log_type::error, "configuration file argument is missing.");
        return log::print_help();
    }

    log::print_started(_daemon);
    // Initialization routines

    _initialized = true;
    log::print(log_type::info, "initialized.");
}

void control::run() {}

void control::stop() {}

void control::signal_handler(int signal) {
    log::print(log_type::warning, "received %d signal, stopping", signal);
    stop();
}

bool &control::initialized() { return _initialized; }

}  // namespace octopus_mq::thread
