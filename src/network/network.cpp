#include "network/network.hpp"

#include <arpa/inet.h>
#include <fcntl.h>
#include <ifaddrs.h>
#include <netdb.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>

#include <cstring>
#include <stdexcept>

#include "core/error.hpp"
#include "core/log.hpp"

#define OCTOMQ_MAX_PORT_STRLEN (6)
#define OCTOMQ_MAX_HOSTNAME_LEN (256)
#define OCTOMQ_IFACE_NAME_ANY ("*")
#define OCTOMQ_RECV_BUFFER_SIZE (65536)

namespace octopus_mq {

address::address() : _ip(OCTOMQ_NULL_IP), _port(OCTOMQ_NULL_PORT) {}

address::address(const ip_int &ip, const port_int &port) : _ip(ip), _port(port) {}

address::address(const string &ip, const port_int &port) : _ip(to_ip(ip)), _port(port) {}

address::address(const string &address) { address_string(address); }

ip_int address::to_ip(const string &ip_string) {
    struct in_addr ia;
    if (inet_pton(AF_INET, ip_string.c_str(), &ia))
        return (ip_int)ia.s_addr;
    else
        return OCTOMQ_NULL_IP;
}

void address::address_string(const string &address_string) {
    size_t pos = address_string.find_last_of(':');
    if (pos != string::npos) {
        string ip = address_string.substr(0, pos);
        string port = address_string.substr(pos + 1);
        _ip = to_ip(ip);
        _port = stoi(port);
    } else if (address_string.size() >= sizeof("0.0.0.0")) {
        _ip = to_ip(address_string);
        _port = OCTOMQ_NULL_PORT;
    } else if (address_string.size() <= sizeof("65535")) {
        _ip = OCTOMQ_NULL_IP;
        _port = stoi(address_string);
    }
}

void address::port(const port_int &port) { _port = port; }

void address::ip(const ip_int &ip) { _ip = ip; }

string address::to_string() const {
    char addr_str[INET_ADDRSTRLEN + 1 + OCTOMQ_MAX_PORT_STRLEN];
    struct in_addr ia;
    memset(&addr_str, 0, sizeof(addr_str));
    memset(&ia, 0, sizeof(ia));
    ia.s_addr = (in_addr_t)_ip;
    if (inet_ntop(AF_INET, &ia, addr_str, INET_ADDRSTRLEN) == nullptr)
        throw std::runtime_error("[socket] cannot convert address to string (ip conversion error)");
    size_t pos = strlen(addr_str);
    if (snprintf(addr_str + pos, sizeof(addr_str) - pos, ":%d", _port) < 0)
        throw std::runtime_error(
            "[socket] cannot convert address to string (port conversion error)");
    return string(addr_str);
}

const port_int &address::port() const { return _port; }

const ip_int &address::ip() const { return _ip; }

bool address::empty() const { return (_ip == OCTOMQ_NULL_IP) and (_port == OCTOMQ_NULL_PORT); }

phy::phy() : _ip(OCTOMQ_NULL_IP), _name("any") {}

phy::phy(const string &name) : _ip(OCTOMQ_NULL_IP), _name(name) {
    phy_addresses();
    if (_ip == OCTOMQ_NULL_IP) throw std::runtime_error("interface not found: " + _name);
}

phy::phy(string &&name) : _ip(OCTOMQ_NULL_IP), _name(std::move(name)) {
    phy_addresses();
    if (_ip == OCTOMQ_NULL_IP) throw std::runtime_error("interface not found: " + _name);
}

phy::phy(const ip_int &ip) : _ip(ip), _name(phy_name()) {}

string phy::phy_name() {
    ifaddrs *interface = nullptr;
    string name;
    if (getifaddrs(&interface) < 0) return OCTOMQ_NULL_IP;
    for (ifaddrs *if_iter = interface; if_iter != nullptr; if_iter = if_iter->ifa_next)
        if (if_iter->ifa_addr != nullptr and if_iter->ifa_name != nullptr and
            if_iter->ifa_addr->sa_family == AF_INET and
            _ip == (ip_int)((struct sockaddr_in *)if_iter->ifa_addr)->sin_addr.s_addr) {
            name = if_iter->ifa_name;
            break;
        }
    freeifaddrs(interface);
    return name;
}

void phy::phy_addresses() {
    ifaddrs *interface = nullptr;
    if (getifaddrs(&interface) == 0) {
        for (ifaddrs *if_iter = interface; if_iter != nullptr; if_iter = if_iter->ifa_next)
            if (if_iter->ifa_addr != nullptr and if_iter->ifa_name != nullptr and
                if_iter->ifa_addr->sa_family == AF_INET and _name.compare(if_iter->ifa_name) == 0) {
                _ip = ((struct sockaddr_in *)if_iter->ifa_addr)->sin_addr.s_addr;
                _netmask = ((struct sockaddr_in *)if_iter->ifa_netmask)->sin_addr.s_addr;
                break;
            }
    } else
        throw std::runtime_error("cannot read system network interface list.");
    freeifaddrs(interface);
}

ip_int phy::broadcast_address() const { return _ip | ~_netmask; }

void phy::name(const string &name) {
    _name = name;
    phy_addresses();
    if (_ip == OCTOMQ_NULL_IP) throw std::runtime_error("interface not found: " + _name);
}

void phy::name(string &&name) {
    _name = std::move(name);
    phy_addresses();
    if (_ip == OCTOMQ_NULL_IP) throw std::runtime_error("interface not found: " + _name);
}

void phy::ip(const ip_int &ip) {
    _ip = ip;
    _name = phy_name();
    if (_name.empty()) {
        char addr_str[INET_ADDRSTRLEN];
        struct in_addr ia;
        memset(&ia, 0, sizeof(ia));
        ia.s_addr = (in_addr_t)_ip;
        if (inet_ntop(AF_INET, &ia, addr_str, INET_ADDRSTRLEN) != nullptr)
            throw std::runtime_error("interface not found: " + string(addr_str));
    }
    phy_addresses();
}

const string &phy::name() const { return _name; }

const ip_int &phy::ip() const { return _ip; }

const ip_int &phy::netmask() const { return _netmask; }

socket::socket(const class phy &phy, const transport_type &transport, const port_int port)
    : _phy(phy),
      _transport(transport),
      _tx_type(tx_type::unicast),
      _socket(OCTOMQ_NULL_SOCKET),
      _address(_phy.ip(), port),
      _multicast_radius(0) {}

socket::socket(const class phy &phy, const transport_type &transport, const port_int port,
               const socket_int socket)
    : _phy(phy),
      _transport(transport),
      _tx_type(tx_type::unicast),
      _socket(OCTOMQ_NULL_SOCKET),
      _address(_phy.ip(), port),
      _multicast_radius(0) {}

socket::socket(const class phy &phy, const transport_type &transport,
               const address &multicast_group, const uint8_t &multicast_radius)
    : _phy(phy),
      _transport(transport),
      _tx_type(tx_type::multicast),
      _socket(OCTOMQ_NULL_SOCKET),
      _address(_phy.ip(), multicast_group.port()),
      _multicast_group(multicast_group),
      _multicast_radius(multicast_radius) {}

bool socket::operator==(const socket &b) const { return _socket == b._socket; }

bool socket::operator<(const socket &b) const { return _socket < b._socket; }

void socket::open() {
    if (_address.ip() == OCTOMQ_NULL_IP)
        throw std::runtime_error("[socket] device " + _phy.name() + " is not available.");
    switch (_transport) {
        case transport_type::udp: {
            _socket = ::socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
            if (_socket < 0)
                throw std::runtime_error("[socket] cannot open udp socket: " +
                                         std::to_string(_socket));
        } break;
        case transport_type::tcp:
        case transport_type::websocket: {
            _socket = ::socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
            if (_socket < 0)
                throw std::runtime_error("[socket] cannot open tcp socket: " +
                                         std::to_string(_socket));
        } break;
    }
    if (const int reuseaddr = 1;
        setsockopt(_socket, SOL_SOCKET, SO_REUSEADDR, &reuseaddr, sizeof(reuseaddr)) < 0) {
        ::close(_socket);
        throw std::runtime_error("[socket] cannot set option SO_REUSEADDR for socket.");
    }
    sockaddr_in addr;
    addr.sin_family = AF_INET;
    addr.sin_port = htons(_address.port());
    addr.sin_addr.s_addr = _address.ip();
    if (::bind(_socket, (sockaddr *)&addr, sizeof(addr)) < 0) {
        ::close(_socket);
        throw std::runtime_error("[socket] cannot bind socket.");
    }
    if (_transport == transport_type::udp) {
        switch (_tx_type) {
            case tx_type::multicast: {
                ip_mreq mreq;
                mreq.imr_interface.s_addr = _address.ip();
                mreq.imr_multiaddr.s_addr = _multicast_group.ip();
                if (setsockopt(_socket, IPPROTO_IP, IP_ADD_MEMBERSHIP, &mreq, sizeof(mreq)) < 0) {
                    ::close(_socket);
                    throw std::runtime_error(
                        "[socket] cannot set option IP_ADD_MEMBERSHIP for multicast socket.");
                }
                if (setsockopt(_socket, IPPROTO_IP, IP_MULTICAST_TTL, &_multicast_radius,
                               sizeof(_multicast_radius)) < 0) {
                    ::close(_socket);
                    throw std::runtime_error(
                        "[socket] cannot set option IP_MULTICAST_TTL for multicast socket.");
                }
            } break;
            case tx_type::broadcast: {
                if (const int bcast = 1;
                    setsockopt(_socket, SOL_SOCKET, SO_BROADCAST, &bcast, sizeof(bcast)) < 0) {
                    ::close(_socket);
                    throw std::runtime_error(
                        "[socket] cannot set option SO_BROADCAST for broadcast socket.");
                }
            } break;
            default:
                break;
        }
    }
    if (int flags = fcntl(_socket, F_GETFL, 0); flags < 0)
        throw std::runtime_error("[socket] cannot read socket flags.");
    else if (fcntl(_socket, F_SETFL, flags | O_NONBLOCK) < 0)
        throw std::runtime_error("[socket] cannot set socket to non-blocking mode.");
}

void socket::listen() {
    if (_transport == transport_type::tcp) {
        if (::listen(_socket, OCTOMQ_SOCKET_MAXCONN) <= 0)
            throw std::runtime_error("[socket] listen failed on socket " + std::to_string(_socket));
    } else
        throw std::runtime_error("[socket] listen is only possible on tcp sockets.");
}

std::shared_ptr<socket> socket::accept(const uint32_t timeout) {
    if (_transport == transport_type::tcp or _transport == transport_type::websocket) {
        fd_set readset;

        sockaddr_storage sa;
        socklen_t len = sizeof(sa);
        socket_int accepted_socket = OCTOMQ_NULL_SOCKET;
        if (accepted_socket = ::accept(_socket, (sockaddr *)&sa, &len); accepted_socket < 0)
            throw std::runtime_error("[socket] accept failed on socket " + std::to_string(_socket));
        return std::make_shared<socket>(_phy, _transport, _address.port(), accepted_socket);
    } else
        throw std::runtime_error("[socket] accept is only possible on tcp sockets.");
}

void socket::connect(const address &remote_address) {
    if (_transport == transport_type::tcp) {
        sockaddr_in addr;
        addr.sin_family = AF_INET;
        addr.sin_port = htons(remote_address.port());
        addr.sin_addr.s_addr = remote_address.ip();
        if (::connect(_socket, (sockaddr *)&addr, sizeof(addr)) < 0)
            throw std::runtime_error("[socket] connect failed on socket " +
                                     std::to_string(_socket));
    }
    _connected = true;
}

ssize_t socket::recv(network_payload &payload) {
    uint8_t recv_buffer[OCTOMQ_RECV_BUFFER_SIZE];
    ::recv(_socket, (void *)recv_buffer, OCTOMQ_RECV_BUFFER_SIZE, 0);
}

ssize_t socket::send_tcp(const network_payload &payload) {
    if (_transport == transport_type::udp) throw invalid_invocation("send_tcp()");
    return ::send(_socket, (const void *)payload.data(), payload.size(), 0);
}

ssize_t socket::send_udp(const address &remote_address, const network_payload &payload) {
    if (_transport != transport_type::udp) throw invalid_invocation("send_udp()");
    sockaddr_in dest;
    dest.sin_family = AF_INET;
    dest.sin_port = htons(remote_address.port());
    dest.sin_addr.s_addr = remote_address.ip();
    return ::sendto(_socket, (const void *)payload.data(), payload.size(), 0,
                    (const sockaddr *)&dest, sizeof(dest));
}

void socket::close() {
    if (_socket > 0) {
        ::close(_socket);
        _socket = OCTOMQ_NULL_SOCKET;
        _connected = false;
    }
}

const phy &socket::phy() const { return _phy; }

const transport_type &socket::transport() const { return _transport; }

const address &socket::this_address() const { return _address; }

const bool &socket::is_connected() const { return _connected; }

adapter_params::adapter_params()
    : phy(),
      port(OCTOMQ_NULL_PORT),
      transport(transport_type::tcp),
      protocol(protocol_type::mqtt) {}

}  // namespace octopus_mq
