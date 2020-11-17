#ifndef OCTOMQ_BRIDGE_PROTOCOL_H_
#define OCTOMQ_BRIDGE_PROTOCOL_H_

#include "network/network.hpp"

namespace octopus_mq::bridge {

class endpoints {
   public:
    // In discovery process, octopus_mq send
    octopus_mq::address address;
    octopus_mq::scope scope;
};

using endpoints_ptr = std::shared_ptr<endpoints>;

namespace protocol {

    // String "octopus!" in hex representation
    constexpr uint64_t magic_number = 0x6f63746f70757321;

    enum class packet_kind { normal = 0x00, ack = 0x10, nack = 0x20 };

    // 0x0* -- "packet"
    // 0x1* -- "packet"_ack
    // 0x2* -- "packet"_nack
    enum class packet_header {
        search = 0x01,
        bridge_info = 0x02,
        heartbeat = 0x03,
        heartbeat_ack = 0x13,
        heartbeat_nack = 0x23,
        scope = 0x04,
        scope_ack = 0x14,
        scope_nack = 0x24,
        publish = 0x05,
        publish_ack = 0x15,
        publish_nack = 0x25,
        finish = 0x06,
        finish_ack = 0x16,
        finish_nack = 0x26
    };

}  // namespace protocol

}  // namespace octopus_mq::bridge

#endif
