#ifndef OCTOMQ_BRIDGE_PROTOCOL_H_
#define OCTOMQ_BRIDGE_PROTOCOL_H_

#include <chrono>
#include <cstdint>
#include <memory>
#include <string>

#include "core/error.hpp"
#include "network/message.hpp"
#include "network/network.hpp"

namespace octopus_mq::bridge {

using std::string;

class endpoints {
   public:
    // In discovery process, octopus_mq send
    address address;
    scope scope;
};

using endpoints_ptr = std::shared_ptr<endpoints>;

namespace protocol {

    enum class version { v1 = 0x01 };

    namespace v1 {

        enum class packet_family { normal = 0x00, ack = 0x10, nack = 0x20, unknown = 0xf0 };

        // 0x0* -- "packet" — sent by octopus_mq
        // 0x1* -- "packet"_ack -- received by octopus_mq
        // 0x2* -- "packet"_nack -- send by octopus_mq after timeout if "packet"_ack was not
        // received
        enum class packet_type {
            probe = 0x01,
            probe_ack = 0x11,
            probe_nack = 0x21,
            heartbeat = 0x02,
            heartbeat_ack = 0x12,
            heartbeat_nack = 0x22,
            subscribe = 0x03,
            subscribe_ack = 0x13,
            subscribe_nack = 0x23,
            unsubscribe = 0x04,
            unsubscribe_ack = 0x14,
            unsubscribe_nack = 0x24,
            publish = 0x05,
            publish_ack = 0x15,
            publish_nack = 0x25,
            disconnect = 0x06,
            disconnect_ack = 0x16,
            disconnect_nack = 0x26,
            unknown = 0xff
        };

        namespace packet_name {

            constexpr char probe[] = "probe";
            constexpr char heartbeat[] = "heartbeat";
            constexpr char subscribe[] = "subscribe";
            constexpr char unsubscribe[] = "unsubscribe";
            constexpr char publish[] = "publish";
            constexpr char disconnect[] = "disconnect";
            constexpr char ack[] = "ack";
            constexpr char nack[] = "nack";
            constexpr char unknown[] = "unknown";

            const string from_type(const packet_type& type);

        }  // namespace packet_name

        namespace constants {

            constexpr uint64_t magic_number =
                0x6f63746f70757321;  // String "octopus!" in hex representation

            constexpr int magic_offset = 0;
            constexpr int magic_size = sizeof(magic_number);
            constexpr int version_offset = magic_offset + magic_size;
            constexpr int version_size = sizeof(uint8_t);
            constexpr int type_offset = version_offset + version_size;
            constexpr int type_size = sizeof(uint8_t);
            constexpr int seq_n_offset = type_offset + type_size;
            constexpr int seq_n_size = sizeof(uint32_t);
            constexpr int header_size = seq_n_offset + seq_n_size;

            constexpr int min_seq_n = 1;

            constexpr int subscription_flags_size = sizeof(uint8_t);
            constexpr int publication_flags_size =
                sizeof(uint32_t) + sizeof(uint16_t) + sizeof(uint8_t);
            constexpr char null_terminator = '\0';

        }  // namespace constants

        class subscription {
           public:
            std::string topic_filter;
            uint8_t qos;

            uint32_t size() const {
                return topic_filter.size() + constants::subscription_flags_size;
            }

            void serialize(network_payload& payload) const;
            uint32_t deserialize(const network_payload::const_iterator begin);
        };

        using subscription_ptr = std::shared_ptr<subscription>;
        using subscriptions = std::vector<subscription_ptr>;

        class publication {
           public:
            message_payload_ptr message;
            std::string topic;
            std::string origin_clid;
            uint32_t origin_ip;
            uint16_t origin_port;
            uint8_t qos;

            uint32_t size() const {
                return message->size() + topic.size() + 1 + origin_clid.size() + 1 +
                       constants::publication_flags_size;
            }

            void serialize(network_payload& payload) const;
            uint32_t deserialize(const network_payload::const_iterator begin);
        };

        using publication_ptr = std::shared_ptr<publication>;
        using publications = std::vector<publication_ptr>;

        class packet {
           public:
            packet(const packet_type& packet_type, const uint32_t seq_n);  // Serialize constructor
            packet(const network_payload& payload);  // Deserialize constructor

            bool valid;
            uint64_t magic;
            version version;
            packet_type type;
            uint32_t sequence_number;
            network_payload payload;

            static packet_family family(const packet_type& type);
        };

        // Generic ACK packet
        class ack final : public packet {
           public:
            ack(const v1::packet_type& packet_type, const uint32_t seq_n)
                : packet(packet_type, seq_n) {
                if (packet::family(type) != packet_family::ack)
                    throw bridge_malformed_packet_constructed();
                valid = true;
            }  // Serialize constructor

            ack(const network_payload& payload) : packet(payload) {}  // Deserialize constructor
        };

        // Generic NACK packet
        class nack final : public packet {
           public:
            nack(const v1::packet_type& packet_type, const uint32_t seq_n)
                : packet(packet_type, seq_n) {
                if (packet::family(type) != packet_family::nack)
                    throw bridge_malformed_packet_constructed();
                valid = true;
            }  // Serialize constructor

            nack(const network_payload& payload) : packet(payload) {}  // Deserialize constructor
        };

        // PROBE packet
        class probe final : public packet {
           public:
            probe(const uint32_t seq_n) : packet(packet_type::probe, seq_n) {
                valid = true;
            }  // Serialize constructor

            probe(const network_payload& payload) : packet(payload) {}  // Deserialize constructor
        };

        // HEARTBEAT packet
        class heartbeat final : public packet {
           public:
            heartbeat(const uint32_t seq_n) : packet(packet_type::heartbeat, seq_n) {
                valid = true;
            }  // Serialize constructor

            heartbeat(const network_payload& payload)
                : packet(payload) {}  // Deserialize constructor
        };

        // DISCONNECT packet
        class disconnect final : public packet {
           public:
            disconnect(const uint32_t seq_n) : packet(packet_type::disconnect, seq_n) {
                valid = true;
            }  // Serialize constructor

            disconnect(const network_payload& payload)
                : packet(payload) {}  // Deserialize constructor
        };

        // SUBSCRIBE packet
        class subscribe final : public packet {
           public:
            subscribe(const uint32_t seq_n, const uint32_t sub_id,
                      const subscriptions& subs);  // Serialize constructor

            subscribe(const network_payload& payload);  // Deserialize constructor

            uint32_t subscription_id;
        };

        // UNSUBSCRIBE packet
        class unsubscribe final : public packet {
           public:
            unsubscribe(const uint32_t seq_n, const uint32_t sub_id,
                        const subscriptions& subs);  // Serialize constructor

            unsubscribe(const network_payload& payload);  // Deserialize constructor
        };

        // PUBLISH packet
        class publish final : public packet {
           public:
            publish(const uint32_t seq_n, const uint32_t pub_id,
                    const publications& pubs);  // Serialize constructor

            publish(const network_payload& payload);  // Deserialize constructor

            uint32_t publication_id;
        };

    }  // namespace v1

}  // namespace protocol

}  // namespace octopus_mq::bridge

#endif
