#ifndef OCTOMQ_BRIDGE_SERVER_H_
#define OCTOMQ_BRIDGE_SERVER_H_

#include <chrono>
#include <condition_variable>
#include <set>
#include <mutex>
#include <string>

#include <boost/asio.hpp>

#include "network/bridge/adapter.hpp"
#include "network/bridge/protocol.hpp"
#include "network/bridge/protocol_error.hpp"
#include "network/network.hpp"

namespace octopus_mq::bridge {

using namespace boost;

class server {
    using network_error_handler = std::function<void(boost::system::error_code ec)>;
    using protocol_error_handler = std::function<void(protocol::basic_error err)>;

    // Members initialized in constructor initializer list
    asio::io_context& _ioc;
    asio::ip::udp::endpoint _udp_ep;
    asio::ip::udp::endpoint _poly_udp_ep;
    asio::ip::udp::socket _udp_socket;
    std::unique_ptr<asio::ip::icmp::endpoint> _icmp_ep;
    std::unique_ptr<asio::ip::icmp::socket> _icmp_socket;
    adapter_settings_ptr _settings;
    const std::string _adapter_name;
    const bool _use_icmp;

    // Members initialized in constructor body or in runtime
    bool _stop_request;
    std::set<connection_ptr> _endpoints;
    std::unique_ptr<asio::steady_timer> _unicast_discovery_delay_timer;
    std::unique_ptr<asio::steady_timer> _polycast_discovery_delay_timer;
    network_payload_ptr _polycast_receive_buffer;
    std::queue<message_ptr> _publish_queue;
    std::uint32_t _max_nacks;

    // External handlers
    network_error_handler _network_error_handler;
    protocol_error_handler _protocol_error_handler;

    void initialize();

   public:
    // Constructor with ICMP
    server(asio::io_context& ioc, asio::ip::udp::endpoint&& udp_endpoint,
           asio::ip::icmp::endpoint&& icmp_endpoint, adapter_settings_ptr settings,
           const std::string& adapter_name);

    // Constructor without ICMP
    server(asio::io_context& ioc, asio::ip::udp::endpoint&& udp_endpoint,
           adapter_settings_ptr settings, const std::string& adapter_name);

    // External control functions
    void run();
    void stop();
    void publish(const message_ptr message);

    // External handler setters
    void set_network_error_handler(network_error_handler handler = network_error_handler());
    void set_protocol_error_handler(protocol_error_handler handler = protocol_error_handler());

   private:
    // Internal control functions
    void start_discovery();

    void async_polycast_discovery();
    void async_unicast_discovery(const std::set<connection_ptr>::iterator& iter,
                                 const bool& retry = false);
    void async_unicast_rediscovery(const connection_ptr& endpoint, const bool& retry = false);

    void send_polycast_heartbeat();
    void send_unicast_heartbeat(const connection_ptr& endpoint);
    void async_nack_sender(const connection_ptr& endpoint, const protocol::v1::packet_type& type,
                           const std::uint32_t& packet_id);

    void async_udp_polycast_listen();
    void async_udp_listen_to(const connection_ptr& endpoint, const std::size_t& buffer_size);
    void udp_send_to(const connection_ptr& endpoint, protocol::v1::packet_ptr packet);

    // Internal handlers
    void handle_udp_polycast_receive(const boost::system::error_code& ec,
                                     const std::size_t& bytes_received);
    void handle_udp_receive_from(const connection_ptr& endpoint,
                                 const boost::system::error_code& ec,
                                 const std::size_t& bytes_received);
    void handle_unicast_probe_sent(const std::set<connection_ptr>::iterator& iter,
                                   const connection_ptr& endpoint,
                                   const protocol::v1::packet_ptr& packet,
                                   const boost::system::error_code& ec,
                                   const std::size_t& bytes_sent, const bool& retry);
    void handle_unicast_rediscovery_sent(const connection_ptr& endpoint,
                                         const protocol::v1::packet_ptr& packet,
                                         const boost::system::error_code& ec,
                                         const std::size_t& bytes_sent);

    void handle_packet(const connection_ptr& endpoint, protocol::v1::packet_ptr packet);
    bool is_packet_expected(const connection_ptr& endpoint, const protocol::v1::packet_type& type);

    // Packet handlers
    void handle_probe(const connection_ptr& endpoint);
    void handle_heartbeat(const connection_ptr& endpoint, protocol::v1::packet_ptr packet);
    void handle_heartbeat_ack(const connection_ptr& endpoint);
    void handle_heartbeat_nack(const connection_ptr& endpoint, protocol::v1::packet_ptr packet);
};

}  // namespace octopus_mq::bridge

#endif
