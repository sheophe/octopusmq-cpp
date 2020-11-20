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

}  // namespace octopus_mq::bridge::protocol

#endif
