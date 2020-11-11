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
#include "network/network.hpp"
#include "core/message_pool.hpp"

namespace octopus_mq {

using std::string;

using thread_id = uint32_t;

using arg_handler = void (*)();

class control_settings {
    phy _phy;
    protocol_type _proto;
    transport_type _transport;
    port_int _port;
    string _root_topic;

    static inline const std::map<string, protocol_type> _ctrl_proto_map = {
        { "mqtt", protocol_type::mqtt }, { "dds", protocol_type::dds }
    };

    static inline const std::map<string, transport_type> _ctrl_transport_map = {
        { "udp", transport_type::udp },
        { "tcp", transport_type::tcp },
        { "websocket", transport_type::websocket }
    };

   public:
    control_settings();

    void phy(const phy &phy);
    void phy(class phy &&phy);
    void phy(const string &phy);
    void protocol(const protocol_type &proto);
    void protocol(const string &proto);
    void transport(const transport_type &transport);
    void transport(const string &transport);
    void port(const port_int &port);
    void root(const string &root);
    void root(string &&root);

    const class phy &phy() const;
    const protocol_type &protocol() const;
    const transport_type &transport() const;
    const port_int &port() const;
    const string &root() const;
};

class control {
    static inline bool _initialized = false;
    static inline bool _daemon = false;
    static inline bool _should_stop = false;

    static inline message_pool _message_pool = message_pool();
    static inline adapter_pool _adapter_pool = adapter_pool();

    static void arg_daemon();
    static void arg_help();
    static void daemonize();
    static void initialize_adapters();
    static void shutdown_adapters();
    static void print_adapters();

    static void message_pool_manager();  // Main thread routine

    static inline std::map<string, arg_handler> _argument_map = { { "--daemon", arg_daemon },
                                                                  { "--help", arg_help } };

   public:
    static void init_signal_handlers();
    static void signal_handler(int sig);

    static void run(const int argc, const char **argv);
};

}  // namespace octopus_mq

#endif
