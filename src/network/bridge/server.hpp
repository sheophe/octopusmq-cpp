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
    adapter_settings_ptr _settings;
    asio::steady_timer _polycast_discovery_delay_timer;
    network_payload_ptr _polycast_receive_buffer;
    std::uint32_t _max_nacks;
    const std::string _adapter_name;

    // Members initialized in constructor body or in runtime
    bool _stop_request;
    std::set<connection_ptr> _endpoints;
    std::queue<message_ptr> _publish_queue;

    // External handlers
    network_error_handler _network_error_handler;
    protocol_error_handler _protocol_error_handler;

   public:
    server(asio::io_context& ioc, asio::ip::udp::endpoint&& udp_endpoint,
           adapter_settings_ptr settings, const std::string& adapter_name);

    // External control functions
    void run();
    void stop();
    void publish(message_ptr message);

    // External handler setters
    void set_network_error_handler(network_error_handler handler = network_error_handler());
    void set_protocol_error_handler(protocol_error_handler handler = protocol_error_handler());

   private:
    // Internal control functions
    void start_discovery();

    void async_polycast_probe();
    void send_polycast_heartbeat();
    void async_polycast_listen();
    void async_listen_to(const connection_ptr& endpoint);
    void sync_send_to(const connection_ptr& endpoint, protocol::v1::packet_ptr packet);

    // Internal handlers
    void handle_polycast_receive(const boost::system::error_code& ec,
                                 const std::size_t& bytes_received);
    void handle_receive_from(const connection_ptr& endpoint, const boost::system::error_code& ec,
                             const std::size_t& bytes_received);

    void handle_packet(const connection_ptr& endpoint, protocol::v1::packet_ptr packet);

    // Packet handlers
    void handle_probe(const connection_ptr& endpoint, protocol::v1::packet_ptr packet);
    void handle_publish(const connection_ptr& endpoint, protocol::v1::packet_ptr packet);
    void handle_heartbeat(const connection_ptr& endpoint, protocol::v1::packet_ptr packet);
    void handle_acknack(const connection_ptr& endpoint, protocol::v1::packet_ptr packet);
};

}  // namespace octopus_mq::bridge

#endif
