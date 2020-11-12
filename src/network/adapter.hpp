#ifndef OCTOMQ_ADAPTER_H_
#define OCTOMQ_ADAPTER_H_

#include <list>
#include <map>
#include <string>
#include <thread>
#include <queue>
#include <mutex>
#include <condition_variable>

#include "json.hpp"
#include "core/message.hpp"
#include "network/network.hpp"

#define OCTOMQ_ADAPTER_FIELD_INTERFACE "interface"
#define OCTOMQ_ADAPTER_FIELD_PROTOCOL "protocol"
#define OCTOMQ_ADAPTER_FIELD_PORT "port"
#define OCTOMQ_ADAPTER_FIELD_TRANSPORT "transport"
#define OCTOMQ_ADAPTER_FIELD_SCOPE "scope"
#define OCTOMQ_ADAPTER_FIELD_DOMAIN "domain"
#define OCTOMQ_ADAPTER_FIELD_ROLE "role"
#define OCTOMQ_ADAPTER_FIELD_QOS "qos"
#define OCTOMQ_ADAPTER_FIELD_NAME "name"
#define OCTOMQ_ADAPTER_FIELD_SECURITY "security"
#define OCTOMQ_ADAPTER_FIELD_CERTIFICATE "certificate"

#define OCTOMQ_ADAPTER_TRANSPORT_UDP "udp"
#define OCTOMQ_ADAPTER_TRANSPORT_TCP "tcp"
#define OCTOMQ_ADAPTER_TRANSPORT_TLS "tls"
#define OCTOMQ_ADAPTER_TRANSPORT_WS "websocket"
#define OCTOMQ_ADAPTER_TRANSPORT_TLSWS "tls-websocket"

namespace octopus_mq {

using std::string, std::shared_ptr;

class adapter_settings;
using adapter_settings_parser_item = nlohmann::detail::iter_impl<const nlohmann::json>;
using adapter_settings_parser =
    std::map<string, void (*)(adapter_settings *, const adapter_settings_parser_item &)>;

class adapter_settings {
    phy _phy;
    port_int _port;
    protocol_type _protocol;
    string _name;
    bool _generated_name;
    nlohmann::json _json;

    static inline const std::map<protocol_type, string> _protocol_name = {
        { protocol_type::mqtt, "mqtt" }, { protocol_type::dds, "dds" }
    };

   public:
    adapter_settings(const protocol_type &protocol);
    adapter_settings(const protocol_type &protocol, const string &phy, const port_int &port);
    adapter_settings(const protocol_type &protocol, const nlohmann::json &json);

    void phy(const class phy &phy);
    void phy(const string &phy);
    void port(const port_int &port);
    void name(const string &name);
    void name_append(const string &appendix);  // Only works with generated names

    const class phy &phy() const;
    const port_int &port() const;
    const protocol_type &protocol() const;
    const string &name() const;
    const nlohmann::json &json() const;

    bool compare_binding(const ip_int ip, const port_int port) const;
    const string binging_name() const;
    const string &protocol_name() const;

    static const string &protocol_name(const protocol_type &protocol);
};

using adapter_settings_ptr = std::shared_ptr<adapter_settings>;

class message_queue;

class adapter_interface {
   protected:
    adapter_settings_ptr _adapter_settings;
    // Lifetime of message pool referenced by _global_msg_pool should be greater than lifetime of
    // adapter interface instance.
    message_queue &_global_queue;

   public:
    adapter_interface(const adapter_settings_ptr adapter_settings, message_queue &global_queue);

    virtual void run() = 0;
    virtual void stop() = 0;
    virtual void inject_publish(const message_ptr message) = 0;
};

using adapter_iface_ptr = std::shared_ptr<adapter_interface>;
using adapter_pool = std::vector<std::pair<adapter_settings_ptr, adapter_iface_ptr>>;
using adapter_message_pair = std::pair<adapter_settings_ptr, message_ptr>;

class message_queue {
    std::queue<adapter_message_pair> _queue;
    std::mutex _queue_mutex;
    std::condition_variable _queue_cv;

   public:
    void push(const adapter_settings_ptr adapter, const message_ptr message);
    bool wait_and_pop(std::chrono::milliseconds timeout, adapter_message_pair &destination);
    bool wait_and_pop_all(std::chrono::milliseconds timeout, adapter_pool &pool);
};

}  // namespace octopus_mq

#endif
