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
};

using adapter_settings_ptr = std::shared_ptr<adapter_settings>;

}  // namespace octopus_mq::mqtt

#endif
