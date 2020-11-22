#ifndef OCTOMQ_BRIDGE_PROTOCOL_H_
#define OCTOMQ_BRIDGE_PROTOCOL_H_

#include <chrono>
#include <cinttypes>
#include <memory>
#include <string>
#include <queue>

#include <boost/asio.hpp>

#include "core/error.hpp"
#include "network/bridge/protocol_error.hpp"
#include "network/message.hpp"
#include "network/network.hpp"

namespace octopus_mq::bridge {

namespace protocol {

    enum class version : std::uint8_t { v1 = 0x01 };

    constexpr version min_version = version::v1;
    constexpr version max_version = version::v1;

    namespace v1 {

        enum class connection_state {
            undiscovered,         // Node was not yet discovered but planned
            discovery_requested,  // Discovery request was sent to node
            discovered,           // Node successfully "connected"
            nacking,              // Node didn't sent ack and server is now sending nacks
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

            const std::string from_type(const packet_type& type);

        }  // namespace packet_name

        namespace constants {

            constexpr std::uint32_t magic_number = 0x42514d4f;  // String "OMQB" (OctopusMQ Bridge)
                                                                // in hexadecimal representation
            constexpr std::uint8_t min_packet_type = static_cast<std::uint8_t>(packet_type::probe);
            constexpr std::uint8_t min_packet_kind = static_cast<std::uint8_t>(packet_kind::probe);
            constexpr std::uint8_t max_packet_kind =
                static_cast<std::uint8_t>(packet_kind::disconnect);
            constexpr std::uint8_t max_packet_family = 0x02;
            constexpr std::uint8_t disconnect_nack_typeid = 0x26;  // This packet does not exist
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

                constexpr std::size_t min = header_size;
                constexpr std::size_t probe = min + sizeof(ip_int) + sizeof(port_int);
                constexpr std::size_t max = 0x0400;

                constexpr std::size_t sub_unsub_min = min + 2 * sizeof(uint32_t) + sizeof(uint8_t);

            }  // namespace packet_size

        }  // namespace constants

        class opayload_stream {
            network_payload& _payload;

           public:
            opayload_stream(network_payload& payload);

            opayload_stream& operator<<(const std::uint8_t& value);
            opayload_stream& operator<<(const std::uint16_t& value);
            opayload_stream& operator<<(const std::uint32_t& value);
            opayload_stream& operator<<(const std::uint64_t& value);
            opayload_stream& operator<<(const version& value);
            opayload_stream& operator<<(const packet_type& value);
            opayload_stream& operator<<(const std::string& value);
        };

        class ipayload_stream {
            const network_payload& _payload;
            network_payload::const_iterator _iterator;

           public:
            ipayload_stream(const network_payload& payload);

            ipayload_stream& operator>>(std::uint8_t& value);
            ipayload_stream& operator>>(std::uint16_t& value);
            ipayload_stream& operator>>(std::uint32_t& value);
            ipayload_stream& operator>>(std::uint64_t& value);
            ipayload_stream& operator>>(version& value);
            ipayload_stream& operator>>(packet_type& value);
            ipayload_stream& operator>>(std::string& value);

            void skip_header();
        };

        class packet {
           public:
            packet(const packet_type& packet_type,
                   const std::uint32_t& seq_n);  // Serialize constructor
            packet(const packet_type& packet_type,
                   network_payload_ptr payload);  // Deserialize constructor

            std::uint32_t magic;
            version version;
            packet_type type;
            std::uint32_t sequence_number;

            network_payload payload;

            const std::string type_name() const;
            static packet_family family(const packet_type& type);
            static packet_kind kind(const packet_type& type);
            static packet_type type_from_payload(const network_payload_ptr& payload);
            static packet_type nack_type(const packet_type& type);
        };

        using packet_ptr = std::unique_ptr<packet>;

        // Generic ACK packet
        class ack final : public packet {
           public:
            ack(const packet_type& packet_type, const std::uint32_t& seq_n)
                : packet(packet_type, seq_n) {
                if (packet::family(type) != packet_family::ack)
                    throw bridge_malformed_packet_constructed();
            }  // Serialize constructor

            ack(const packet_type& packet_type, network_payload_ptr payload)
                : packet(packet_type, payload) {}  // Deserialize constructor
        };

        // Generic NACK packet
        class nack final : public packet {
           public:
            nack(const packet_type& packet_type, const std::uint32_t& seq_n)
                : packet(packet_type, seq_n) {
                if (packet::family(type) != packet_family::nack)
                    throw bridge_malformed_packet_constructed();
            }  // Serialize constructor

            nack(const packet_type& packet_type, network_payload_ptr payload)
                : packet(packet_type, payload) {}  // Deserialize constructor
        };

        // PROBE packet
        class probe final : public packet {
           public:
            probe(const ip_int& ip, const port_int& port,
                  const std::uint32_t& seq_n);  // Serialize constructor

            probe(network_payload_ptr payload);  // Deserialize constructor

            ip_int ip;
            port_int port;
        };

        using discovered_nodes = std::set<std::pair<ip_int, port_int>>;

        // HEARTBEAT packet
        class heartbeat final : public packet {
           public:
            heartbeat(const discovered_nodes& nodes, const std::chrono::milliseconds& interval,
                      const std::uint32_t& seq_n);
            heartbeat(network_payload_ptr payload);

            std::uint32_t interval;
            discovered_nodes nodes;
        };

        // DISCONNECT packet
        class disconnect final : public packet {
           public:
            disconnect(const std::uint32_t& seq_n)
                : packet(packet_type::disconnect, seq_n) {}  // Serialize constructor

            disconnect(network_payload_ptr payload)
                : packet(packet_type::disconnect, payload) {}  // Deserialize constructor
        };

        // Aggregated subscribe container
        class subscription {
           public:
            void add_topic(const std::string& topic);
            void generate_payload();
            void parse_payload();

            std::set<std::uint64_t> topic_hashes;
            std::set<std::string> topic_names;

            network_payload full_payload;
        };

        // Aggregated publish container
        class publication {
           public:
            void add_message(message_ptr message);
            void generate_payload();
            void parse_payload();

            std::vector<message_ptr> message;

            network_payload full_payload;
        };

        // SUBSCRIBE/UNSUBSCRIBE packet
        class subscribe_unsubscribe final : public packet {
           public:
            subscribe_unsubscribe(const packet_type& packet_type, const std::uint32_t& seq_n,
                                  const std::uint32_t& sub_id);  // Serialize constructor

            std::uint32_t subscription_id;
            std::uint32_t total_blocks;
            std::uint8_t block_n;
        };

        using sub_unsub_ptr = std::unique_ptr<subscribe_unsubscribe>;

        // PUBLISH packet
        class publish final : public packet {
           public:
            publish(const std::uint32_t seq_n,
                    const std::uint32_t pub_id);  // Serialize constructor

            std::uint32_t publication_id;
            std::uint32_t total_blocks;
            std::uint8_t block_n;
        };

        using publish_ptr = std::unique_ptr<publish>;

        class packet_factory {
           public:
            static packet_ptr from_payload(const network_payload_ptr& payload,
                                           const std::size_t& size);
        };

    }  // namespace v1

}  // namespace protocol

using namespace boost;

class connection {
   public:
    connection(const ip_int& ip, const port_int& port)
        : address(ip, port),
          udp_endpoint(asio::ip::udp::endpoint(asio::ip::address_v4(ip), port)),
          udp_receive_buffer(std::make_shared<network_payload>()),
          state(protocol::v1::connection_state::undiscovered),
          last_sent_packet_type(protocol::v1::packet_type::disconnect),
          last_received_packet_type(protocol::v1::packet_type::disconnect),
          last_seq_n(protocol::v1::constants::uninit_seq_n),
          last_pub_id(protocol::v1::constants::uninit_pub_id),
          last_sub_id(protocol::v1::constants::uninit_sub_id),
          received_nacks(0),
          sent_nacks(0),
          rediscovery_attempts(0) {}

    scope scope;
    address address;
    asio::ip::udp::endpoint udp_endpoint;
    network_payload_ptr udp_receive_buffer;
    std::unique_ptr<protocol::v1::subscription> subscription_outgoing_store;
    std::unique_ptr<protocol::v1::subscription> subscription_incoming_store;
    std::queue<protocol::v1::sub_unsub_ptr> subscribe_outgoing_queue;
    std::queue<protocol::v1::sub_unsub_ptr> subscribe_incoming_queue;
    std::unique_ptr<protocol::v1::subscription> unsubscription_outgoing_store;
    std::unique_ptr<protocol::v1::subscription> unsubscription_incoming_store;
    std::queue<protocol::v1::sub_unsub_ptr> unsubscribe_outgoing_queue;
    std::queue<protocol::v1::sub_unsub_ptr> unsubscribe_incoming_queue;
    std::unique_ptr<protocol::v1::publication> publication_outgoing_store;
    std::unique_ptr<protocol::v1::publication> publication_incoming_store;
    std::queue<protocol::v1::publish_ptr> publish_outgoing_queue;
    std::queue<protocol::v1::publish_ptr> publish_incoming_queue;
    std::unique_ptr<asio::steady_timer> unicast_rediscovery_timer;
    std::unique_ptr<asio::steady_timer> heartbeat_timer;
    std::unique_ptr<asio::steady_timer> heartbeat_nack_timer;
    std::unique_ptr<asio::steady_timer> publish_nack_timer;
    std::unique_ptr<asio::steady_timer> subscribe_nack_timer;
    std::unique_ptr<asio::steady_timer> unsubscribe_nack_timer;
    std::chrono::milliseconds heartbeat_interval;
    protocol::v1::connection_state state;
    protocol::v1::packet_type last_sent_packet_type;
    protocol::v1::packet_type last_received_packet_type;
    std::uint32_t last_seq_n;
    std::uint32_t last_pub_id;
    std::uint32_t last_sub_id;
    std::uint8_t received_nacks;
    std::uint8_t sent_nacks;
    std::uint32_t rediscovery_attempts;
};

using connection_ptr = std::unique_ptr<connection>;

}  // namespace octopus_mq::bridge

#endif
