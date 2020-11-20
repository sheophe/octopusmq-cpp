#include "network/bridge/server.hpp"
#include "core/log.hpp"

#include <thread>
#include <utility>

#include <boost/lexical_cast.hpp>

namespace octopus_mq::bridge {

server::server(ip::udp::endpoint&& endpoint, const port_int& send_port, io_context& ioc,
               const transport_mode& transport_mode, const discovery_endpoints_format& format,
               const discovery_endpoints& endpoints, const std::string& adapter_name,
               const bool verbose)
    : _ep(std::forward<ip::udp::endpoint>(endpoint)),
      _send_port(send_port),
      _ioc(ioc),
      _transport_mode(transport_mode),
      _socket(_ioc),
      _adapter_name(adapter_name),
      _max_nacks(protocol::v1::constants::max_nacks_count),
      _probe_delay(adapter::default_timeouts::delay),
      _verbose(verbose),
      _stop_request(false) {
    if (format == discovery_endpoints_format::list) {
        for (auto& ip : std::get<discovery_list>(endpoints))
            if ((ip == network::constants::loopback_ip) or
                (ip::udp::endpoint(ip::make_address(address::ip_string(ip)), _send_port) != _ep))
                _endpoints.push_back(std::make_shared<connection>(ip, _send_port));
    } else {
        discovery_range range = std::get<discovery_range>(endpoints);
        for (ip_int ip = range.from; ip <= range.to; address::increment_ip(ip))
            if ((ip == network::constants::loopback_ip) or
                (ip::udp::endpoint(ip::make_address(address::ip_string(ip)), _send_port) != _ep))
                _endpoints.push_back(std::make_shared<connection>(ip, _send_port));
    }
    _single_endpoint = (_endpoints.size() == 1);
}

void server::set_probe_delay(const std::chrono::milliseconds& delay) { _probe_delay = delay; }

void server::set_max_nacks(const std::uint8_t& max_nacks) { _max_nacks = max_nacks; }

void server::set_system_error_handler(system_error_handler handler) {
    _system_error_handler = std::move(handler);
}

void server::set_protocol_error_handler(protocol_error_handler handler) {
    _protocol_error_handler = std::move(handler);
}

void server::run() {
    _stop_request = false;
    try {
        _socket.open(ip::udp::v4());
        _socket.set_option(ip::udp::socket::reuse_address(true));
        if (_transport_mode == transport_mode::broadcast)
            _socket.set_option(socket_base::broadcast(true));
        _socket.bind(_ep);
    } catch (boost::system::system_error const& e) {
        boost::asio::post(_ioc, [this, ec = e.code()] {
            if (_system_error_handler) _system_error_handler(ec);
        });
        return;
    }
    start_discovery();
}

void server::stop() {
    _stop_request = true;
    _socket.close();
}

using namespace protocol::v1;

void server::start_discovery() {
    if (_transport_mode == transport_mode::unicast) {
        std::mutex delay_mutex;
        std::condition_variable delay_cv;
        std::size_t loop_i = 0;
        for (auto& endpoint : _endpoints) {
            try {
                send_to(endpoint, std::make_shared<protocol::v1::probe>(++(endpoint->last_seq_n)));
                endpoint->state = connection_state::discovery_requested;
            } catch (const boost::system::system_error& error) {
                if (_system_error_handler) _system_error_handler(error.code());
            }

            async_listen(endpoint,
                         std::max(constants::packet_size::probe, constants::packet_size::ack));

            if (++loop_i; loop_i < _endpoints.size()) {
                std::unique_lock<std::mutex> delay_lock(delay_mutex);
                if (delay_cv.wait_for(delay_lock, _probe_delay, [this]() { return _stop_request; }))
                    break;
            }
        }
    }

    if (_verbose)
        log::print(log_type::info, "sent discovery packets to all endpoints.", _adapter_name);
}

void server::async_listen(connection_ptr endpoint, const std::size_t max_packet_size) {
    network_payload_ptr receive_buffer = endpoint->receive_buffer;
    receive_buffer->resize(max_packet_size);
    _socket.async_receive_from(
        boost::asio::buffer(*receive_buffer), endpoint->asio_endpoint,
        [this, endpoint](const boost::system::error_code ec, std::size_t bytes_received) {
            handle_receive(endpoint, endpoint->receive_buffer, ec, bytes_received);
        });
}

void server::send_to(connection_ptr endpoint, packet_ptr packet, const bool print_log) {
    const std::string endpoint_address = endpoint->address.to_string();
    const std::size_t bytes_sent =
        _socket.send_to(boost::asio::buffer(packet->payload), endpoint->asio_endpoint);
    if (packet->payload.size() < bytes_sent)
        throw boost::system::system_error(boost::asio::error::message_size);

    if (_verbose)
        log::print(log_type::info,
                   "sent " + std::to_string(bytes_sent) + " bytes to " + endpoint_address,
                   _adapter_name);

    if (print_log)
        log::print_event(_adapter_name, endpoint_address, std::string(), network_event_type::send,
                         packet->type_name());
}

void server::handle_receive(connection_ptr endpoint, network_payload_ptr payload,
                            const boost::system::error_code& ec, std::size_t bytes_received) {
    if (ec and _system_error_handler) _system_error_handler(ec);
    std::size_t expected_packet_size =
        std::max(constants::packet_size::probe, constants::packet_size::ack);

    if (_verbose)
        log::print(log_type::info,
                   "received " + std::to_string(bytes_received) + " bytes from " +
                       endpoint->address.to_string(),
                   _adapter_name);

    try {
        handle_packet(endpoint, packet_factory::from_payload(payload, bytes_received));
    } catch (const protocol::basic_error& error) {
        if (_protocol_error_handler) _protocol_error_handler(error);
    } catch (const boost::system::system_error& error) {
        if (_system_error_handler) _system_error_handler(error.code());
    }

    async_listen(endpoint, expected_packet_size);
}

bool server::is_packet_asynchronous(const packet_type& type) {
    return ((type == packet_type::probe) or (type == packet_type::heartbeat) or
            (type == packet_type::publish) or (type == packet_type::subscribe) or
            (type == packet_type::unsubscribe) or (type == packet_type::disconnect) or
            (packet::family(type) == packet_family::nack));
}

bool server::is_packet_expected(connection_ptr endpoint, const packet_type& type) {
    const connection_state& state = endpoint->state;

    if ((state == connection_state::undiscovered) or (state == connection_state::disconnected))
        // First packet from yet undiscovered or disconnected endpoint.
        // Only probe is accepted as a first incoming packet.
        return type == packet_type::probe;

    if (state == connection_state::discovery_requested)
        // First packet from endpoint to which discovery packet has been sent.
        // Only probe (happens when both endpoints send a packet at the same time) and probe_ack are
        // accepted as first incoming packets.
        return (type == packet_type::probe) or (type == packet_type::probe_ack);

    // After discovery handshake, packets 'hearbeat', 'publish', 'subscribe', 'unsubscribe' and
    // 'disconnect' are accepted regardless of current sequence, as well as 'nack' and 'probe'
    // packet. That is because remote endpoint may send them asynchronously.
    // Packets 'nack' and 'probe' may be received asynchronously when remote endpoint lost
    // connection to local server.
    if ((type == packet_type::probe) or (type == packet_type::heartbeat) or
        (type == packet_type::publish) or (type == packet_type::subscribe) or
        (type == packet_type::unsubscribe) or (type == packet_type::disconnect) or
        (packet::family(type) == packet_family::nack))
        return true;

    // Other packets should come in correct sequence.
    switch (endpoint->last_received_packet_type) {
        case packet_type::probe:
            return (type == packet_type::probe) or (type == packet_type::probe_ack);
        case packet_type::heartbeat:
            return (type == packet_type::heartbeat) or (type == packet_type::heartbeat_ack);
        case packet_type::publish:
            return type == packet_type::publish_ack;
        case packet_type::subscribe:
            return type == packet_type::subscribe_ack;
        case packet_type::unsubscribe:
            return type == packet_type::unsubscribe_ack;
        case packet_type::disconnect:
            return type == packet_type::disconnect_ack;
        default:
            return false;
    }
}

void server::handle_packet(connection_ptr endpoint, packet_ptr packet) {
    const packet_type& type = packet->type;
    const std::string& address = endpoint->address.to_string();

    if (not is_packet_expected(endpoint, type))
        throw protocol::invalid_packet_sequence(packet->type_name(), address);
    else {
        if (endpoint->last_seq_n > packet->sequence_number)
            throw protocol::out_of_order(packet->type_name(), address);

        ++(endpoint->last_seq_n);
        endpoint->last_received_packet_type = type;

        log::print_event(_adapter_name, address, std::string(), network_event_type::receive,
                         packet->type_name());

        // Send response packet
        switch (type) {
            case packet_type::probe:
                handle_probe(endpoint);
                break;
                // case packet_type::probe_ack:
                //     return handle_probe_ack(ep, packet);
                // case packet_type::heartbeat:
                //     return handle_heartbeat(ep, packet);
                // case packet_type::heartbeat_ack:
                //     return handle_heartbeat_ack(ep, packet);
                // case packet_type::heartbeat_nack:
                //     return handle_heartbeat_nack(ep, packet);
                // case packet_type::subscribe:
                //     return handle_subscribe(ep, packet);
                // case packet_type::subscribe_ack:
                //     return handle_subscribe_ack(ep, packet);
                // case packet_type::subscribe_nack:
                //     return handle_subscribe_nack(ep, packet);
                // case packet_type::unsubscribe:
                //     return handle_unsubscribe(ep, packet);
                // case packet_type::unsubscribe_ack:
                //     return handle_unsubscribe_ack(ep, packet);
                // case packet_type::unsubscribe_nack:
                //     return handle_unsubscribe_nack(ep, packet);
                // case packet_type::publish:
                //     return handle_publish(ep, packet);
                // case packet_type::publish_ack:
                //     return handle_publish_ack(ep, packet);
                // case packet_type::publish_nack:
                //     return handle_publish_nack(ep, packet);
                // case packet_type::disconnect:
                //     return handle_disconnect(ep, packet);
                // case packet_type::disconnect_ack:
                //     return handle_disconnect_ack(ep, packet);
            default:
                break;
        }
    }
}

void server::handle_probe(connection_ptr endpoint) {
    send_to(endpoint,
            std::make_shared<protocol::v1::ack>(packet_type::probe_ack, endpoint->last_seq_n));
    endpoint->state = connection_state::discovered;
    endpoint->last_sent_packet_type = packet_type::probe_ack;
}

}  // namespace octopus_mq::bridge
