#ifndef OCTOMQ_MQTT_ADAPTER_H_
#define OCTOMQ_MQTT_ADAPTER_H_

#include <list>
#include <map>
#include <memory>
#include <string>

#include "network/adapter.hpp"
#include "network/network.hpp"

namespace octopus_mq::mqtt {

using std::string;

class adapter_settings : public octopus_mq::adapter_settings {
    transport_type _transport;
    address _remote_address;  // is used only when adapter is in client mode
    adapter_role _role;

    static inline const std::map<string, adapter_role> _role_from_name = {
        { adapter::role_name::broker, adapter_role::broker },
        { adapter::role_name::client, adapter_role::client }
    };

    static inline const std::map<string, transport_type> _transport_from_name = {
        { adapter::transport_name::tcp, transport_type::tcp },
        { adapter::transport_name::websocket, transport_type::websocket },
#ifdef OCTOMQ_ENABLE_TLS
        { adapter::transport_name::tls, transport_type::tls },
        { adapter::transport_name::tls_websocket, transport_type::tls_websocket }
#endif
    };

   public:
    adapter_settings(const nlohmann::json &json);

    void transport(const transport_type &transport);
    void transport(const string &transport);
    void role(const adapter_role &role);
    void role(const string &role);

    const transport_type &transport() const;
    const adapter_role &role() const;
};

using adapter_settings_ptr = std::shared_ptr<adapter_settings>;

}  // namespace octopus_mq::mqtt

#endif
