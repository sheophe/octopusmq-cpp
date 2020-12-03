#ifndef OCTOMQ_BRIDGE_PROTOCOL_H_
#define OCTOMQ_BRIDGE_PROTOCOL_H_

#include <chrono>
#include <cinttypes>
#include <memory>
#include <string>
#include <utility>
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

        enum class packet_type : std::uint8_t {
            probe = 0x01,
            publish = 0x02,
            heartbeat = 0x03,
            acknack = 0x04
        };

        enum class compression_type : std::uint8_t {
            none = 0x00,
            bzip2 = 0x01,
            gzip = 0x02,
            zlib = 0x03
        };

        namespace packet_name {
            constexpr char probe[] = "probe";
            constexpr char heartbeat[] = "heartbeat";
            constexpr char publish[] = "publish";
            constexpr char acknack[] = "acknack";
            constexpr char unknown[] = "unknown";

            const std::string from_type(const packet_type& type);

        }  // namespace packet_name

        namespace constants {

            constexpr std::uint32_t magic_number = 0x42514d4f;  // String "OMQB" (OctopusMQ Bridge)
                                                                // in hexadecimal representation
            constexpr std::uint8_t min_type = static_cast<std::uint8_t>(packet_type::probe);
            constexpr std::uint8_t max_type = static_cast<std::uint8_t>(packet_type::acknack);
            constexpr std::uint8_t unknown_packet_type = 0xff;

            constexpr std::size_t magic_offset = 0;
            constexpr std::size_t magic_size = sizeof(magic_number);
            constexpr std::size_t version_offset = magic_offset + magic_size;
            constexpr std::size_t version_size = sizeof(std::uint8_t);
            constexpr std::size_t type_offset = version_offset + version_size;
            constexpr std::size_t type_size = sizeof(std::uint8_t);
            constexpr std::size_t ip_offset = type_offset + type_size;
            constexpr std::size_t ip_size = sizeof(ip_int);
            constexpr std::size_t port_offset = ip_offset + ip_size;
            constexpr std::size_t port_size = sizeof(port_int);
            constexpr std::size_t header_size = port_offset + port_size;

            constexpr std::size_t publish_header_size =
                header_size + sizeof(std::uint32_t) + sizeof(std::uint8_t) + sizeof(std::uint32_t) +
                sizeof(std::uint32_t) + sizeof(std::uint64_t);

            constexpr std::uint32_t uninit_packet_id = 0;

            constexpr std::size_t subscription_flags_size = sizeof(std::uint8_t);
            constexpr std::size_t publication_flags_size =
                sizeof(std::uint32_t) + sizeof(std::uint16_t) + sizeof(std::uint8_t);

            constexpr char null_terminator = '\0';

            constexpr std::uint8_t max_nacks_count = 3;

            namespace packet_size {

                constexpr std::size_t min = header_size;
                constexpr std::size_t probe = min + sizeof(ip_int) + sizeof(port_int);
                constexpr std::size_t max = 0x0400;
                constexpr std::size_t max_publish_payload = max - constants::publish_header_size;

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
            opayload_stream& operator<<(const network_payload_ptr& value);

            void push_payload(const network_payload_iter_pair& iter_pair);
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
            const network_payload::const_iterator& current_iterator() const;
        };

        class packet {
           public:
            packet(const packet_type& packet_type,
                   const address& sender_address);  // Serialize constructor
            packet(const packet_type& packet_type,
                   network_payload_ptr payload);  // Deserialize constructor

            std::uint32_t magic;
            version version;
            packet_type type;
            address sender_address;
            network_payload payload;

            const std::string type_name() const;
            static std::pair<packet_type, address> meta_from_payload(
                const network_payload_ptr& payload);
        };

        using packet_ptr = std::unique_ptr<packet>;

        // PROBE packet
        class probe final : public packet {
           public:
            probe(const address& sender_address,
                  const scope& sender_scope);    // Serialize constructor
            probe(network_payload_ptr payload);  // Deserialize constructor

            scope sender_scope;
        };

        // HEARTBEAT packet
        class heartbeat final : public packet {
           public:
            heartbeat(const address& sender_address, const std::uint32_t& packet_id,
                      const std::chrono::milliseconds& interval,
                      const std::vector<std::uint64_t>& published_hashes);
            heartbeat(network_payload_ptr payload);

            std::uint32_t packet_id;
            std::uint32_t interval;
            std::uint32_t published_n;
            std::vector<std::uint64_t> published_hashes;
        };

        // ACKNACK packet
        class acknack final : public packet {
           public:
            acknack(const address& sender_address,
                    const std::uint32_t& packet_id);  // Serialize constructor
            acknack(network_payload_ptr payload);     // Deserialize constructor

            std::uint32_t packet_id;
        };

        // Aggregated publish container
        class publication {
            std::vector<message_ptr> _messages;
            network_payload _payload;
            compression_type _compression;
            network_payload::const_iterator _iter;
            std::size_t _total_blocks;

           public:
            publication()
                : _compression(compression_type::none), _iter(_payload.begin()), _total_blocks(0) {}

            void emplace(message_ptr message);
            void clear();
            bool empty();

            std::size_t total_blocks() const;
            compression_type compression() const;
            void read(network_payload_iter_pair& block_iterators);
            void generate_payload(const compression_type& compression);
            void from_payload(network_payload_ptr payload);
        };

        using publication_ptr = std::unique_ptr<publication>;

        // PUBLISH packet
        // Caller of the serialize constructor MUST make sure that pub is not nullptr
        class publish final : public packet {
           public:
            publish(const address& sender_address, const std::uint32_t& packet_id,
                    publication_ptr pub, const std::size_t& block_number);  // Serialize constructor
            publish(network_payload_ptr payload);

            std::uint32_t packet_id;
            compression_type compression;
            std::uint32_t total_blocks;
            std::uint32_t block_n;
            std::uint32_t block_size;
            network_payload compressed_payload_block;
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
    connection(const ip_int& ip, const port_int& port, asio::io_context& ioc)
        : address(ip, port),
          udp_endpoint(asio::ip::udp::endpoint(asio::ip::address_v4(ip), port)),
          udp_receive_buffer(std::make_shared<network_payload>()),
          heartbeat_timer(ioc),
          last_sent_packet_type(protocol::v1::packet_type::acknack),
          last_received_packet_type(protocol::v1::packet_type::acknack),
          last_heartbeat_id(protocol::v1::constants::uninit_packet_id),
          last_publish_id(protocol::v1::constants::uninit_packet_id) {}

    scope scope;
    address address;
    asio::ip::udp::endpoint udp_endpoint;
    network_payload_ptr udp_receive_buffer;
    protocol::v1::publication_ptr publication_outgoing_store;
    protocol::v1::publication_ptr publication_incoming_store;
    std::queue<protocol::v1::publish_ptr> publish_outgoing_queue;
    std::queue<protocol::v1::publish_ptr> publish_incoming_queue;
    asio::steady_timer heartbeat_timer;
    std::chrono::milliseconds heartbeat_interval;
    protocol::v1::packet_type last_sent_packet_type;
    protocol::v1::packet_type last_received_packet_type;
    std::uint32_t last_heartbeat_id;
    std::uint32_t last_publish_id;
};

using connection_ptr = std::unique_ptr<connection>;

}  // namespace octopus_mq::bridge

#endif
