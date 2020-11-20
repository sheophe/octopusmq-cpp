#include "network/bridge/protocol.hpp"

#include <algorithm>
#include <cstring>
#include <map>

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

packet::packet(const packet_type& packet_type, const std::uint32_t seq_n)
    : valid(false),
      magic(constants::magic_number),
      version(version::v1),
      type(packet_type),
      sequence_number(seq_n),
      payload(constants::header_size) {
    const std::uint8_t u8_version = static_cast<std::uint8_t>(version);
    const std::uint8_t u8_type = static_cast<std::uint8_t>(type);
    std::memcpy(payload.data() + constants::magic_offset, &magic, constants::magic_size);
    std::memcpy(payload.data() + constants::version_offset, &u8_version, constants::version_size);
    std::memcpy(payload.data() + constants::type_offset, &u8_type, constants::type_size);
    std::memcpy(payload.data() + constants::seq_n_offset, &sequence_number, constants::seq_n_size);
}

packet::packet(const packet_type& packet_type, network_payload_ptr payload)
    : valid(false), magic(0), version(version::v1), type(packet_type) {
    const char* data = payload->data();

    magic = *reinterpret_cast<const std::uint32_t*>(data);
    if (magic != constants::magic_number) throw invalid_magic_number();

    const std::uint8_t version_n =
        *reinterpret_cast<const std::uint8_t*>(data + constants::version_offset);
    if (version_n < static_cast<std::uint8_t>(min_version) or
        version_n > static_cast<std::uint8_t>(max_version))
        throw unsupported_version();
    version = static_cast<protocol::version>(version_n);

    sequence_number = *reinterpret_cast<const std::uint32_t*>(data + constants::seq_n_offset);
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
    if (type_n < constants::min_packet_type or type_n == constants::probe_nack_typeid or
        type_n == constants::disconnect_nack_typeid or
        ((type_n & 0xf0) >> 4) > constants::max_packet_family or
        (type_n & 0x0f) < constants::min_packet_kind or
        (type_n & 0x0f) > constants::max_packet_kind)
        throw invalid_packet_type();
    return static_cast<packet_type>(type_n);
}

void subscription::serialize(network_payload& payload) const {
    char header[sizeof(std::uint32_t) + constants::subscription_flags_size];
    std::uint32_t size = this->size();
    std::memcpy(header, &size, sizeof(std::uint32_t));
    std::memcpy(header + sizeof(std::uint32_t), &qos, sizeof(std::uint8_t));

    std::copy(header, header + sizeof(header), std::back_inserter(payload));
    std::copy(topic_filter.begin(), topic_filter.end(), std::back_inserter(payload));
}

std::uint32_t subscription::deserialize(const network_payload::const_iterator begin) {
    const std::uint32_t size = *reinterpret_cast<const std::uint32_t*>(begin.base());
    qos = *reinterpret_cast<const std::uint8_t*>(begin.base() + sizeof(size));
    topic_filter = std::string(begin + sizeof(size) + sizeof(qos), begin + sizeof(size) + size);
    return size + sizeof(size);
}

void publication::serialize(network_payload& payload) const {
    char header[sizeof(std::uint32_t) + constants::publication_flags_size];
    std::uint32_t size = this->size();
    std::memcpy(header, &size, sizeof(size));
    std::memcpy(header + sizeof(size), &qos, sizeof(qos));
    std::memcpy(header + sizeof(size) + sizeof(qos), &origin_port, sizeof(origin_port));
    std::memcpy(header + sizeof(size) + sizeof(qos) + sizeof(origin_port), &origin_ip,
                sizeof(origin_ip));

    std::copy(header, header + sizeof(header), std::back_inserter(payload));
    std::copy(origin_clid.begin(), origin_clid.end(), std::back_inserter(payload));
    payload.push_back(constants::null_terminator);
    std::copy(topic.begin(), topic.end(), std::back_inserter(payload));
    payload.push_back(constants::null_terminator);
    std::copy(message->begin(), message->end(), std::back_inserter(payload));
}

std::uint32_t publication::deserialize(const network_payload::const_iterator begin) {
    const std::uint32_t size = *reinterpret_cast<const std::uint32_t*>(begin.base());
    qos = *reinterpret_cast<const std::uint8_t*>(begin.base() + sizeof(size));
    origin_port =
        *reinterpret_cast<const std::uint16_t*>(begin.base() + sizeof(size) + sizeof(qos));
    origin_ip = *reinterpret_cast<const std::uint32_t*>(begin.base() + sizeof(size) + sizeof(qos) +
                                                        sizeof(origin_port));
    origin_clid = std::string(begin.base() + sizeof(size) + sizeof(qos) + sizeof(origin_port) +
                              sizeof(origin_ip));
    topic = std::string(begin.base() + sizeof(size) + sizeof(qos) + sizeof(origin_port) +
                        sizeof(origin_ip) + sizeof(origin_clid) + 1);
    const network_payload::const_iterator payload_begin =
        begin + sizeof(size) + sizeof(qos) + sizeof(origin_port) + sizeof(origin_ip) +
        sizeof(origin_clid) + 1 + sizeof(topic) + 1;
    const network_payload::const_iterator payload_end = begin + sizeof(size) + size;
    message = std::make_shared<message_payload>(payload_begin, payload_end);
    return size + sizeof(size);
}

packet_ptr packet_factory::from_payload(network_payload_ptr payload, const std::size_t& size) {
    if (size < constants::header_size) throw packet_too_small();

    packet_type type = packet::type_from_payload(payload);
    switch (type) {
        case packet_type::probe:
            return std::make_shared<probe>(payload);
        case packet_type::probe_ack:
        case packet_type::heartbeat_ack:
        case packet_type::subscribe_ack:
        case packet_type::unsubscribe_ack:
        case packet_type::publish_ack:
        case packet_type::disconnect_ack:
            return std::make_shared<ack>(type, payload);
        case packet_type::heartbeat_nack:
        case packet_type::subscribe_nack:
        case packet_type::unsubscribe_nack:
        case packet_type::publish_nack:
            return std::make_shared<nack>(type, payload);
        default:
            return nullptr;
    }
}

}  // namespace octopus_mq::bridge::protocol::v1