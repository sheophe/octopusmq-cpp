#ifndef OCTOMQ_MQTT_ADAPTER_H_
#define OCTOMQ_MQTT_ADAPTER_H_

#include <list>
#include <map>
#include <string>

#include "network/adapter.hpp"
#include "network/network.hpp"

#define OCTOMQ_MQTT_ADAPTER_ROLE_BROKER "broker"
#define OCTOMQ_MQTT_ADAPTER_ROLE_CLIENT "client"

namespace octopus_mq {

using std::string;

enum class mqtt_adapter_role { broker, client };

enum class mqtt_version { v3, v5 };

class mqtt_adapter_settings : public adapter_settings {
    transport_type _transport;
    address _remote_address;  // is used only when adapter is in client mode
    mqtt_adapter_role _role;
    std::list<string> _scope;

    static inline const std::map<string, mqtt_adapter_role> _role_from_name = {
        { OCTOMQ_MQTT_ADAPTER_ROLE_BROKER, mqtt_adapter_role::broker },
        { OCTOMQ_MQTT_ADAPTER_ROLE_CLIENT, mqtt_adapter_role::client }
    };

    static inline const std::map<string, transport_type> _transport_from_name = {
        { OCTOMQ_ADAPTER_TRANSPORT_TCP, transport_type::tcp },
        { OCTOMQ_ADAPTER_TRANSPORT_TLS, transport_type::tls },
        { OCTOMQ_ADAPTER_TRANSPORT_WS, transport_type::websocket }
    };

   public:
    mqtt_adapter_settings();
    mqtt_adapter_settings(const nlohmann::json &json);

    void transport(const transport_type &transport);
    void transport(const string &transport);
    void role(const mqtt_adapter_role &role);
    void role(const string &role);
    void scope(const string &scope);
    void scope(const std::list<string> &scope);

    const transport_type &transport() const;
    const mqtt_adapter_role &role() const;
    const std::list<string> &scope() const;
};

}  // namespace octopus_mq

#endif
