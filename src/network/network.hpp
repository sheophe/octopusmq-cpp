#ifndef OCTOMQ_PROTOCOL_H_
#define OCTOMQ_PROTOCOL_H_

#include <cstdint>
#include <string>
#include <vector>

namespace octopus_mq {

using std::string;

enum class protocol_type { bridge, dds, mqtt };

enum class transport_mode { unicast, multicast, broadcast };

enum class transport_type { udp, tcp, tls, websocket, tls_websocket };

enum class network_event_type { send, receive };

using port_int = std::uint16_t;

using ip_int = std::uint32_t;

using socket_int = int32_t;

using network_payload = std::vector<char>;

using network_payload_ptr = std::shared_ptr<network_payload>;

namespace network {

    namespace constants {

        constexpr ip_int null_ip = 0;
        constexpr ip_int loopback_ip = 0x0100007f;  // 127.0.0.1 in reverse byte order
        constexpr ip_int loopback_ip_min = loopback_ip;
        constexpr ip_int loopback_ip_max = 0xfeffff7f;  // 127.255.255.254 in reverse byte order
        constexpr ip_int host_min_mask = 0x01000000;    // 0.0.0.1 in reverse byte order
        constexpr ip_int host_max_mask = 0xfeffffff;    // 255.255.255.254 in reverse byte order
        constexpr ip_int max_ip = 0xffffffff;
        constexpr port_int null_port = 0;
        constexpr port_int max_port = 65535;
        constexpr socket_int null_socket = -1;
        constexpr char any_interface[] = "*";

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

namespace mqtt {

    enum class version { v3, v5 };

    enum class adapter_role { broker, client };

    namespace adapter_role_name {

        constexpr char broker[] = "broker";
        constexpr char client[] = "client";

    }  // namespace adapter_role_name

}  // namespace mqtt

class address {
    ip_int _ip;
    port_int _port;
    void address_string(const string &address_string);

   public:
    address();
    address(const ip_int &ip, const port_int &port);
    address(const string &ip, const port_int &port);
    explicit address(const string &address);

    void port(const port_int &port);
    void ip(const ip_int &ip);
    void ip(const string &ip);

    static ip_int to_ip(const string &ip_string);
    static string ip_string(const ip_int &ip);
    static void increment_ip(ip_int &ip);
    static void decrement_ip(ip_int &ip);

    const port_int &port() const;
    const ip_int &ip() const;
    bool empty() const;
    string to_string() const;
};

class phy {
    ip_int _ip;
    ip_int _netmask;
    string _name;

    ip_int default_phy_ip();
    string phy_name();
    void phy_addresses();

   public:
    phy();
    explicit phy(const string &name);
    explicit phy(string &&name);
    explicit phy(const ip_int &ip);

    void name(const string &name);
    void name(string &&name);
    void ip(const ip_int &ip);

    const string &name() const;
    const ip_int &ip() const;
    const string ip_string() const;
    const string broadcast_string() const;

    const ip_int &netmask() const;
    ip_int net() const;
    ip_int wildcard() const;
    ip_int broadcast() const;
    ip_int host_min() const;
    ip_int host_max() const;
};

}  // namespace octopus_mq

#endif
