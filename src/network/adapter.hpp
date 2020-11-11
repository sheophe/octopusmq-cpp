#ifndef OCTOMQ_ADAPTER_H_
#define OCTOMQ_ADAPTER_H_

#include <list>
#include <map>
#include <string>
#include <thread>

#include "json.hpp"
#include "core/message_pool.hpp"
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
    std::unordered_map<string, void (*)(adapter_settings *, const adapter_settings_parser_item &)>;

class adapter_settings {
    phy _phy;
    port_int _port;
    protocol_type _protocol;
    string _name;
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

class adapter_interface {
   protected:
    adapter_settings_ptr _adapter_settings;
    // Lifetime of message pool referenced by _global_msg_pool should be greater than lifetime of
    // adapter interface instance.
    message_pool &_global_msg_pool;

   public:
    adapter_interface(const adapter_settings_ptr adapter_settings, message_pool &global_msg_pool);

    virtual void run() = 0;
    virtual void stop() = 0;
    virtual void inject_publish(const std::shared_ptr<message> message) = 0;
};

using adapter_iface_ptr = std::shared_ptr<adapter_interface>;
using adapter_pool = std::vector<std::pair<adapter_settings_ptr, adapter_iface_ptr>>;

}  // namespace octopus_mq

#endif
