#include "network/bridge/adapter.hpp"

#include "core/error.hpp"
#include "core/log.hpp"

namespace octopus_mq::bridge {

discovery_range::discovery_range()
    : from(network::constants::null_ip), to(network::constants::null_ip) {}

adapter_settings::adapter_settings(const nlohmann::json& json)
    : octopus_mq::adapter_settings(protocol_type::bridge, json) {
    _discovery_settings.mode = transport_mode::broadcast;
    // Parse discovery field
    if (not json.contains(adapter::field_name::discovery))
        throw missing_field_error(adapter::field_name::discovery);

    const nlohmann::json discovery_field = json[adapter::field_name::discovery];
    if (not discovery_field.is_object()) throw field_type_error(adapter::field_name::discovery);

    // Parsing discovery mode
    if (not discovery_field.contains(adapter::field_name::mode))
        throw missing_field_error(adapter::field_name::discovery, adapter::field_name::mode);

    const nlohmann::json& mode_field = discovery_field[adapter::field_name::mode];
    if (mode_field.is_string()) {
        std::string mode_name = mode_field.get<string>();

        if (mode_name == network::transport_mode_name::unicast) {
            // Parsing endpoints for unicast discovery mode
            discovery_endpoints_format format = discovery_endpoints_format::list;
            discovery_endpoints endpoints;

            if (discovery_field.contains(adapter::field_name::endpoints)) {
                // Parsing discovery endpoint list
                const nlohmann::json& endpoints_field =
                    discovery_field[adapter::field_name::endpoints];

                if (not endpoints_field.is_array())
                    throw field_type_error(adapter::field_name::discovery,
                                           adapter::field_name::endpoints);

                std::vector<string> endpoints_strvec = endpoints_field.get<std::vector<string>>();

                // Variable 'format' is initialized as 'list', so no need to set it here again
                endpoints = discovery_list();
                for (auto& endpoint_string : endpoints_strvec) {
                    address addr(endpoint_string);
                    if (addr.ip() == network::constants::null_ip)
                        throw invalid_bridge_endpoint(endpoint_string);
                    std::get<discovery_list>(endpoints).push_back(addr.ip());
                }
            } else if (discovery_field.contains(adapter::field_name::from) and
                       discovery_field.contains(adapter::field_name::to)) {
                const nlohmann::json& from_field = discovery_field[adapter::field_name::from];
                const nlohmann::json& to_field = discovery_field[adapter::field_name::to];

                if (not from_field.is_string())
                    throw field_type_error(adapter::field_name::discovery,
                                           adapter::field_name::from);
                if (not to_field.is_string())
                    throw field_type_error(adapter::field_name::discovery, adapter::field_name::to);

                string from_string = from_field.get<string>();
                address from_addr(from_string);
                const ip_int& from_ip = from_addr.ip();
                if (from_ip == network::constants::null_ip)
                    throw invalid_bridge_endpoint(from_string);

                string to_string = to_field.get<string>();
                address to_addr(to_string);
                const ip_int& to_ip = to_addr.ip();
                if (to_ip == network::constants::null_ip) throw invalid_bridge_endpoint(to_string);

                // Currently only supporting endpoints from the same subnet
                const ip_int from_subnet = from_ip & network::constants::extract_subnets;
                const ip_int to_subnet = to_ip & network::constants::extract_subnets;
                if (from_subnet != to_subnet) throw bridge_range_different_subnets();

                const ip_int from_msb = from_ip & network::constants::extract_msb;
                const ip_int to_msb = to_ip & network::constants::extract_msb;
                if (to_msb < from_msb) throw invalid_bridge_range();

                format = discovery_endpoints_format::range;
                endpoints = discovery_range();
                std::get<discovery_range>(endpoints).from = from_ip;
                std::get<discovery_range>(endpoints).to = to_ip;
            } else
                throw bridge_discovery_not_set();

            this->discovery(format, endpoints);
        }
        // Bridge only supports unicast or broadcast
        else if (mode_name != network::transport_mode_name::broadcast)
            throw invalid_bridge_discovery_mode(mode_name);
    } else
        throw field_type_error(adapter::field_name::discovery, adapter::field_name::mode);

    // Parsing optional timeouts field
    std::chrono::milliseconds discovery_timeout = adapter::default_timeouts::discovery;
    std::chrono::milliseconds acknowledge_timeout = adapter::default_timeouts::acknowledge;
    std::chrono::milliseconds heartbeat_timeout = adapter::default_timeouts::heartbeat;

    if (json.contains(adapter::field_name::timeouts)) {
        const nlohmann::json timeouts_field = json[adapter::field_name::timeouts];
        if (not timeouts_field.is_object()) throw field_type_error(adapter::field_name::timeouts);

        // Parsing optional timeouts.discovery field
        if (timeouts_field.contains(adapter::field_name::discovery)) {
            const nlohmann::json discovery_timeout_field =
                timeouts_field[adapter::field_name::discovery];

            if (not discovery_timeout_field.is_number_integer())
                throw field_type_error(adapter::field_name::timeouts,
                                       adapter::field_name::discovery);

            uint32_t discovery_int = discovery_timeout_field.get<uint32_t>();
            discovery_timeout = std::chrono::milliseconds(discovery_int);
        }

        // Parsing optional timeouts.acknowledge field
        if (timeouts_field.contains(adapter::field_name::acknowledge)) {
            const nlohmann::json acknowledge_timeout_field =
                timeouts_field[adapter::field_name::acknowledge];

            if (not acknowledge_timeout_field.is_number_integer())
                throw field_type_error(adapter::field_name::timeouts,
                                       adapter::field_name::acknowledge);

            uint32_t acknowledge_int = acknowledge_timeout_field.get<uint32_t>();
            acknowledge_timeout = std::chrono::milliseconds(acknowledge_int);
        }

        // Parsing optional timeouts.heartbeat field
        if (timeouts_field.contains(adapter::field_name::heartbeat)) {
            const nlohmann::json heartbeat_timeout_field =
                timeouts_field[adapter::field_name::heartbeat];

            if (not heartbeat_timeout_field.is_number_integer())
                throw field_type_error(adapter::field_name::timeouts,
                                       adapter::field_name::heartbeat);

            uint32_t heartbeat_int = heartbeat_timeout_field.get<uint32_t>();
            heartbeat_timeout = std::chrono::milliseconds(heartbeat_int);
        }
    }

    timeouts(discovery_timeout, acknowledge_timeout, heartbeat_timeout);
}

void adapter_settings::discovery(const discovery_endpoints_format& format,
                                 const discovery_endpoints& endpoints) {
    _discovery_settings.mode = transport_mode::unicast;
    _discovery_settings.format = format;
    _discovery_settings.endpoints = endpoints;
}

void adapter_settings::timeouts(const std::chrono::milliseconds& discovery,
                                const std::chrono::milliseconds& acknowledge,
                                const std::chrono::milliseconds& heartbeat) {
    _timeouts.discovery = discovery;
    _timeouts.acknowledge = acknowledge;
    _timeouts.heartbeat = heartbeat;
}

const discovery_settings& adapter_settings::discovery() const { return _discovery_settings; }

const bridge::timeouts& adapter_settings::timeouts() const { return _timeouts; }

}  // namespace octopus_mq::bridge