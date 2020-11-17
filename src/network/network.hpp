#ifndef OCTOMQ_PROTOCOL_H_
#define OCTOMQ_PROTOCOL_H_

#include <cstdint>
#include <string>
#include <vector>

namespace octopus_mq {

using std::string;

enum class protocol_type { mqtt, dds };

enum class transport_mode { unicast, multicast, broadcast };

enum class transport_type { udp, tcp, tls, websocket, tls_websocket };

enum class network_event_type { send, receive };

using port_int = uint32_t;

using ip_int = uint32_t;

using socket_int = int32_t;

using network_payload = std::vector<unsigned char>;

namespace network {

    namespace constants {

        constexpr ip_int null_ip = 0;
        constexpr ip_int loopback_ip = 0x0100007f;  // 127.0.0.1 in reverse byte order
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

        constexpr char unicast[] = "unicast";
        constexpr char multicast[] = "multicast";
        constexpr char broadcast[] = "broadcast";

    }  // namespace transport_mode_name

    namespace protocol_name {

        constexpr char mqtt[] = "mqtt";
        constexpr char dds[] = "dds";

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
    const ip_int &netmask() const;
    ip_int broadcast_address() const;
};

}  // namespace octopus_mq

#endif
