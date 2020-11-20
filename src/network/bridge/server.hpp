#ifndef OCTOMQ_BRIDGE_SERVER_H_
#define OCTOMQ_BRIDGE_SERVER_H_

#include <chrono>
#include <condition_variable>
#include <map>
#include <mutex>
#include <string>

#include <boost/asio.hpp>

#include "network/bridge/adapter.hpp"
#include "network/bridge/protocol.hpp"
#include "network/bridge/protocol_error.hpp"
#include "network/network.hpp"

namespace octopus_mq::bridge {

using namespace boost::asio;

class server {
    using system_error_handler = std::function<void(boost::system::error_code ec)>;
    using protocol_error_handler = std::function<void(protocol::basic_error err)>;

    // Members initialized in constructor initializer list
    ip::udp::endpoint _ep;
    const port_int _send_port;
    io_context& _ioc;
    const transport_mode _transport_mode;
    ip::udp::socket _socket;
    const std::string& _adapter_name;
    std::uint8_t _max_nacks;
    std::chrono::milliseconds _probe_delay;
    const bool _verbose;
    bool _stop_request;

    // Members initialized in constructor body or in runtime
    bool _single_endpoint;
    std::vector<connection_ptr> _endpoints;

    // External handlers
    system_error_handler _system_error_handler;
    protocol_error_handler _protocol_error_handler;

   public:
    server(ip::udp::endpoint&& endpoint, const port_int& send_port, io_context& ioc,
           const transport_mode& transport_mode, const discovery_endpoints_format& format,
           const discovery_endpoints& endpoints, const std::string& adapter_name,
           const bool verbose);

    // External control functions
    void run();
    void stop();

    // External config setters
    void set_probe_delay(const std::chrono::milliseconds& delay);
    void set_max_nacks(const std::uint8_t& max_nacks);

    // External handler setters
    void set_system_error_handler(system_error_handler handler = system_error_handler());
    void set_protocol_error_handler(protocol_error_handler handler = protocol_error_handler());

   private:
    // Internal control functions
    void start_discovery();
    void async_listen(connection_ptr endpoint, const std::size_t max_packet_size);
    void send_to(connection_ptr endpoint, protocol::v1::packet_ptr packet,
                 const bool print_log = true);

    // Internal handlers
    void handle_receive(connection_ptr endpoint, network_payload_ptr payload,
                        const boost::system::error_code& ec, std::size_t bytes_received);
    void handle_packet(connection_ptr endpoint, protocol::v1::packet_ptr packet);
    bool is_packet_asynchronous(const protocol::v1::packet_type& type);
    bool is_packet_expected(connection_ptr endpoint, const protocol::v1::packet_type& type);

    // Packet handlers
    void handle_probe(connection_ptr endpoint);
    // void handle_probe_ack(const ip::udp::endpoint& ep, protocol::v1::packet_ptr packet);
    // void handle_heartbeat(const ip::udp::endpoint& ep, protocol::v1::packet_ptr packet);
    // void handle_heartbeat_ack(const ip::udp::endpoint& ep, protocol::v1::packet_ptr packet);
    // void handle_heartbeat_nack(const ip::udp::endpoint& ep, protocol::v1::packet_ptr packet);
    // void handle_subscribe(const ip::udp::endpoint& ep, protocol::v1::packet_ptr packet);
    // void handle_subscribe_ack(const ip::udp::endpoint& ep, protocol::v1::packet_ptr packet);
    // void handle_subscribe_nack(const ip::udp::endpoint& ep, protocol::v1::packet_ptr packet);
    // void handle_unsubscribe(const ip::udp::endpoint& ep, protocol::v1::packet_ptr packet);
    // void handle_unsubscribe_ack(const ip::udp::endpoint& ep, protocol::v1::packet_ptr packet);
    // void handle_unsubscribe_nack(const ip::udp::endpoint& ep, protocol::v1::packet_ptr packet);
    // void handle_publish(const ip::udp::endpoint& ep, protocol::v1::packet_ptr packet);
    // void handle_publish_ack(const ip::udp::endpoint& ep, protocol::v1::packet_ptr packet);
    // void handle_publish_nack(const ip::udp::endpoint& ep, protocol::v1::packet_ptr packet);
    // void handle_disconnect(const ip::udp::endpoint& ep, protocol::v1::packet_ptr packet);
    // void handle_disconnect_ack(const ip::udp::endpoint& ep, protocol::v1::packet_ptr packet);
};

}  // namespace octopus_mq::bridge

#endif
