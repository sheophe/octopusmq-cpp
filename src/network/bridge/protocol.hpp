#ifndef OCTOMQ_BRIDGE_PROTOCOL_H_
#define OCTOMQ_BRIDGE_PROTOCOL_H_

#include <chrono>
#include <cinttypes>
#include <memory>
#include <string>

#include <boost/asio.hpp>

#include "core/error.hpp"
#include "network/bridge/protocol_error.hpp"
#include "network/message.hpp"
#include "network/network.hpp"

namespace octopus_mq::bridge {

using std::string;

namespace protocol {

    enum class version : std::uint8_t { v1 = 0x01 };

    constexpr version min_version = version::v1;
    constexpr version max_version = version::v1;

    namespace v1 {

        enum class connection_state {
            undiscovered,         // Node was not yet discovered but planned
            discovery_requested,  // Discovery request was sent to node
            discovered,           // Node successfully "connected"
            lost,                 // Node stopped responding. Discovery should restart
            disconnected          // Node disconnected gracefully. No need to start discovery again
        };

        enum class packet_family : std::uint8_t { normal = 0x00, ack = 0x10, nack = 0x20 };

        enum class packet_kind : std::uint8_t {
            probe = 0x01,
            heartbeat = 0x02,
            subscribe = 0x03,
            unsubscribe = 0x04,
            publish = 0x05,
            disconnect = 0x06
        };

        // packet_type = (packet_family | packet_kind);
        // 0x0* -- "packet" — sent by octopus_mq
        // 0x1* -- "packet"_ack -- received by octopus_mq
        // 0x2* -- "packet"_nack -- send by octopus_mq after specified timeout if "packet"_ack was
        // not received
        enum class packet_type : std::uint8_t {
            probe = 0x01,
            probe_ack = 0x11,
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
            disconnect_ack = 0x16
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

            constexpr std::uint32_t magic_number = 0x42514d4f;  // String "OMQB" (OctopusMQ Bridge)
                                                                // in hexadecimal representation
            constexpr std::uint8_t min_packet_type = static_cast<std::uint8_t>(packet_type::probe);
            constexpr std::uint8_t min_packet_kind = static_cast<std::uint8_t>(packet_kind::probe);
            constexpr std::uint8_t max_packet_kind =
                static_cast<std::uint8_t>(packet_kind::disconnect);
            constexpr std::uint8_t max_packet_family = 0x02;
            constexpr std::uint8_t probe_nack_typeid = 0x21;  // This packet does not actually exist
            constexpr std::uint8_t disconnect_nack_typeid = 0x26;  // This one too
            constexpr std::uint8_t unknown_packet_type = 0xff;

            constexpr std::size_t magic_offset = 0;
            constexpr std::size_t magic_size = sizeof(magic_number);
            constexpr std::size_t version_offset = magic_offset + magic_size;
            constexpr std::size_t version_size = sizeof(std::uint8_t);
            constexpr std::size_t type_offset = version_offset + version_size;
            constexpr std::size_t type_size = sizeof(std::uint8_t);
            constexpr std::size_t seq_n_offset = type_offset + type_size;
            constexpr std::size_t seq_n_size = sizeof(std::uint32_t);
            constexpr std::size_t header_size = seq_n_offset + seq_n_size;

            constexpr std::uint32_t uninit_seq_n = 0;
            constexpr std::uint32_t min_seq_n = 1;
            constexpr std::uint32_t uninit_sub_id = 0;
            constexpr std::uint32_t uninit_pub_id = 0;

            constexpr std::size_t subscription_flags_size = sizeof(std::uint8_t);
            constexpr std::size_t publication_flags_size =
                sizeof(std::uint32_t) + sizeof(std::uint16_t) + sizeof(std::uint8_t);

            constexpr char null_terminator = '\0';

            constexpr std::uint8_t max_nacks_count = 3;

            namespace packet_size {

                constexpr std::size_t probe = header_size;
                constexpr std::size_t ack = header_size;
                constexpr std::size_t nack = header_size;

            }  // namespace packet_size

        }  // namespace constants

        class packet {
           public:
            packet(const packet_type& packet_type,
                   const std::uint32_t seq_n);  // Serialize constructor
            packet(const packet_type& packet_type,
                   network_payload_ptr payload);  // Deserialize constructor

            bool valid;
            std::uint32_t magic;
            version version;
            packet_type type;
            std::uint32_t sequence_number;

            network_payload payload;

            const std::string type_name() const;
            static packet_family family(const packet_type& type);
            static packet_kind kind(const packet_type& type);
            static packet_type type_from_payload(const network_payload_ptr& payload);
        };

        using packet_ptr = std::shared_ptr<packet>;

        // Generic ACK packet
        class ack final : public packet {
           public:
            ack(const packet_type& packet_type, const std::uint32_t seq_n)
                : packet(packet_type, seq_n) {
                if (packet::family(type) != packet_family::ack)
                    throw bridge_malformed_packet_constructed();
                valid = true;
            }  // Serialize constructor

            ack(const packet_type& packet_type, network_payload_ptr payload)
                : packet(packet_type, payload) {}  // Deserialize constructor
        };

        // Generic NACK packet
        class nack final : public packet {
           public:
            nack(const packet_type& packet_type, const std::uint32_t seq_n)
                : packet(packet_type, seq_n) {
                if (packet::family(type) != packet_family::nack)
                    throw bridge_malformed_packet_constructed();
                valid = true;
            }  // Serialize constructor

            nack(const packet_type& packet_type, network_payload_ptr payload)
                : packet(packet_type, payload) {}  // Deserialize constructor
        };

        // PROBE packet
        class probe final : public packet {
           public:
            probe(const std::uint32_t seq_n) : packet(packet_type::probe, seq_n) {
                valid = true;
            }  // Serialize constructor

            probe(network_payload_ptr payload)
                : packet(packet_type::probe, payload) {}  // Deserialize constructor
        };

        // HEARTBEAT packet
        class heartbeat final : public packet {
           public:
            heartbeat(const std::uint32_t seq_n);
            heartbeat(network_payload_ptr payload);
        };

        // DISCONNECT packet
        class disconnect final : public packet {
           public:
            disconnect(const std::uint32_t seq_n) : packet(packet_type::disconnect, seq_n) {
                valid = true;
            }  // Serialize constructor

            disconnect(network_payload_ptr payload)
                : packet(packet_type::disconnect, payload) {}  // Deserialize constructor
        };

        class subscription {
           public:
            std::string topic_filter;
            std::uint8_t qos;

            std::uint32_t size() const {
                return topic_filter.size() + constants::subscription_flags_size;
            }

            void serialize(network_payload& payload) const;
            std::uint32_t deserialize(const network_payload::const_iterator begin);
        };

        using subscription_ptr = std::shared_ptr<subscription>;
        using subscriptions = std::vector<subscription_ptr>;

        class publication {
           public:
            message_payload_ptr message;
            std::string topic;
            std::string origin_clid;
            std::uint32_t origin_ip;
            std::uint16_t origin_port;
            std::uint8_t qos;

            std::uint32_t size() const {
                return message->size() + topic.size() + sizeof(constants::null_terminator) +
                       origin_clid.size() + sizeof(constants::null_terminator) +
                       constants::publication_flags_size;
            }

            void serialize(network_payload& payload) const;
            std::uint32_t deserialize(const network_payload::const_iterator begin);
        };

        using publication_ptr = std::shared_ptr<publication>;
        using publications = std::vector<publication_ptr>;

        // SUBSCRIBE packet
        class subscribe final : public packet {
           public:
            subscribe(const std::uint32_t seq_n, const std::uint32_t sub_id,
                      const subscriptions& subs);  // Serialize constructor

            subscribe(network_payload_ptr payload);  // Deserialize constructor

            std::uint32_t subscription_id;
        };

        // UNSUBSCRIBE packet
        class unsubscribe final : public packet {
           public:
            unsubscribe(const std::uint32_t seq_n, const std::uint32_t sub_id,
                        const subscriptions& subs);  // Serialize constructor

            unsubscribe(network_payload_ptr payload);  // Deserialize constructor
        };

        // PUBLISH packet
        class publish final : public packet {
           public:
            publish(const std::uint32_t seq_n, const std::uint32_t pub_id,
                    const publications& pubs);  // Serialize constructor

            publish(network_payload_ptr payload);  // Deserialize constructor

            std::uint32_t publication_id;
        };

        class packet_factory {
           public:
            static packet_ptr from_payload(network_payload_ptr payload, const std::size_t& size);
        };

    }  // namespace v1

}  // namespace protocol

class connection {
   public:
    connection(const ip_int& ip, const port_int& port)
        : address(ip, port),
          asio_endpoint(boost::asio::ip::udp::endpoint(
              boost::asio::ip::make_address(address::ip_string(ip)), port)),
          receive_buffer(std::make_shared<network_payload>()),
          state(protocol::v1::connection_state::undiscovered),
          last_sent_packet_type(protocol::v1::packet_type::disconnect),
          last_received_packet_type(protocol::v1::packet_type::disconnect),
          last_seq_n(protocol::v1::constants::uninit_seq_n),
          last_pub_id(protocol::v1::constants::uninit_pub_id),
          last_sub_id(protocol::v1::constants::uninit_sub_id),
          received_nacks(0),
          sent_nacks(0) {}

    scope scope;
    address address;
    boost::asio::ip::udp::endpoint asio_endpoint;
    network_payload_ptr receive_buffer;
    protocol::v1::connection_state state;
    protocol::v1::subscriptions subs;
    protocol::v1::packet_type last_sent_packet_type;
    protocol::v1::packet_type last_received_packet_type;
    std::uint32_t last_seq_n;
    std::uint32_t last_pub_id;
    std::uint32_t last_sub_id;
    std::uint8_t received_nacks;
    std::uint8_t sent_nacks;
};

using connection_ptr = std::shared_ptr<connection>;

}  // namespace octopus_mq::bridge

#endif
