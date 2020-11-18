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

const string packet_name::from_type(const packet_type& type) {
    const packet_family family = packet::family(type);
    switch (family) {
        case packet_family::normal:
            return packet_name_map[type];
        case packet_family::ack:
            return packet_name::ack;
        case packet_family::nack:
            return packet_name::nack;
        case packet_family::unknown:
            return packet_name::unknown;
    }
}

packet::packet(const packet_type& packet_type, const uint32_t seq_n)
    : valid(false),
      magic(constants::magic_number),
      version(version::v1),
      type(packet_type),
      sequence_number(seq_n),
      payload(constants::header_size) {
    const uint8_t u8_version = static_cast<uint8_t>(version);
    const uint8_t u8_type = static_cast<uint8_t>(type);
    std::memcpy(payload.data() + constants::magic_offset, &magic, constants::magic_size);
    std::memcpy(payload.data() + constants::version_offset, &u8_version, constants::version_size);
    std::memcpy(payload.data() + constants::type_offset, &u8_type, constants::type_size);
}

packet::packet(const network_payload& payload)
    : valid(false), magic(0), version(version::v1), type(packet_type::unknown), payload(payload) {
    if (payload.size() < constants::header_size) throw bridge_malformed_packet();
    const char* payload_data = this->payload.data();
    std::memcpy(&magic, payload_data + constants::magic_offset, constants::magic_size);
    version = static_cast<protocol::version>(*(payload_data + constants::version_offset));
    type = static_cast<protocol::v1::packet_type>(*(payload_data + constants::type_offset));
}

packet_family packet::family(const packet_type& type) {
    return static_cast<packet_family>(static_cast<uint8_t>(type) & 0xf0);
}

void subscription::serialize(network_payload& payload) const {
    char header[sizeof(uint32_t) + constants::subscription_flags_size];
    uint32_t size = this->size();
    std::memcpy(header, &size, sizeof(uint32_t));
    std::memcpy(header + sizeof(uint32_t), &qos, sizeof(uint8_t));

    std::copy(header, header + sizeof(header), std::back_inserter(payload));
    std::copy(topic_filter.begin(), topic_filter.end(), std::back_inserter(payload));
}

uint32_t subscription::deserialize(const network_payload::const_iterator begin) {
    const uint32_t size = *reinterpret_cast<const uint32_t*>(begin.base());
    qos = *reinterpret_cast<const uint8_t*>(begin.base() + sizeof(size));
    topic_filter = std::string(begin + sizeof(size) + sizeof(qos), begin + sizeof(size) + size);
    return size + sizeof(size);
}

void publication::serialize(network_payload& payload) const {
    char header[sizeof(uint32_t) + constants::publication_flags_size];
    uint32_t size = this->size();
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

uint32_t publication::deserialize(const network_payload::const_iterator begin) {
    const uint32_t size = *reinterpret_cast<const uint32_t*>(begin.base());
    qos = *reinterpret_cast<const uint8_t*>(begin.base() + sizeof(size));
    origin_port = *reinterpret_cast<const uint16_t*>(begin.base() + sizeof(size) + sizeof(qos));
    origin_ip = *reinterpret_cast<const uint32_t*>(begin.base() + sizeof(size) + sizeof(qos) +
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

}  // namespace octopus_mq::bridge::protocol::v1