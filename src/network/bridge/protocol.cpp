#include "network/bridge/protocol.hpp"

#include <algorithm>
#include <cstring>
#include <map>
#include <utility>

namespace octopus_mq::bridge::protocol::v1 {

static std::map<const packet_type, const std::string> packet_name_map = {
    { packet_type::probe, packet_name::probe },
    { packet_type::heartbeat, packet_name::heartbeat },
    { packet_type::subscribe, packet_name::subscribe },
    { packet_type::unsubscribe, packet_name::unsubscribe },
    { packet_type::publish, packet_name::publish },
    { packet_type::disconnect, packet_name::disconnect }
};

const std::string packet_name::from_type(const packet_type& type) {
    const packet_family family = packet::family(type);
    switch (family) {
        case packet_family::normal:
            return packet_name_map[type];
        case packet_family::ack:
            return packet_name::ack;
        case packet_family::nack:
            return packet_name::nack;
    }
}

opayload_stream::opayload_stream(network_payload& payload) : _payload(payload) {}

opayload_stream& opayload_stream::operator<<(const std::uint8_t& value) {
    _payload.push_back(static_cast<char>(value));
    return *this;
}

opayload_stream& opayload_stream::operator<<(const std::uint16_t& value) {
    _payload.push_back(static_cast<char>(value & 0xff));
    _payload.push_back(static_cast<char>((value >> 8) & 0xff));
    return *this;
}

opayload_stream& opayload_stream::operator<<(const std::uint32_t& value) {
    _payload.push_back(static_cast<char>(value & 0xff));
    _payload.push_back(static_cast<char>((value >> 8) & 0xff));
    _payload.push_back(static_cast<char>((value >> 16) & 0xff));
    _payload.push_back(static_cast<char>((value >> 24) & 0xff));
    return *this;
}

opayload_stream& opayload_stream::operator<<(const std::uint64_t& value) {
    _payload.push_back(static_cast<char>(value & 0xff));
    _payload.push_back(static_cast<char>((value >> 8) & 0xff));
    _payload.push_back(static_cast<char>((value >> 16) & 0xff));
    _payload.push_back(static_cast<char>((value >> 24) & 0xff));
    _payload.push_back(static_cast<char>((value >> 32) & 0xff));
    _payload.push_back(static_cast<char>((value >> 40) & 0xff));
    _payload.push_back(static_cast<char>((value >> 48) & 0xff));
    _payload.push_back(static_cast<char>((value >> 56) & 0xff));
    return *this;
}

opayload_stream& opayload_stream::operator<<(const version& value) {
    _payload.push_back(static_cast<char>(value));
    return *this;
}

opayload_stream& opayload_stream::operator<<(const packet_type& value) {
    _payload.push_back(static_cast<char>(value));
    return *this;
}

opayload_stream& opayload_stream::operator<<(const std::string& value) {
    std::copy(value.begin(), value.end(), std::back_inserter(_payload));
    _payload.push_back('\0');
    return *this;
}

ipayload_stream::ipayload_stream(const network_payload& payload)
    : _payload(payload), _iterator(_payload.begin()) {}

ipayload_stream& ipayload_stream::operator>>(std::uint8_t& value) {
    if (_iterator + sizeof(std::uint8_t) > _payload.end())
        throw ipayload_stream_out_of_range("uint8_t");
    value = static_cast<std::uint8_t>(*_iterator++);
    return *this;
}

ipayload_stream& ipayload_stream::operator>>(std::uint16_t& value) {
    if (_iterator + sizeof(std::uint16_t) > _payload.end())
        throw ipayload_stream_out_of_range("uint16_t");
    char* value_array = reinterpret_cast<char*>(&value);
    value_array[0] = (*_iterator++);
    value_array[1] = (*_iterator++);
    return *this;
}

ipayload_stream& ipayload_stream::operator>>(std::uint32_t& value) {
    if (_iterator + sizeof(std::uint32_t) > _payload.end())
        throw ipayload_stream_out_of_range("uint32_t");
    char* value_array = reinterpret_cast<char*>(&value);
    value_array[0] = (*_iterator++);
    value_array[1] = (*_iterator++);
    value_array[2] = (*_iterator++);
    value_array[3] = (*_iterator++);
    return *this;
}

ipayload_stream& ipayload_stream::operator>>(std::uint64_t& value) {
    if (_iterator + sizeof(std::uint64_t) > _payload.end())
        throw ipayload_stream_out_of_range("uint64_t");
    char* value_array = reinterpret_cast<char*>(&value);
    value_array[0] = (*_iterator++);
    value_array[1] = (*_iterator++);
    value_array[2] = (*_iterator++);
    value_array[3] = (*_iterator++);
    value_array[4] = (*_iterator++);
    value_array[5] = (*_iterator++);
    value_array[6] = (*_iterator++);
    value_array[7] = (*_iterator++);
    return *this;
}

ipayload_stream& ipayload_stream::operator>>(version& value) {
    if (_iterator + sizeof(version) > _payload.end()) throw ipayload_stream_out_of_range("version");
    value = static_cast<version>(*_iterator++);
    return *this;
}

ipayload_stream& ipayload_stream::operator>>(packet_type& value) {
    if (_iterator + sizeof(packet_type) > _payload.end())
        throw ipayload_stream_out_of_range("packet_type");
    value = static_cast<packet_type>(*_iterator++);
    return *this;
}

ipayload_stream& ipayload_stream::operator>>(std::string& value) {
    for (char c = *_iterator; c != 0 and _iterator < _payload.end(); c = *_iterator++)
        value.push_back(c);
    return *this;
}

void ipayload_stream::skip_header() { _iterator += constants::header_size; }

packet::packet(const packet_type& packet_type, const std::uint32_t& seq_n)
    : magic(constants::magic_number),
      version(version::v1),
      type(packet_type),
      sequence_number(seq_n) {
    opayload_stream ops(payload);
    ops << magic << version << type << sequence_number;
}

packet::packet(const packet_type& packet_type, network_payload_ptr payload)
    : magic(0), version(version::v1), type(packet_type) {
    std::uint8_t version_n;
    std::uint8_t type_n;
    ipayload_stream ips(*payload);
    ips >> magic >> version_n >> type_n >> sequence_number;
    if (magic != constants::magic_number) throw invalid_magic_number();
    if (version_n < static_cast<std::uint8_t>(min_version) or
        version_n > static_cast<std::uint8_t>(max_version))
        throw unsupported_version();
    version = static_cast<protocol::version>(version_n);
    if (sequence_number < constants::min_seq_n) throw invalid_sequence_number();
}

const std::string packet::type_name() const { return packet_name::from_type(type); }

packet_family packet::family(const packet_type& type) {
    return static_cast<packet_family>(static_cast<std::uint8_t>(type) & 0xf0);
}

packet_kind packet::kind(const packet_type& type) {
    return static_cast<packet_kind>(static_cast<std::uint8_t>(type) & 0xf0);
}

packet_type packet::type_from_payload(const network_payload_ptr& payload) {
    const std::uint8_t type_n =
        *reinterpret_cast<const std::uint8_t*>(payload->data() + constants::type_offset);
    if (type_n < constants::min_packet_type or type_n == constants::disconnect_nack_typeid or
        (((type_n & 0x0f) == static_cast<std::uint8_t>(packet_kind::probe)) and
         ((type_n & 0xf0) >> 4) != static_cast<std::uint8_t>(packet_family::normal)) or
        ((type_n & 0xf0) >> 4) > constants::max_packet_family or
        (type_n & 0x0f) < constants::min_packet_kind or
        (type_n & 0x0f) > constants::max_packet_kind)
        throw invalid_packet_type();
    return static_cast<packet_type>(type_n);
}

packet_type packet::nack_type(const packet_type& type) {
    switch (type) {
        case packet_type::heartbeat:
            return packet_type::heartbeat_nack;
        case packet_type::publish:
            return packet_type::publish_nack;
        case packet_type::subscribe:
            return packet_type::subscribe_nack;
        case packet_type::unsubscribe:
            return packet_type::unsubscribe_nack;
        default:
            throw protocol::nack_does_not_exist();
    }
}

probe::probe(const ip_int& ip, const port_int& port, const std::uint32_t& seq_n)
    : packet(packet_type::probe, seq_n), ip(ip), port(port) {
    opayload_stream ops(payload);
    ops << this->ip << this->port;
}

probe::probe(network_payload_ptr payload) : packet(packet_type::probe, payload) {
    ipayload_stream ips(*payload);
    ips.skip_header();
    ips >> ip >> port;
}

heartbeat::heartbeat(const discovered_nodes& nodes, const std::chrono::milliseconds& interval,
                     const std::uint32_t& seq_n)
    : packet(packet_type::heartbeat, seq_n),
      interval(static_cast<std::uint32_t>(interval.count())),
      nodes(nodes) {
    opayload_stream ops(payload);
    ops << this->interval << static_cast<std::uint32_t>(this->nodes.size());
    for (auto& node : this->nodes) ops << node.first << node.second;
}

heartbeat::heartbeat(network_payload_ptr payload) : packet(packet_type::heartbeat, payload) {
    ipayload_stream ips(*payload);
    ips.skip_header();
    std::uint32_t list_size;
    ips >> interval >> list_size;
    if (payload->size() < list_size + sizeof(std::uint32_t)) throw protocol::packet_too_small();
    while (list_size) {
        ip_int address;
        port_int port;
        ips >> address >> port;
        nodes.emplace(std::make_pair(address, port));
        --list_size;
    }
}

void subscription::add_topic(const std::string& topic) {
    if (scope::valid_topic(topic)) {
        if (topic.size() + 1 < sizeof(std::uint64_t))
            topic_names.emplace(topic);
        else {
            std::hash<std::string> hasher;
            topic_hashes.emplace(static_cast<std::uint64_t>(hasher(topic)));
        }
    } else if (scope::valid_topic_filter(topic))
        topic_names.emplace(topic);
}

void subscription::generate_payload() {
    opayload_stream ops(full_payload);
    ops << static_cast<std::uint32_t>(topic_hashes.size());
    for (auto& hash : topic_hashes) ops << hash;
    ops << static_cast<std::uint32_t>(topic_names.size());
    for (auto& name : topic_names) ops << name;
}

void subscription::parse_payload() {
    ipayload_stream ips(full_payload);
    std::uint32_t hashes_size;
    ips >> hashes_size;
    while (hashes_size) {
        std::uint64_t hash;
        ips >> hash;
        topic_hashes.emplace(hash);
        --hashes_size;
    }
    std::uint32_t names_size;
    ips >> names_size;
    while (names_size) {
        std::string name;
        ips >> name;
        topic_names.emplace(name);
        --names_size;
    }
}

subscribe_unsubscribe::subscribe_unsubscribe(const packet_type& packet_type,
                                             const std::uint32_t& seq_n,
                                             const std::uint32_t& sub_id)
    : packet(packet_type, seq_n), subscription_id(sub_id), total_blocks(0), block_n(0) {
    if (type != packet_type::subscribe and type != packet_type::unsubscribe)
        throw bridge_malformed_packet_constructed();
}

publish::publish(const std::uint32_t seq_n, const std::uint32_t pub_id)
    : packet(packet_type::publish, seq_n), publication_id(pub_id), total_blocks(0), block_n(0) {}

packet_ptr packet_factory::from_payload(const network_payload_ptr& payload,
                                        const std::size_t& size) {
    if (size < constants::header_size) throw packet_too_small();

    packet_type type = packet::type_from_payload(payload);
    switch (type) {
        case packet_type::probe:
            return std::make_unique<probe>(payload);
        case packet_type::heartbeat:
            return std::make_unique<heartbeat>(payload);
        case packet_type::heartbeat_ack:
        case packet_type::subscribe_ack:
        case packet_type::unsubscribe_ack:
        case packet_type::publish_ack:
        case packet_type::disconnect_ack:
            return std::make_unique<ack>(type, payload);
        case packet_type::heartbeat_nack:
        case packet_type::subscribe_nack:
        case packet_type::unsubscribe_nack:
        case packet_type::publish_nack:
            return std::make_unique<nack>(type, payload);
        default:
            return nullptr;
    }
}

}  // namespace octopus_mq::bridge::protocol::v1