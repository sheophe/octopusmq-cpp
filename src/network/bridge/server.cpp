#include "network/bridge/server.hpp"
#include "core/log.hpp"

#include <iterator>
#include <utility>
#include <cstdlib>

namespace octopus_mq::bridge {

// Constructor with ICMP
server::server(asio::io_context& ioc, asio::ip::udp::endpoint&& udp_endpoint,
               asio::ip::icmp::endpoint&& icmp_endpoint, adapter_settings_ptr settings,
               const std::string& adapter_name)
    : _ioc(ioc),
      _udp_ep(std::forward<asio::ip::udp::endpoint>(udp_endpoint)),
      _udp_socket(_ioc),
      _icmp_ep(std::make_unique<asio::ip::icmp::endpoint>(
          std::forward<asio::ip::icmp::endpoint>(icmp_endpoint))),
      _icmp_socket(std::make_unique<asio::ip::icmp::socket>(_ioc)),
      _settings(settings),
      _adapter_name(adapter_name),
      _use_icmp(true) {
    initialize();
}

// Constructor without ICMP
server::server(asio::io_context& ioc, asio::ip::udp::endpoint&& udp_endpoint,
               adapter_settings_ptr settings, const std::string& adapter_name)
    : _ioc(ioc),
      _udp_ep(std::forward<asio::ip::udp::endpoint>(udp_endpoint)),
      _udp_socket(_ioc),
      _settings(settings),
      _adapter_name(adapter_name),
      _use_icmp(false) {
    initialize();
}

void server::initialize() {
    _max_nacks = 3;
    if (_settings->transport_mode() == transport_mode::unicast) {
        const port_int& send_port = _settings->send_port();
        if (_settings->discovery().format == discovery_endpoints_format::list) {
            for (auto& ip : std::get<discovery_list>(_settings->discovery().endpoints))
                if (ip::is_loopback(ip) or
                    (asio::ip::udp::endpoint(asio::ip::address_v4(ip), send_port) != _udp_ep))
                    _endpoints.emplace(std::make_unique<connection>(ip, send_port, _ioc));
        } else {
            discovery_range range = std::get<discovery_range>(_settings->discovery().endpoints);
            for (ip_int ip = range.from; ip <= range.to; ++ip)
                if (ip::is_loopback(ip) or
                    (asio::ip::udp::endpoint(asio::ip::address_v4(ip), send_port) != _udp_ep))
                    _endpoints.emplace(std::make_unique<connection>(ip, send_port, _ioc));
        }
    } else {
        _poly_udp_ep =
            asio::ip::udp::endpoint(asio::ip::address_v4(_settings->polycast_address().ip()),
                                    _settings->polycast_address().port());
        _polycast_receive_buffer =
            std::make_shared<network_payload>(protocol::v1::constants::packet_size::max);
    }
}

void server::run() {
    _stop_request = false;
    try {
        if (_use_icmp) _icmp_socket->open(asio::ip::icmp::v4());
        _udp_socket.open(asio::ip::udp::v4());
        _udp_socket.set_option(asio::socket_base::reuse_address(true));
        if (_settings->transport_mode() == transport_mode::broadcast)
            _udp_socket.set_option(asio::socket_base::broadcast(true));
        else if (_settings->transport_mode() == transport_mode::multicast) {
            _udp_socket.set_option(asio::ip::multicast::join_group(_poly_udp_ep.address().to_v4(),
                                                                   _udp_ep.address().to_v4()));
            _udp_socket.set_option(asio::ip::multicast::hops(_settings->multicast_hops()));
            _udp_socket.set_option(
                asio::ip::multicast::outbound_interface(_udp_ep.address().to_v4()));
        }
        _udp_socket.bind(asio::ip::udp::endpoint(asio::ip::address_v4(), _udp_ep.port()));
    } catch (boost::system::system_error const& e) {
        asio::post(_ioc, [this, ec = e.code()] {
            if (_network_error_handler) _network_error_handler(ec);
        });
        return;
    }
    start_discovery();
}  // namespace octopus_mq::bridge

void server::stop() {
    _stop_request = true;
    _udp_socket.close();
    if (_use_icmp) _icmp_socket->close();
}

void server::publish(const message_ptr message) { (*message); }

void server::set_network_error_handler(network_error_handler handler) {
    _network_error_handler = std::move(handler);
}

void server::set_protocol_error_handler(protocol_error_handler handler) {
    _protocol_error_handler = std::move(handler);
}

using namespace protocol::v1;

void server::start_discovery() {
    if (_settings->transport_mode() == transport_mode::unicast) {
        _unicast_discovery_delay_timer = std::make_unique<asio::steady_timer>(_ioc);
        async_unicast_discovery(_endpoints.begin());
    } else {
        _polycast_discovery_delay_timer = std::make_unique<asio::steady_timer>(_ioc);
        async_udp_polycast_listen();
        async_polycast_discovery();
    }
}

void server::async_polycast_discovery() {
    // Broadcast/multicast probe packet
    packet_ptr probe =
        std::make_unique<protocol::v1::probe>(_settings->phy().ip(), _settings->port());

    const std::size_t bytes_sent = _udp_socket.send_to(asio::buffer(probe->payload), _poly_udp_ep);
    if (probe->payload.size() < bytes_sent)
        throw boost::system::system_error(asio::error::message_size);

    log::print_event(_adapter_name, _settings->polycast_address().to_string(), std::string(),
                     network_event_type::send, probe->type_name());

    // Restart the timer for next probe broadcast/multicast
    _polycast_discovery_delay_timer->expires_after(_settings->timeouts().discovery);
    _polycast_discovery_delay_timer->async_wait([this](const boost::system::error_code& ec) {
        if (ec != asio::error::operation_aborted) async_polycast_discovery();
    });
}

void server::async_unicast_discovery(const std::set<connection_ptr>::iterator& iter,
                                     const bool& retry) {
    const connection_ptr& endpoint = *iter;
    if (retry) ++(endpoint->rediscovery_attempts);

    protocol::v1::packet_ptr probe =
        std::make_unique<protocol::v1::probe>(_udp_ep.address().to_v4().to_ulong(), _udp_ep.port());

    _udp_socket.async_send_to(
        asio::buffer(probe->payload), endpoint->udp_endpoint,
        [this, &endpoint, iter = std::move(iter), packet = std::move(probe), retry](
            const boost::system::error_code& ec, const std::size_t& bytes_sent) {
            handle_unicast_probe_sent(iter, endpoint, packet, ec, bytes_sent, retry);
        });
}

void server::async_unicast_rediscovery(const connection_ptr& endpoint, const bool& retry) {
    if (retry) ++(endpoint->rediscovery_attempts);

    protocol::v1::packet_ptr probe =
        std::make_unique<protocol::v1::probe>(_udp_ep.address().to_v4().to_ulong(), _udp_ep.port());

    _udp_socket.async_send_to(
        asio::buffer(probe->payload), endpoint->udp_endpoint,
        [this, &endpoint, packet = std::move(probe)](const boost::system::error_code& ec,
                                                     const std::size_t& bytes_sent) {
            handle_unicast_rediscovery_sent(endpoint, packet, ec, bytes_sent);
        });
}

void server::async_udp_polycast_listen() {
    _udp_socket.async_receive(
        asio::buffer(*_polycast_receive_buffer),
        [this](const boost::system::error_code& ec, const std::size_t& bytes_received) {
            handle_udp_polycast_receive(ec, bytes_received);
        });
}

void server::async_udp_listen_to(const connection_ptr& endpoint, const std::size_t& buffer_size) {
    const network_payload_ptr& receive_buffer = endpoint->udp_receive_buffer;
    if (receive_buffer->size() != buffer_size) receive_buffer->resize(buffer_size);
    _udp_socket.async_receive_from(
        asio::buffer(*receive_buffer), endpoint->udp_endpoint,
        [this, &endpoint](const boost::system::error_code& ec, const std::size_t& bytes_received) {
            handle_udp_receive_from(endpoint, ec, bytes_received);
        });
}

void server::async_nack_sender(const connection_ptr& endpoint,
                               const protocol::v1::packet_type& type,
                               const std::uint32_t& packet_id) {
    if (endpoint->sent_nacks == _max_nacks) {
        // If after _max_nacks retransmissions endpoint didn't respond, mark it as lost and send
        // rediscovery instead of another nack.
        log::print(log_type::error, "connection lost with " + endpoint->address.to_string());

        endpoint->state = connection_state::lost;
        endpoint->heartbeat_timer.cancel();
        endpoint->unicast_rediscovery_timer.expires_after(_settings->timeouts().rescan);
        endpoint->unicast_rediscovery_timer.async_wait(
            [this, &endpoint](const boost::system::error_code& ec) {
                if (ec != asio::error::operation_aborted) async_unicast_rediscovery(endpoint);
            });
    } else {
        // Send nack packet
        udp_send_to(endpoint,
                    std::make_unique<protocol::v1::nack>(type, packet_id, endpoint->sent_nacks));
        ++(endpoint->sent_nacks);

        // Restart the timer for another nack
        endpoint->nack_timer.expires_after(_settings->timeouts().acknowledge);
        endpoint->nack_timer.async_wait(
            [this, &endpoint, type, packet_id](const boost::system::error_code& ec) {
                if (ec != asio::error::operation_aborted)
                    async_nack_sender(endpoint, type, packet_id);
            });
    }
}

void server::handle_udp_polycast_receive(const boost::system::error_code& ec,
                                         const std::size_t& bytes_received) {
    if (ec and _network_error_handler) _network_error_handler(ec);

    try {
        packet_ptr packet = packet_factory::from_payload(_polycast_receive_buffer, bytes_received);
        if (packet->type == packet_type::probe) {
            const ip_int ip = static_cast<protocol::v1::probe*>(packet.get())->ip;
            const port_int port = static_cast<protocol::v1::probe*>(packet.get())->port;

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
                    handle_packet(*found_iter, std::move(packet));
                else
                    handle_packet(
                        *_endpoints.emplace(std::make_unique<connection>(ip, port, _ioc)).first,
                        std::move(packet));
            }
        }
    } catch (const protocol::basic_error& error) {
        if (_protocol_error_handler) _protocol_error_handler(error);
    } catch (const boost::system::system_error& error) {
        if (_network_error_handler) _network_error_handler(error.code());
    }

    async_udp_polycast_listen();
}

void server::handle_udp_receive_from(const connection_ptr& endpoint,
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

    async_udp_listen_to(endpoint, buffer_size);
}

void server::handle_unicast_probe_sent(const std::set<connection_ptr>::iterator& iter,
                                       const connection_ptr& endpoint,
                                       const protocol::v1::packet_ptr& packet,
                                       const boost::system::error_code& ec,
                                       const std::size_t& bytes_sent, const bool& retry) {
    if (ec) {
        if (_network_error_handler) _network_error_handler(ec);
    } else
        async_udp_listen_to(endpoint, constants::packet_size::max);

    if (packet->payload.size() < bytes_sent and _network_error_handler)
        _network_error_handler(boost::system::system_error(asio::error::message_size).code());

    log::print_event(_adapter_name, endpoint->address.to_string(), std::string(),
                     network_event_type::send, packet->type_name());

    // If handler was called on retry attempt, no need to restart the timer and run discovery
    // again. Also no need to check if endpoint timer is created.
    if (not retry) {
        endpoint->last_sent_packet_type = packet->type;
        endpoint->state = connection_state::discovery_requested;

        auto next = std::next(iter);
        while (next != _endpoints.end() and next->get()->state == connection_state::discovered)
            next = std::next(next);

        if (next != _endpoints.end()) {
            _unicast_discovery_delay_timer->expires_after(_settings->timeouts().delay);
            _unicast_discovery_delay_timer->async_wait(
                [this, next = std::move(next)](const boost::system::error_code& ec) {
                    if (ec and _network_error_handler)
                        _network_error_handler(ec);
                    else
                        async_unicast_discovery(next);
                });
        }
    }

    endpoint->unicast_rediscovery_timer.expires_after(_settings->timeouts().rescan);
    endpoint->unicast_rediscovery_timer.async_wait(
        [this, iter = std::move(iter)](const boost::system::error_code& ec) {
            if (ec != asio::error::operation_aborted) async_unicast_discovery(iter, true);
        });
}

void server::handle_unicast_rediscovery_sent(const connection_ptr& endpoint,
                                             const protocol::v1::packet_ptr& packet,
                                             const boost::system::error_code& ec,
                                             const std::size_t& bytes_sent) {
    if (ec and _network_error_handler) _network_error_handler(ec);

    if (packet->payload.size() < bytes_sent and _network_error_handler)
        _network_error_handler(boost::system::system_error(asio::error::message_size).code());

    log::print_event(_adapter_name, endpoint->address.to_string(), std::string(),
                     network_event_type::send, packet->type_name());

    endpoint->unicast_rediscovery_timer.expires_after(_settings->timeouts().rescan);
    endpoint->unicast_rediscovery_timer.async_wait(
        [this, &endpoint](const boost::system::error_code& ec) {
            if (ec != asio::error::operation_aborted) async_unicast_rediscovery(endpoint, true);
        });
}

void server::udp_send_to(const connection_ptr& endpoint, packet_ptr packet) {
    const std::size_t bytes_sent =
        _udp_socket.send_to(asio::buffer(packet->payload), endpoint->udp_endpoint);
    if (packet->payload.size() < bytes_sent)
        throw boost::system::system_error(asio::error::message_size);
    endpoint->last_sent_packet_type = packet->type;

    log::print_event(_adapter_name, endpoint->address.to_string(), std::string(),
                     network_event_type::send, packet->type_name());
}

void server::send_unicast_heartbeat(const connection_ptr& endpoint) {
    // Construct and send heartbeat packet
    discovered_nodes nodes;
    for (auto& ep : _endpoints)
        if (ep != endpoint and ep->state == connection_state::discovered)
            nodes.emplace(std::make_pair(ep->address.ip(), ep->address.port()));

    udp_send_to(endpoint, std::make_unique<protocol::v1::heartbeat>(
                              nodes, ++(endpoint->last_hb_id), _settings->timeouts().heartbeat));

    // Start nack timer for the heartbeat
    endpoint->nack_timer.expires_after(_settings->timeouts().acknowledge);
    endpoint->nack_timer.async_wait([this, &endpoint](const boost::system::error_code& ec) {
        if (ec != asio::error::operation_aborted)
            async_nack_sender(endpoint, packet_type::heartbeat_nack, endpoint->last_hb_id);
    });
}

bool server::is_packet_expected(const connection_ptr& endpoint, const packet_type& type) {
    const connection_state& state = endpoint->state;
    // log::printf(log_type::note, "state: %#04x, type: %#04x", static_cast<std::uint8_t>(state),
    //             static_cast<std::uint8_t>(type));

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

    endpoint->unicast_rediscovery_timer.cancel();
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
        case packet_type::heartbeat_ack:
            handle_heartbeat_ack(endpoint);
            break;
        case packet_type::heartbeat_nack:
            handle_heartbeat_nack(endpoint, std::move(packet));
            break;
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
        default:
            break;
    }
}

void server::handle_probe(const connection_ptr& endpoint) {
    // Rewrite last sequence number to be in sync with discovered endpoint.
    endpoint->state = connection_state::discovered;

    // Start listening to the new endpoint
    async_udp_listen_to(endpoint, protocol::v1::constants::packet_size::max);
    // Send heartbeat as a response
    send_unicast_heartbeat(endpoint);
}

void server::handle_heartbeat(const connection_ptr& endpoint, protocol::v1::packet_ptr packet) {
    endpoint->last_hb_id = static_cast<protocol::v1::heartbeat*>(packet.get())->packet_id;
    endpoint->state = connection_state::discovered;

    // Asynchronous publish, subscribe or unsubscribe may be nacking, in which case the timer should
    // remain active.
    if (endpoint->last_sent_packet_type == packet_type::heartbeat or
        endpoint->last_sent_packet_type == packet_type::heartbeat_nack)
        endpoint->nack_timer.cancel();

    // TODO: parse heartbeat payload
    udp_send_to(endpoint, std::make_unique<protocol::v1::ack>(packet_type::heartbeat_ack,
                                                              endpoint->last_hb_id));

    // Start heartbeat timer for the case when endpoint does not send heartbeat for twice the amount
    // of time specified in received heartbeat packet
    endpoint->heartbeat_interval =
        std::chrono::milliseconds(static_cast<protocol::v1::heartbeat*>(packet.get())->interval);
    endpoint->heartbeat_timer.expires_after(endpoint->heartbeat_interval * 2);
    endpoint->heartbeat_timer.async_wait([this, &endpoint](const boost::system::error_code& ec) {
        if (ec != asio::error::operation_aborted) send_unicast_heartbeat(endpoint);
    });
}

void server::handle_heartbeat_ack(const connection_ptr& endpoint) {
    // Asynchronous publish, subscribe or unsubscribe may be nacking, in which case the timer should
    // remain active.
    if (endpoint->last_sent_packet_type == packet_type::heartbeat or
        endpoint->last_sent_packet_type == packet_type::heartbeat_nack)
        endpoint->nack_timer.cancel();

    // Start timer for the next heartbeat.
    endpoint->heartbeat_timer.expires_after(_settings->timeouts().heartbeat);
    endpoint->heartbeat_timer.async_wait([this, &endpoint](const boost::system::error_code& ec) {
        if (ec != asio::error::operation_aborted) send_unicast_heartbeat(endpoint);
    });
}

void server::handle_heartbeat_nack(const connection_ptr& endpoint,
                                   protocol::v1::packet_ptr packet) {
    // This packet could be received in two cases:
    // 1. This bridge sent 'heartbeat' packet, it got lost (remote endpoint does not even know about
    // this packet). After timeout this bridge sends 'heartbeat_nack' and remote endpoint responds
    // with basically the same packet.
    // 2. Remote endpoint sent 'heartbeat' packet, this bridge sent 'heartbeat_ack' in response. But
    // remote endpoint hadn't received 'heartbeat_ack' from this bridge.
    if (endpoint->last_sent_packet_type == packet_type::heartbeat_nack)
        // This is the first case. This bridge should just send another heartbeat packet.
        send_unicast_heartbeat(endpoint);
    else
        // This is the second case. Send 'heartbeat_ack'.
        udp_send_to(endpoint, std::make_unique<protocol::v1::ack>(
                                  packet_type::heartbeat_ack,
                                  static_cast<protocol::v1::heartbeat*>(packet.get())->packet_id));
}

}  // namespace octopus_mq::bridge
