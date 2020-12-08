#ifndef OCTOMQ_PROTOCOL_H_
#define OCTOMQ_PROTOCOL_H_

#include <cstdint>
#include <memory>
#include <string>
#include <vector>

namespace octopus_mq {

enum class protocol_type { dds, mqtt };
enum class transport_mode { unicast, multicast, broadcast };
enum class transport_type { udp, tcp, tls, websocket, tls_websocket };
enum class network_event_type { send, receive };
using port_int = std::uint16_t;
using ip_int = std::uint32_t;
using socket_int = std::int32_t;
using network_payload = std::vector<char>;
using network_payload_ptr = std::shared_ptr<network_payload>;
using network_payload_iter_pair =
    std::pair<network_payload::const_iterator, network_payload::const_iterator>;

namespace network {

    namespace constants {

        constexpr ip_int null_ip = 0;
        constexpr ip_int loopback_ip = 0x7f000001;       // 127.0.0.1
        constexpr ip_int loopback_net = 0x7f000000;      // 127.0.0.0
        constexpr ip_int loopback_netmask = 0xff000000;  // 255.0.0.0
        constexpr ip_int host_min_mask = 0x00000001;     // 0.0.0.1
        constexpr ip_int host_max_mask = 0xfffffffe;     // 255.255.255.254
        constexpr ip_int max_ip = 0xffffffff;
        constexpr port_int null_port = 0;
        constexpr port_int max_port = 65535;
        constexpr socket_int null_socket = -1;
        constexpr char any_interface_name[] = "*";

    }  // namespace constants

    namespace transport_name {

        constexpr char udp[] = "udp";
        constexpr char tcp[] = "tcp";
        constexpr char tls[] = "tls";
        constexpr char websocket[] = "websocket";
        constexpr char tls_websocket[] = "tls/websocket";

    }  // namespace transport_name

    namespace transport_mode_name {

        constexpr char broadcast[] = "broadcast";
        constexpr char multicast[] = "multicast";
        constexpr char unicast[] = "unicast";

    }  // namespace transport_mode_name

    namespace protocol_name {

        constexpr char bridge[] = "bridge";
        constexpr char dds[] = "dds";
        constexpr char mqtt[] = "mqtt";

    }  // namespace protocol_name

}  // namespace network

namespace ip {

    ip_int from_string(const std::string &ip_string);
    std::string to_string(const ip_int &ip);
    bool is_loopback(const ip_int &ip);

}  // namespace ip

class address {
    ip_int _ip;
    port_int _port;

   public:
    address();
    address(const ip_int &ip, const port_int &port);
    address(const std::string &ip, const port_int &port);
    explicit address(const std::string &address);

    void from_string(const std::string &address_string);
    void port(const port_int &port);
    void ip(const ip_int &ip);
    void ip(const std::string &ip);

    const port_int &port() const;
    const ip_int &ip() const;
    bool empty() const;
    std::string to_string() const;
};

class phy {
    ip_int _ip;
    ip_int _netmask;
    std::string _name;

    ip_int default_phy_ip();
    std::string phy_name();
    void phy_addresses();

   public:
    phy();
    explicit phy(const std::string &name);
    explicit phy(std::string &&name);
    explicit phy(const ip_int &ip);

    void name(const std::string &name);
    void name(std::string &&name);
    void ip(const ip_int &ip);

    const std::string &name() const;
    const ip_int &ip() const;
    const std::string ip_string() const;
    const std::string broadcast_string() const;

    const ip_int &netmask() const;
    ip_int net() const;
    ip_int wildcard() const;
    ip_int broadcast() const;
    ip_int host_min() const;
    ip_int host_max() const;
};

}  // namespace octopus_mq

#endif
