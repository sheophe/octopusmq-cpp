#ifndef OCTOMQ_BRIDGE_ADAPTER_H_
#define OCTOMQ_BRIDGE_ADAPTER_H_

#include <chrono>
#include <map>
#include <memory>
#include <string>
#include <variant>
#include <vector>

#include "network/adapter.hpp"
#include "network/network.hpp"

namespace octopus_mq::bridge {

namespace adapter {

    namespace field_name {

        constexpr char discovery[] = "discovery";
        constexpr char mode[] = "mode";
        constexpr char from[] = "from";
        constexpr char to[] = "to";
        constexpr char endpoints[] = "endpoints";
        constexpr char group[] = "group";
        constexpr char hops[] = "hops";
        constexpr char send_port[] = "send_port";
        constexpr char delay[] = "delay";
        constexpr char timeouts[] = "timeouts";
        constexpr char acknowledge[] = "acknowledge";
        constexpr char heartbeat[] = "heartbeat";
        constexpr char rescan[] = "rescan";

    }  // namespace field_name

    namespace default_timeouts {

        using namespace std::chrono_literals;

        constexpr std::chrono::milliseconds delay = 100ms;
        constexpr std::chrono::milliseconds discovery = 10000ms;
        constexpr std::chrono::milliseconds acknowledge = 1000ms;
        constexpr std::chrono::milliseconds heartbeat = 60000ms;
        constexpr std::chrono::milliseconds rescan = 60000ms;

    }  // namespace default_timeouts

}  // namespace adapter

enum class discovery_endpoints_format { list, range };

using discovery_list = std::vector<ip_int>;

class discovery_range {
   public:
    discovery_range();

    ip_int from;
    ip_int to;
};

using discovery_endpoints = std::variant<discovery_list, discovery_range>;

class discovery_settings {
   public:
    transport_mode mode;
    discovery_endpoints_format format;
    discovery_endpoints endpoints;
};

class timeouts {
   public:
    std::chrono::milliseconds delay;
    std::chrono::milliseconds discovery;
    std::chrono::milliseconds acknowledge;
    std::chrono::milliseconds heartbeat;
    std::chrono::milliseconds rescan;
};

class adapter_settings : public octopus_mq::adapter_settings {
    discovery_settings _discovery_settings;
    timeouts _timeouts;
    transport_mode _transport_mode;
    address _polycast_address;
    std::uint8_t _multicast_hops;
    port_int _send_port;

   public:
    adapter_settings(const nlohmann::json& json);

    // This call will pass discovery settings for unicast and set the mode.
    // Default discovery mode is broadcast.
    void discovery(const discovery_endpoints_format& format, const discovery_endpoints& endpoints);
    void timeouts(const std::chrono::milliseconds& delay,
                  const std::chrono::milliseconds& discovery,
                  const std::chrono::milliseconds& acknowledge,
                  const std::chrono::milliseconds& heartbeat,
                  const std::chrono::milliseconds& rescan);

    const discovery_settings& discovery() const;
    const class timeouts& timeouts() const;
    const transport_mode& transport_mode() const;
    const address& polycast_address() const;
    const std::uint8_t& multicast_hops() const;
    const port_int& send_port() const;
};

using adapter_settings_ptr = std::shared_ptr<bridge::adapter_settings>;

}  // namespace octopus_mq::bridge

#endif