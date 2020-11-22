#ifndef OCTOMQ_BRIDGE_PROTOCOL_ERROR_H_
#define OCTOMQ_BRIDGE_PROTOCOL_ERROR_H_

#include <string>
#include <stdexcept>

namespace octopus_mq::bridge::protocol {

// Base classes
class basic_error : public std::runtime_error {
   public:
    basic_error(const std::string& what) : std::runtime_error("protocol error: " + what) {}
};

class invalid_packet : public basic_error {
   public:
    invalid_packet(const std::string& what) : basic_error(what) {}
};

class invalid_sequence : public basic_error {
   public:
    invalid_sequence(const std::string& what) : basic_error(what) {}
};

// Final classes for invalid_packet
class packet_too_small final : public invalid_packet {
   public:
    packet_too_small() : invalid_packet("received packet is too small.") {}
};

class invalid_magic_number final : public invalid_packet {
   public:
    invalid_magic_number() : invalid_packet("received packet has invalid magic number.") {}
};

class unsupported_version final : public invalid_packet {
   public:
    unsupported_version() : invalid_packet("received packet has unsupported protocol version.") {}
};

class invalid_packet_type final : public invalid_packet {
   public:
    invalid_packet_type() : invalid_packet("received packet has invalid type.") {}
};

class invalid_sequence_number final : public invalid_packet {
   public:
    invalid_sequence_number() : invalid_packet("received packet has invalid sequence number.") {}
};

class nack_does_not_exist final : public invalid_packet {
   public:
    nack_does_not_exist() : invalid_packet("nack does not exist for requested type of packet.") {}
};

// Final classes for invalid_sequence
class invalid_packet_sequence final : public invalid_sequence {
   public:
    invalid_packet_sequence(const std::string& packet_type_name, const std::string& sender_address)
        : invalid_sequence("unexpected '" + packet_type_name + "' packet received from " +
                           sender_address) {}
};

class out_of_order final : public invalid_sequence {
   public:
    out_of_order(const std::string& packet_type_name, const std::string& sender_address)
        : invalid_sequence("out of order '" + packet_type_name + "' packet received from " +
                           sender_address) {}
};

// Other
class ipayload_stream_out_of_range : public std::out_of_range {
   public:
    ipayload_stream_out_of_range(const std::string& type_name)
        : std::out_of_range("cannot read value of type \"" + type_name +
                            "\" from network payload.") {}
};

}  // namespace octopus_mq::bridge::protocol

#endif
