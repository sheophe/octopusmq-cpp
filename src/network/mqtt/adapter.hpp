#ifndef OCTOMQ_MQTT_ADAPTER_H_
#define OCTOMQ_MQTT_ADAPTER_H_

#include <list>
#include <map>
#include <memory>
#include <string>

#include "network/adapter.hpp"
#include "network/network.hpp"

namespace octopus_mq::mqtt {

class adapter_settings : public octopus_mq::adapter_settings {
    transport_type _transport;

#ifdef OCTOMQ_ENABLE_MQTT_CLIENT
    address _remote_address;
    adapter_role _role;

    static inline const std::map<std::string, adapter_role> _role_from_name = {
        { adapter_role_name::broker, adapter_role::broker },
        { adapter_role_name::client, adapter_role::client }
    };
#endif

    static inline const std::map<std::string, transport_type> _transport_from_name = {
        { network::transport_name::tcp, transport_type::tcp },
        { network::transport_name::websocket, transport_type::websocket },
#ifdef OCTOMQ_ENABLE_TLS
        { network::transport_name::tls, transport_type::tls },
        { network::transport_name::tls_websocket, transport_type::tls_websocket }
#endif
    };

   public:
    adapter_settings(const nlohmann::json &json);

    void transport(const transport_type &transport);
    void transport(const std::string &transport);

    const transport_type &transport() const;

#ifdef OCTOMQ_ENABLE_MQTT_CLIENT
    void role(const adapter_role &role);
    void role(const std::string &role);

    const adapter_role &role() const;
#endif
};

using adapter_settings_ptr = std::shared_ptr<adapter_settings>;

}  // namespace octopus_mq::mqtt

#endif
