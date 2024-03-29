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
#define OCTOMQ_RECV_BUFFER_SIZE (65536)

namespace octopus_mq {

address::address() : _ip(network::constants::null_ip), _port(network::constants::null_port) {}

address::address(const ip_int &ip, const port_int &port) : _ip(ip), _port(port) {}

address::address(const string &ip, const port_int &port) : _ip(to_ip(ip)), _port(port) {}

address::address(const string &address) { address_string(address); }

ip_int address::to_ip(const string &ip_string) {
    struct in_addr ia;
    if (inet_pton(AF_INET, ip_string.c_str(), &ia))
        return (ip_int)ia.s_addr;
    else
        return network::constants::null_ip;
}

void address::address_string(const string &address_string) {
    size_t pos = address_string.find_last_of(':');
    if (pos != string::npos) {
        string ip = address_string.substr(0, pos);
        string port = address_string.substr(pos + 1);
        _ip = ip.empty() ? network::constants::null_ip : to_ip(ip);
        _port = port.empty() ? network::constants::null_port : stoi(port);
    } else if (address_string.size() >= sizeof("0.0.0.0")) {
        _ip = to_ip(address_string);
        _port = network::constants::null_port;
    } else if (address_string.size() <= sizeof("65535")) {
        _ip = network::constants::null_ip;
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

bool address::empty() const {
    return (_ip == network::constants::null_ip) and (_port == network::constants::null_port);
}

phy::phy() : _ip(network::constants::null_ip), _name(network::constants::any_interface) {}

phy::phy(const string &name) : _ip(network::constants::null_ip), _name(name) {
    if (_name != network::constants::any_interface) {
        phy_addresses();
        if (_ip == network::constants::null_ip)
            throw std::runtime_error("interface not found: " + _name);
    }
}

phy::phy(string &&name) : _ip(network::constants::null_ip), _name(std::move(name)) {
    if (_name != network::constants::any_interface) {
        phy_addresses();
        if (_ip == network::constants::null_ip)
            throw std::runtime_error("interface not found: " + _name);
    }
}

phy::phy(const ip_int &ip) : _ip(ip), _name(phy_name()) {}

string phy::phy_name() {
    ifaddrs *interface = nullptr;
    string name;
    if (getifaddrs(&interface) < 0) return name;
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

void phy::name(const string &name) {
    _name = name;
    if (_name == network::constants::any_interface) {
        _ip = network::constants::null_ip;
    } else {
        phy_addresses();
        if (_ip == network::constants::null_ip)
            throw std::runtime_error("interface not found: " + _name);
    }
}

void phy::name(string &&name) {
    _name = std::move(name);
    if (_name == network::constants::any_interface) {
        _ip = network::constants::null_ip;
    } else {
        phy_addresses();
        if (_ip == network::constants::null_ip)
            throw std::runtime_error("interface not found: " + _name);
    }
}

void phy::ip(const ip_int &ip) {
    _ip = ip;
    if (_ip != network::constants::null_ip) {
        _name = phy_name();
        if (_name.empty()) throw std::runtime_error("interface not found: " + ip_string());
        phy_addresses();
    }
}

const string &phy::name() const { return _name; }

const ip_int &phy::ip() const { return _ip; }

const string phy::ip_string() const {
    char addr_str[INET_ADDRSTRLEN];
    struct in_addr ia;
    memset(&ia, 0, sizeof(ia));
    ia.s_addr = (in_addr_t)_ip;
    if (inet_ntop(AF_INET, &ia, addr_str, INET_ADDRSTRLEN) != nullptr)
        return string(addr_str);
    else
        return string();
}

const ip_int &phy::netmask() const { return _netmask; }

ip_int phy::broadcast_address() const { return _ip | ~_netmask; }

}  // namespace octopus_mq
