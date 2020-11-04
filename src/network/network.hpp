#ifndef OCTOMQ_PROTOCOL_H_
#define OCTOMQ_PROTOCOL_H_

#include <cstdint>
#include <string>
#include <vector>

#define OCTOMQ_NULL_IP (0)
#define OCTOMQ_NULL_PORT (0)
#define OCTOMQ_PORT_MAX (65535)
#define OCTOMQ_NULL_SOCKET (-1)
#define OCTOMQ_SOCKET_MAXCONN (256)

namespace octopus_mq {

using std::string;

enum class protocol_type { mqtt, dds, stop };

enum class tx_type { unicast, multicast, broadcast };

enum class transport_type { udp, tcp, websocket };

enum class network_event_type { send, receive };

using port_int = uint32_t;

using ip_int = uint32_t;

using socket_int = int32_t;

using network_payload = std::vector<unsigned char>;

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
    const ip_int &netmask() const;
    ip_int broadcast_address() const;
};

class adapter_params {
   public:
    phy phy;
    port_int port;
    transport_type transport;
    protocol_type protocol;

    adapter_params();
};

}  // namespace octopus_mq

#endif
