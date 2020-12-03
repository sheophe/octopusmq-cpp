#include "network/bridge/adapter.hpp"

#include "core/error.hpp"
#include "core/log.hpp"

namespace octopus_mq::bridge {

adapter_settings::adapter_settings(const nlohmann::json& json)
    : octopus_mq::adapter_settings(protocol_type::bridge, json) {
    // Parsing discovery mode field
    if (not json.contains(adapter::field_name::mode))
        throw missing_field_error(adapter::field_name::mode);

    const nlohmann::json& mode_field = json[adapter::field_name::mode];
    std::string mode_name;
    if (mode_field.is_string()) {
        mode_name = mode_field.get<std::string>();

        if (mode_name == network::transport_mode_name::multicast) {
            _transport_mode = transport_mode::multicast;

            // Parsing multicast group
            if (not json.contains(adapter::field_name::group))
                throw missing_field_error(adapter::field_name::group);

            const nlohmann::json& group_field = json[adapter::field_name::group];
            if (not group_field.is_string()) throw field_type_error(adapter::field_name::group);

            // Parsing multicast TTL
            if (not json.contains(adapter::field_name::hops))
                throw missing_field_error(adapter::field_name::hops);

            const nlohmann::json& hops_field = json[adapter::field_name::hops];
            if (not hops_field.is_number_unsigned())
                throw field_type_error(adapter::field_name::hops);

            _multicast_hops = hops_field.get<std::uint8_t>();

            address multicast_group = address(group_field.get<std::string>());
            if (multicast_group.port() != network::constants::null_port)
                _polycast_address = multicast_group;
            else
                _polycast_address = address(multicast_group.ip(), port());

        } else if (mode_name == network::transport_mode_name::broadcast) {
            _transport_mode = transport_mode::broadcast;
            _multicast_hops = 0;
            _polycast_address = address(phy().broadcast(), port());
        } else
            throw unknown_transport_mode_error(mode_name);

        // Parsing optional send_port field
        if (json.contains(adapter::field_name::send_port)) {
            const nlohmann::json& send_port_field = json[adapter::field_name::send_port];

            if (not send_port_field.is_number_unsigned())
                throw field_type_error(adapter::field_name::send_port);

            _send_port = send_port_field.get<std::uint16_t>();
        }
        // By default send_port will be equal to listening port.
        // If that happens on loopback interface, the bridge is recursive
        else if (ip::is_loopback(phy().ip()))
            throw bridge_recursive_config();
        else
            _send_port = port();
    } else
        throw field_type_error(adapter::field_name::mode);

    name_append("(udp " + mode_name + ')');

    // Parsing optional timeouts field
    std::chrono::milliseconds delay_time = adapter::default_timeouts::delay;
    std::chrono::milliseconds discovery_timeout = adapter::default_timeouts::discovery;
    std::chrono::milliseconds acknowledge_timeout = adapter::default_timeouts::acknowledge;
    std::chrono::milliseconds heartbeat_timeout = adapter::default_timeouts::heartbeat;
    std::chrono::milliseconds rescan_timeout = adapter::default_timeouts::rescan;

    if (json.contains(adapter::field_name::timeouts)) {
        const nlohmann::json timeouts_field = json[adapter::field_name::timeouts];
        if (not timeouts_field.is_object()) throw field_type_error(adapter::field_name::timeouts);

        // Parsing optional timeouts.delay field
        if (timeouts_field.contains(adapter::field_name::delay)) {
            const nlohmann::json delay_time_field = timeouts_field[adapter::field_name::delay];

            if (not delay_time_field.is_number_unsigned())
                throw field_type_error(adapter::field_name::timeouts, adapter::field_name::delay);

            std::uint32_t delay_int = delay_time_field.get<std::uint32_t>();
            delay_time = std::chrono::milliseconds(delay_int);
        }

        // Parsing optional timeouts.discovery field
        if (timeouts_field.contains(adapter::field_name::discovery)) {
            const nlohmann::json discovery_timeout_field =
                timeouts_field[adapter::field_name::discovery];

            if (not discovery_timeout_field.is_number_unsigned())
                throw field_type_error(adapter::field_name::timeouts,
                                       adapter::field_name::discovery);

            std::uint32_t discovery_int = discovery_timeout_field.get<std::uint32_t>();
            discovery_timeout = std::chrono::milliseconds(discovery_int);
        }

        // Parsing optional timeouts.acknowledge field
        if (timeouts_field.contains(adapter::field_name::acknowledge)) {
            const nlohmann::json acknowledge_timeout_field =
                timeouts_field[adapter::field_name::acknowledge];

            if (not acknowledge_timeout_field.is_number_unsigned())
                throw field_type_error(adapter::field_name::timeouts,
                                       adapter::field_name::acknowledge);

            std::uint32_t acknowledge_int = acknowledge_timeout_field.get<std::uint32_t>();
            acknowledge_timeout = std::chrono::milliseconds(acknowledge_int);
        }

        // Parsing optional timeouts.heartbeat field
        if (timeouts_field.contains(adapter::field_name::heartbeat)) {
            const nlohmann::json heartbeat_timeout_field =
                timeouts_field[adapter::field_name::heartbeat];

            if (not heartbeat_timeout_field.is_number_unsigned())
                throw field_type_error(adapter::field_name::timeouts,
                                       adapter::field_name::heartbeat);

            std::uint32_t heartbeat_int = heartbeat_timeout_field.get<std::uint32_t>();
            heartbeat_timeout = std::chrono::milliseconds(heartbeat_int);
        }

        // Parsing optional timeouts.rescan field
        if (timeouts_field.contains(adapter::field_name::rescan)) {
            const nlohmann::json rescan_timeout_field = timeouts_field[adapter::field_name::rescan];

            if (not rescan_timeout_field.is_number_unsigned())
                throw field_type_error(adapter::field_name::timeouts, adapter::field_name::rescan);

            std::uint32_t rescan_int = rescan_timeout_field.get<std::uint32_t>();
            rescan_timeout = std::chrono::milliseconds(rescan_int);
        }
    }

    timeouts(delay_time, discovery_timeout, acknowledge_timeout, heartbeat_timeout, rescan_timeout);
}

void adapter_settings::timeouts(const std::chrono::milliseconds& delay,
                                const std::chrono::milliseconds& discovery,
                                const std::chrono::milliseconds& acknowledge,
                                const std::chrono::milliseconds& heartbeat,
                                const std::chrono::milliseconds& rescan) {
    _timeouts.delay = delay;
    _timeouts.discovery = discovery;
    _timeouts.acknowledge = acknowledge;
    _timeouts.heartbeat = heartbeat;
    _timeouts.rescan = rescan;
}

const bridge::timeouts& adapter_settings::timeouts() const { return _timeouts; }

const enum transport_mode& adapter_settings::transport_mode() const { return _transport_mode; }

const address& adapter_settings::polycast_address() const { return _polycast_address; }

const std::uint8_t& adapter_settings::multicast_hops() const { return _multicast_hops; }

const port_int& adapter_settings::send_port() const { return _send_port; }

}  // namespace octopus_mq::bridge