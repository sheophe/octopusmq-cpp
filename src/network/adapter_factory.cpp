#include "network/adapter_factory.hpp"

#include "core/error.hpp"
#include "threads/bridge/bridge.hpp"
#include "threads/mqtt/broker.hpp"
#ifdef OCTOMQ_ENABLE_DDS
#include "threads/dds/peer.hpp"
#endif

namespace octopus_mq {

static inline const std::map<string, protocol_type> _protocol_from_name = {
    { network::protocol_name::bridge, protocol_type::bridge },
    { network::protocol_name::dds, protocol_type::dds },
    { network::protocol_name::mqtt, protocol_type::mqtt }
};

adapter_settings_ptr adapter_settings_factory::from_json(const nlohmann::json &json) {
    if (not json.contains(adapter::field_name::protocol))
        throw missing_field_error(adapter::field_name::protocol);

    const nlohmann::json &item = json[adapter::field_name::protocol];
    if (item.is_string()) {
        string protocol_name = item.get<string>();
        if (auto iter = _protocol_from_name.find(protocol_name);
            iter != _protocol_from_name.end()) {
            const protocol_type protocol = iter->second;
            // Calling protocol-specific constructor
            switch (protocol) {
                case protocol_type::mqtt:
                    return std::make_shared<mqtt::adapter_settings>(json);
                case protocol_type::bridge:
                    return std::make_shared<bridge::adapter_settings>(json);
                case protocol_type::dds:
#ifdef OCTOMQ_ENABLE_DDS
                    return std::make_shared<dds::adapter_settings>(json);
#else
                    throw unknown_protocol_error(protocol_name);
#endif
            }
        } else
            throw unknown_protocol_error(protocol_name);
    } else
        throw field_type_error(adapter::field_name::protocol);
}

adapter_iface_ptr adapter_interface_factory::from_settings(adapter_settings_ptr settings,
                                                           message_queue &message_queue) {
    if (settings == nullptr) throw adapter_not_initialized();

    // Protocol is checked in adapter_settings_factory.
    // Only adapter with valid protocols are stored in settings
    switch (settings->protocol()) {
        case protocol_type::mqtt: {
            mqtt::adapter_settings_ptr mqtt_settings =
                std::static_pointer_cast<mqtt::adapter_settings>(settings);

            switch (mqtt_settings->transport()) {
                case transport_type::tcp:
                    return std::make_shared<mqtt::broker<mqtt_cpp::server<>>>(settings,
                                                                              message_queue);
                case transport_type::websocket:
                    return std::make_shared<mqtt::broker<mqtt_cpp::server_ws<>>>(settings,
                                                                                 message_queue);
#ifdef OCTOMQ_ENABLE_TLS
                case transport_type::tls:
                    return std::make_shared<mqtt::broker<mqtt_cpp::server_tls<>>>(settings,
                                                                                  message_queue);
                case transport_type::tls_websocket:
                    return std::make_shared<mqtt::broker<mqtt_cpp::server_tls_ws<>>>(settings,
                                                                                     message_queue);
#else
                case transport_type::tls:
                case transport_type::tls_websocket:
#endif
                case transport_type::udp:
                    throw adapter_transport_error(settings->name(), settings->protocol_name());
            }
        };
        case protocol_type::bridge:
            return std::make_shared<bridge::implementation>(settings, message_queue);
        case protocol_type::dds:
#ifdef OCTOMQ_ENABLE_DDS
            return std::make_shared<dds::peer>(settings, message_queue);
#else
            return nullptr;
#endif
    }
}

}  // namespace octopus_mq