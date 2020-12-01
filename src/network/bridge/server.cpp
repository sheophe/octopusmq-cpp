#include "network/bridge/server.hpp"
#include "core/log.hpp"

#include <iterator>
#include <utility>
#include <cstdlib>

namespace octopus_mq::bridge {

server::server(asio::io_context& ioc, asio::ip::udp::endpoint&& udp_endpoint,
               adapter_settings_ptr settings, const std::string& adapter_name)
    : _ioc(ioc),
      _udp_ep(std::forward<asio::ip::udp::endpoint>(udp_endpoint)),
      _udp_socket(_ioc),
      _settings(settings),
      _adapter_name(adapter_name) {
    initialize();
}

void server::initialize() {
    _max_nacks = 3;
    _poly_udp_ep = asio::ip::udp::endpoint(asio::ip::address_v4(_settings->polycast_address().ip()),
                                           _settings->polycast_address().port());
    _polycast_receive_buffer =
        std::make_shared<network_payload>(protocol::v1::constants::packet_size::max);
}

void server::run() {
    _stop_request = false;
    try {
        _udp_socket.open(asio::ip::udp::v4());
        _udp_socket.set_option(asio::socket_base::reuse_address(true));
        if (_settings->transport_mode() == transport_mode::multicast) {
            _udp_socket.set_option(asio::ip::multicast::join_group(_poly_udp_ep.address().to_v4(),
                                                                   _udp_ep.address().to_v4()));
            _udp_socket.set_option(asio::ip::multicast::hops(_settings->multicast_hops()));
            _udp_socket.set_option(
                asio::ip::multicast::outbound_interface(_udp_ep.address().to_v4()));
        } else
            _udp_socket.set_option(asio::socket_base::broadcast(true));
        _udp_socket.bind(asio::ip::udp::endpoint(asio::ip::address_v4(), _udp_ep.port()));
    } catch (boost::system::system_error const& e) {
        asio::post(_ioc, [this, ec = e.code()] {
            if (_network_error_handler) _network_error_handler(ec);
        });
        return;
    }
    start_discovery();
}

void server::stop() {
    _stop_request = true;
    _udp_socket.close();
}

void server::publish(message_ptr message) { (*message); }

void server::set_network_error_handler(network_error_handler handler) {
    _network_error_handler = std::move(handler);
}

void server::set_protocol_error_handler(protocol_error_handler handler) {
    _protocol_error_handler = std::move(handler);
}

using namespace protocol::v1;

void server::start_discovery() {
    _polycast_discovery_delay_timer = std::make_unique<asio::steady_timer>(_ioc);
    async_polycast_listen();
    async_polycast_probe();
}

void server::async_polycast_probe() {
    // Broadcast/multicast probe packet
    packet_ptr probe = std::make_unique<protocol::v1::probe>(
        address(_settings->phy().ip(), _settings->port()), _settings->scope());

    const std::size_t bytes_sent = _udp_socket.send_to(asio::buffer(probe->payload), _poly_udp_ep);
    if (probe->payload.size() < bytes_sent)
        throw boost::system::system_error(asio::error::message_size);

    log::print_event(_adapter_name, _settings->polycast_address().to_string(), std::string(),
                     network_event_type::send, probe->type_name());

    // Restart the timer for next probe broadcast/multicast
    _polycast_discovery_delay_timer->expires_after(_settings->timeouts().discovery);
    _polycast_discovery_delay_timer->async_wait([this](const boost::system::error_code& ec) {
        if (ec != asio::error::operation_aborted) async_polycast_probe();
    });
}

void server::async_polycast_listen() {
    _udp_socket.async_receive_from(
        asio::buffer(*_polycast_receive_buffer), _poly_udp_ep,
        [this](const boost::system::error_code& ec, const std::size_t& bytes_received) {
            handle_polycast_receive(ec, bytes_received);
        });
}

void server::async_listen_to(const connection_ptr& endpoint, const std::size_t& buffer_size) {
    const network_payload_ptr& receive_buffer = endpoint->udp_receive_buffer;
    if (receive_buffer->size() != buffer_size) receive_buffer->resize(buffer_size);
    _udp_socket.async_receive_from(
        asio::buffer(*receive_buffer), endpoint->udp_endpoint,
        [this, &endpoint](const boost::system::error_code& ec, const std::size_t& bytes_received) {
            handle_receive_from(endpoint, ec, bytes_received);
        });
}

void server::handle_polycast_receive(const boost::system::error_code& ec,
                                     const std::size_t& bytes_received) {
    if (ec and _network_error_handler) _network_error_handler(ec);

    try {
        if (*reinterpret_cast<std::uint32_t*>(_polycast_receive_buffer->data()) ==
            constants::magic_number) {
            std::pair<packet_type, address> packet_meta =
                packet::meta_from_payload(_polycast_receive_buffer);
            if (packet_meta.first == packet_type::probe) {
                const ip_int& ip = packet_meta.second.ip();
                const port_int& port = packet_meta.second.port();
                // Process packet only if it wasn't sent from the local endpoint.
                // Ports however should match.
                if (_settings->phy().ip() != ip and _settings->port() == port) {
                    std::set<connection_ptr>::iterator found_iter = _endpoints.end();
                    for (auto iter = _endpoints.begin(); iter != _endpoints.end(); ++iter)
                        if ((*iter)->address.ip() == ip and (*iter)->address.port() == port) {
                            found_iter = iter;
                            break;
                        }
                    if (found_iter != _endpoints.end())
                        handle_packet(*found_iter, packet_factory::from_payload(
                                                       _polycast_receive_buffer, bytes_received));
                    else
                        handle_packet(
                            *_endpoints.emplace(std::make_unique<connection>(ip, port, _ioc)).first,
                            packet_factory::from_payload(_polycast_receive_buffer, bytes_received));
                }
            }
        } else
            throw protocol::invalid_magic_number();
    } catch (const protocol::basic_error& error) {
        if (_protocol_error_handler) _protocol_error_handler(error);
    } catch (const boost::system::system_error& error) {
        if (_network_error_handler) _network_error_handler(error.code());
    }

    async_polycast_listen();
}

void server::handle_receive_from(const connection_ptr& endpoint,
                                 const boost::system::error_code& ec,
                                 const std::size_t& bytes_received) {
    if (ec and _network_error_handler) _network_error_handler(ec);

    try {
        handle_packet(endpoint,
                      packet_factory::from_payload(endpoint->udp_receive_buffer, bytes_received));
    } catch (const protocol::basic_error& error) {
        if (_protocol_error_handler) _protocol_error_handler(error);
    } catch (const boost::system::system_error& error) {
        if (_network_error_handler) _network_error_handler(error.code());
    }

    const std::size_t& buffer_size = (endpoint->state == connection_state::discovered)
                                         ? constants::packet_size::max
                                         : constants::packet_size::probe;

    async_listen_to(endpoint, buffer_size);
}

void server::sync_send_to(const connection_ptr& endpoint, packet_ptr packet) {
    const std::size_t bytes_sent =
        _udp_socket.send_to(asio::buffer(packet->payload), endpoint->udp_endpoint);
    if (packet->payload.size() < bytes_sent)
        throw boost::system::system_error(asio::error::message_size);
    endpoint->last_sent_packet_type = packet->type;

    log::print_event(_adapter_name, endpoint->address.to_string(), std::string(),
                     network_event_type::send, packet->type_name());
}

bool server::is_packet_expected(const connection_ptr& endpoint, const packet_type& type) {
    const connection_state& state = endpoint->state;

    if ((state == connection_state::undiscovered) or (state == connection_state::disconnected))
        // First packet from yet undiscovered or disconnected endpoint.
        // Only 'probe' is accepted as a first incoming packet.
        return type == packet_type::probe;

    if (state == connection_state::discovery_requested)
        // First packet from endpoint to which 'probe' packet has been sent.
        // Only 'probe' and 'heartbeat' are accepted as first incoming packets.
        return (type == packet_type::probe) or (type == packet_type::heartbeat);

    // Considering the possibility of out-of-order packets anything could happen after connection
    // has been established.
    return true;
}

void server::handle_packet(const connection_ptr& endpoint, packet_ptr packet) {
    const packet_type& type = packet->type;
    const std::string address = endpoint->address.to_string();

    log::print_event(_adapter_name, address, std::string(), network_event_type::receive,
                     packet->type_name());

    endpoint->last_received_packet_type = type;
    endpoint->rediscovery_attempts = 0;
    endpoint->sent_nacks = 0;

    // Handle packet based on its type
    switch (type) {
        case packet_type::probe:
            handle_probe(endpoint);
            break;
        case packet_type::heartbeat:
            handle_heartbeat(endpoint, std::move(packet));
            break;
            // case packet_type::acknack:
            //     handle_acknack(endpoint, std::move(packet));
            //     break;
            // case packet_type::publish:
            //     handle_publish(endpoint, std::move(packet));
            //     break;
        default:
            break;
    }
}

void server::handle_probe(const connection_ptr& endpoint) {
    // Rewrite last sequence number to be in sync with discovered endpoint.
    endpoint->state = connection_state::discovered;
}

void server::handle_heartbeat(const connection_ptr& endpoint, protocol::v1::packet_ptr packet) {
    endpoint->last_hb_id = static_cast<protocol::v1::heartbeat*>(packet.get())->packet_id;
    endpoint->state = connection_state::discovered;
}

}  // namespace octopus_mq::bridge
