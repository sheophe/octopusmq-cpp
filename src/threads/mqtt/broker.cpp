#include "threads/mqtt/broker.hpp"
#include "core/log.hpp"

#include <iomanip>
#include <boost/asio/ip/address.hpp>

#define OCTOMQ_MQTT_BROKER_ROLE "broker"
#define OCTOMQ_MQTT_PROTOCOL_NAME "mqtt"

namespace octopus_mq::mqtt {

using namespace boost::asio;

template <typename Server>
inline void broker<Server>::close_connection(connection_sp const& con) {
    this->_connections.erase(con);
    this->_meta.erase(con);
    std::lock_guard<std::mutex> _subs_lock(this->_subs_mutex);
    auto& idx = this->_subs.template get<connection_tag>();
    auto r = idx.equal_range(con);
    idx.erase(r.first, r.second);
}

template <typename Server>
inline void broker<Server>::worker() {
    _server->listen();
    _ioc.run();
}

template <typename Server>
broker<Server>::broker(const octopus_mq::adapter_settings_ptr adapter_settings,
                       message_pool& global_queue)
    : adapter_interface(adapter_settings, global_queue) {
    // When octopus_mq::phy gets the name defined in OCTOMQ_IFACE_NAME_ANY
    // instead of correct interface name (which means any interface should be listened),
    // it stores address defined in OCTOMQ_NULL_IP as an interface IP address.
    if (_adapter_settings->phy().ip() == OCTOMQ_NULL_IP)
        _server = std::make_unique<Server>(
            ip::tcp::endpoint(ip::tcp::v4(),
                              boost::lexical_cast<uint16_t>(_adapter_settings->port())),
            _ioc);
    else
        _server = std::make_unique<Server>(
            ip::tcp::endpoint(ip::make_address(_adapter_settings->phy().ip_string()),
                              boost::lexical_cast<uint16_t>(_adapter_settings->port())),
            _ioc);

    _server->set_error_handler([](mqtt_cpp::error_code ec) {
        if (ec != boost::system::errc::operation_canceled)
            log::print(log_type::error, ec.message());
    });

    _server->set_accept_handler([this](connection_sp spep) {
        auto& ep = *spep;
        std::weak_ptr<connection> wp(spep);

        auto llre = ep.socket().lowest_layer().remote_endpoint();
        address remote_address(llre.address().to_string(), llre.port());
        _meta[spep].address = remote_address;

        // Pass spep to keep lifetime.
        // It makes sure wp.lock() never return nullptr in the handlers below
        // including close_handler and error_handler.
        ep.start_session(std::move(spep));

        using packet_id_t = typename std::remove_reference_t<decltype(ep)>::packet_id_t;

        // Set connection level handlers (lower than MQTT)
        ep.set_close_handler([this, wp]() {
            log::print(log_type::info,
                       "adapter '" + _adapter_settings->name() + "': connection closed.");
            auto sp = wp.lock();
            BOOST_ASSERT(sp);
            this->close_connection(sp);
        });

        ep.set_error_handler([this, wp](mqtt_cpp::error_code ec) {
            auto sp = wp.lock();
            BOOST_ASSERT(sp);
            std::string message;
            if (ec == boost::system::errc::bad_file_descriptor)
                message = "socket error on " + _meta[sp].client_id;
            else
                message = ec.message() + " on " + _meta[sp].client_id;
            log::print(log_type::error, "adapter '" + _adapter_settings->name() + "': " + message);
            this->close_connection(sp);
        });

        ep.set_pingreq_handler([this, wp]() {
            auto sp = wp.lock();
            BOOST_ASSERT(sp);
            log::print_event(_adapter_settings->phy(), _meta[sp].address, _meta[sp].client_id,
                             OCTOMQ_MQTT_PROTOCOL_NAME, _adapter_settings->port(),
                             OCTOMQ_MQTT_BROKER_ROLE, network_event_type::receive, "pingreq");
            sp->pingresp();
            log::print_event(_adapter_settings->phy(), _meta[sp].address, _meta[sp].client_id,
                             OCTOMQ_MQTT_PROTOCOL_NAME, _adapter_settings->port(),
                             OCTOMQ_MQTT_BROKER_ROLE, network_event_type::send, "pingresp");
            return true;
        });

        // Set handlers for MQTTv3 protocol
        ep.set_connect_handler([this, wp](mqtt_cpp::buffer client_id,
                                          mqtt_cpp::optional<mqtt_cpp::buffer> /*username*/,
                                          mqtt_cpp::optional<mqtt_cpp::buffer> /*password*/,
                                          mqtt_cpp::optional<mqtt_cpp::will>,
                                          bool /*clean_session*/, std::uint16_t /*keep_alive*/) {
            auto sp = wp.lock();
            BOOST_ASSERT(sp);
            this->_connections.insert(sp);
            this->_meta[sp].client_id = client_id;
            this->_meta[sp].protocol_version = version::v3;
            log::print_event(_adapter_settings->phy(), _meta[sp].address, _meta[sp].client_id,
                             OCTOMQ_MQTT_PROTOCOL_NAME, _adapter_settings->port(),
                             OCTOMQ_MQTT_BROKER_ROLE, network_event_type::receive, "connect");
            sp->connack(false, mqtt_cpp::connect_return_code::accepted);
            log::print_event(_adapter_settings->phy(), _meta[sp].address, _meta[sp].client_id,
                             OCTOMQ_MQTT_PROTOCOL_NAME, _adapter_settings->port(),
                             OCTOMQ_MQTT_BROKER_ROLE, network_event_type::send, "connack");
            return true;
        });

        ep.set_disconnect_handler([this, wp]() {
            auto sp = wp.lock();
            BOOST_ASSERT(sp);
            log::print_event(_adapter_settings->phy(), _meta[sp].address, _meta[sp].client_id,
                             OCTOMQ_MQTT_PROTOCOL_NAME, _adapter_settings->port(),
                             OCTOMQ_MQTT_BROKER_ROLE, network_event_type::receive, "disconnect");
            this->close_connection(sp);
        });

        ep.set_puback_handler([this, wp](packet_id_t /*packet_id*/) {
            auto sp = wp.lock();
            BOOST_ASSERT(sp);
            log::print_event(_adapter_settings->phy(), _meta[sp].address, _meta[sp].client_id,
                             OCTOMQ_MQTT_PROTOCOL_NAME, _adapter_settings->port(),
                             OCTOMQ_MQTT_BROKER_ROLE, network_event_type::receive, "puback");
            return true;
        });

        ep.set_pubrec_handler([this, wp](packet_id_t /*packet_id*/) {
            auto sp = wp.lock();
            BOOST_ASSERT(sp);
            log::print_event(_adapter_settings->phy(), _meta[sp].address, _meta[sp].client_id,
                             OCTOMQ_MQTT_PROTOCOL_NAME, _adapter_settings->port(),
                             OCTOMQ_MQTT_BROKER_ROLE, network_event_type::receive, "pubrec");
            return true;
        });

        ep.set_pubrel_handler([this, wp](packet_id_t /*packet_id*/) {
            auto sp = wp.lock();
            BOOST_ASSERT(sp);
            log::print_event(_adapter_settings->phy(), _meta[sp].address, _meta[sp].client_id,
                             OCTOMQ_MQTT_PROTOCOL_NAME, _adapter_settings->port(),
                             OCTOMQ_MQTT_BROKER_ROLE, network_event_type::receive, "pubrel");
            return true;
        });

        ep.set_pubcomp_handler([this, wp](packet_id_t /*packet_id*/) {
            auto sp = wp.lock();
            BOOST_ASSERT(sp);
            log::print_event(_adapter_settings->phy(), _meta[sp].address, _meta[sp].client_id,
                             OCTOMQ_MQTT_PROTOCOL_NAME, _adapter_settings->port(),
                             OCTOMQ_MQTT_BROKER_ROLE, network_event_type::receive, "pubcomp");
            return true;
        });

        ep.set_publish_handler([this, wp](mqtt_cpp::optional<packet_id_t> /*packet_id*/,
                                          mqtt_cpp::publish_options pubopts,
                                          mqtt_cpp::buffer topic_name, mqtt_cpp::buffer contents) {
            auto sp = wp.lock();
            BOOST_ASSERT(sp);
            log::print_event(_adapter_settings->phy(), _meta[sp].address, _meta[sp].client_id,
                             OCTOMQ_MQTT_PROTOCOL_NAME, _adapter_settings->port(),
                             OCTOMQ_MQTT_BROKER_ROLE, network_event_type::receive, "publish");
            std::unique_lock<std::mutex> _subs_lock(this->_subs_mutex);
            auto const& idx = this->_subs.template get<topic_tag>();
            auto r = idx.equal_range(topic_name);
            for (; r.first != r.second; ++r.first) {
                r.first->con->publish(topic_name, contents,
                                      std::min(r.first->qos_value, pubopts.get_qos()));
                auto llre = r.first->con->socket().lowest_layer().remote_endpoint();
                address remote_address(llre.address().to_string(), llre.port());
                log::print_event(_adapter_settings->phy(), remote_address,
                                 _meta[r.first->con].client_id, OCTOMQ_MQTT_PROTOCOL_NAME,
                                 _adapter_settings->port(), OCTOMQ_MQTT_BROKER_ROLE,
                                 network_event_type::send, "publish");
            }
            _subs_lock.unlock();
            ++(this->_meta[sp].n_publishes);
            return true;
        });

        ep.set_subscribe_handler(
            [this, wp](
                packet_id_t packet_id,
                std::vector<std::tuple<mqtt_cpp::buffer, mqtt_cpp::subscribe_options>> entries) {
                auto sp = wp.lock();
                BOOST_ASSERT(sp);
                log::print_event(_adapter_settings->phy(), _meta[sp].address, _meta[sp].client_id,
                                 OCTOMQ_MQTT_PROTOCOL_NAME, _adapter_settings->port(),
                                 OCTOMQ_MQTT_BROKER_ROLE, network_event_type::receive, "subscribe");
                std::vector<mqtt_cpp::suback_return_code> res;
                res.reserve(entries.size());
                for (auto const& e : entries) {
                    mqtt_cpp::buffer topic = std::get<0>(e);
                    mqtt_cpp::qos qos_value = std::get<1>(e).get_qos();
                    res.emplace_back(mqtt_cpp::qos_to_suback_return_code(qos_value));
                    std::lock_guard<std::mutex> _subs_lock(this->_subs_mutex);
                    this->_subs.emplace(std::move(topic), sp, qos_value);
                }
                sp->suback(packet_id, res);
                log::print_event(_adapter_settings->phy(), _meta[sp].address, _meta[sp].client_id,
                                 OCTOMQ_MQTT_PROTOCOL_NAME, _adapter_settings->port(),
                                 OCTOMQ_MQTT_BROKER_ROLE, network_event_type::send, "suback");
                return true;
            });

        ep.set_unsubscribe_handler([this, wp](packet_id_t packet_id,
                                              std::vector<mqtt_cpp::buffer> topics) {
            auto sp = wp.lock();
            BOOST_ASSERT(sp);
            log::print_event(_adapter_settings->phy(), _meta[sp].address, _meta[sp].client_id,
                             OCTOMQ_MQTT_PROTOCOL_NAME, _adapter_settings->port(),
                             OCTOMQ_MQTT_BROKER_ROLE, network_event_type::receive, "unsubscribe");
            std::unique_lock<std::mutex> _subs_lock(this->_subs_mutex);
            for (auto const& topic : topics) {
                this->_subs.erase(topic);
            }
            _subs_lock.unlock();
            sp->unsuback(packet_id);
            log::print_event(_adapter_settings->phy(), _meta[sp].address, _meta[sp].client_id,
                             OCTOMQ_MQTT_PROTOCOL_NAME, _adapter_settings->port(),
                             OCTOMQ_MQTT_BROKER_ROLE, network_event_type::send, "unsuback");
            return true;
        });

        // Set handlers for MQTTv5 protocol
        ep.set_v5_connect_handler(
            [this, wp](mqtt_cpp::buffer client_id,
                       mqtt_cpp::optional<mqtt_cpp::buffer> const& /*username*/,
                       mqtt_cpp::optional<mqtt_cpp::buffer> const& /*password*/,
                       mqtt_cpp::optional<mqtt_cpp::will>, bool /*clean_start*/,
                       std::uint16_t /*keep_alive*/, mqtt_cpp::v5::properties) {
                auto sp = wp.lock();
                BOOST_ASSERT(sp);
                log::print_event(_adapter_settings->phy(), _meta[sp].address, _meta[sp].client_id,
                                 OCTOMQ_MQTT_PROTOCOL_NAME, _adapter_settings->port(),
                                 OCTOMQ_MQTT_BROKER_ROLE, network_event_type::receive, "connect");
                this->_connections.insert(sp);
                this->_meta[sp].client_id = client_id;
                this->_meta[sp].protocol_version = version::v5;
                sp->connack(false, mqtt_cpp::v5::connect_reason_code::success);
                log::print_event(_adapter_settings->phy(), _meta[sp].address, _meta[sp].client_id,
                                 OCTOMQ_MQTT_PROTOCOL_NAME, _adapter_settings->port(),
                                 OCTOMQ_MQTT_BROKER_ROLE, network_event_type::send, "connack");
                return true;
            });

        ep.set_v5_disconnect_handler(
            [this, wp](mqtt_cpp::v5::disconnect_reason_code /*reason_code*/,
                       mqtt_cpp::v5::properties) {
                auto sp = wp.lock();
                BOOST_ASSERT(sp);
                log::print_event(_adapter_settings->phy(), _meta[sp].address, _meta[sp].client_id,
                                 OCTOMQ_MQTT_PROTOCOL_NAME, _adapter_settings->port(),
                                 OCTOMQ_MQTT_BROKER_ROLE, network_event_type::receive, "connect");
                this->close_connection(sp);
            });

        ep.set_v5_puback_handler([this, wp](packet_id_t /*packet_id*/,
                                            mqtt_cpp::v5::puback_reason_code /*reason_code*/,
                                            mqtt_cpp::v5::properties) {
            auto sp = wp.lock();
            BOOST_ASSERT(sp);
            log::print_event(_adapter_settings->phy(), _meta[sp].address, _meta[sp].client_id,
                             OCTOMQ_MQTT_PROTOCOL_NAME, _adapter_settings->port(),
                             OCTOMQ_MQTT_BROKER_ROLE, network_event_type::receive, "puback");
            return true;
        });

        ep.set_v5_pubrec_handler([this, wp](packet_id_t /*packet_id*/,
                                            mqtt_cpp::v5::pubrec_reason_code /*reason_code*/,
                                            mqtt_cpp::v5::properties) {
            auto sp = wp.lock();
            BOOST_ASSERT(sp);
            log::print_event(_adapter_settings->phy(), _meta[sp].address, _meta[sp].client_id,
                             OCTOMQ_MQTT_PROTOCOL_NAME, _adapter_settings->port(),
                             OCTOMQ_MQTT_BROKER_ROLE, network_event_type::receive, "pubrec");
            return true;
        });

        ep.set_v5_pubrel_handler([this, wp](packet_id_t /*packet_id*/,
                                            mqtt_cpp::v5::pubrel_reason_code /*reason_code*/,
                                            mqtt_cpp::v5::properties) {
            auto sp = wp.lock();
            BOOST_ASSERT(sp);
            log::print_event(_adapter_settings->phy(), _meta[sp].address, _meta[sp].client_id,
                             OCTOMQ_MQTT_PROTOCOL_NAME, _adapter_settings->port(),
                             OCTOMQ_MQTT_BROKER_ROLE, network_event_type::receive, "pubrel");
            return true;
        });

        ep.set_v5_pubcomp_handler([this, wp](packet_id_t /*packet_id*/,
                                             mqtt_cpp::v5::pubcomp_reason_code /*reason_code*/,
                                             mqtt_cpp::v5::properties) {
            auto sp = wp.lock();
            BOOST_ASSERT(sp);
            log::print_event(_adapter_settings->phy(), _meta[sp].address, _meta[sp].client_id,
                             OCTOMQ_MQTT_PROTOCOL_NAME, _adapter_settings->port(),
                             OCTOMQ_MQTT_BROKER_ROLE, network_event_type::receive, "pubcomp");
            return true;
        });

        ep.set_v5_publish_handler([this, wp](mqtt_cpp::optional<packet_id_t> /*packet_id*/,
                                             mqtt_cpp::publish_options pubopts,
                                             mqtt_cpp::buffer topic_name, mqtt_cpp::buffer contents,
                                             mqtt_cpp::v5::properties props) {
            auto sp = wp.lock();
            BOOST_ASSERT(sp);
            log::print_event(_adapter_settings->phy(), _meta[sp].address, _meta[sp].client_id,
                             OCTOMQ_MQTT_PROTOCOL_NAME, _adapter_settings->port(),
                             OCTOMQ_MQTT_BROKER_ROLE, network_event_type::receive, "publish");
            std::unique_lock<std::mutex> _subs_lock(this->_subs_mutex);
            auto const& idx = this->_subs.template get<topic_tag>();
            auto r = idx.equal_range(topic_name);
            for (; r.first != r.second; ++r.first) {
                mqtt_cpp::retain retain = (r.first->rap_value == mqtt_cpp::rap::retain)
                                              ? pubopts.get_retain()
                                              : mqtt_cpp::retain::no;
                r.first->con->publish(topic_name, contents,
                                      std::min(r.first->qos_value, pubopts.get_qos()) | retain,
                                      std::move(props));
                auto llre = r.first->con->socket().lowest_layer().remote_endpoint();
                address remote_address(llre.address().to_string(), llre.port());
                log::print_event(_adapter_settings->phy(), remote_address,
                                 _meta[r.first->con].client_id, OCTOMQ_MQTT_PROTOCOL_NAME,
                                 _adapter_settings->port(), OCTOMQ_MQTT_BROKER_ROLE,
                                 network_event_type::send, "publish");
            }
            _subs_lock.unlock();
            ++(this->_meta[sp].n_publishes);
            return true;
        });

        ep.set_v5_subscribe_handler(
            [this, wp](
                packet_id_t packet_id,
                std::vector<std::tuple<mqtt_cpp::buffer, mqtt_cpp::subscribe_options>> entries,
                mqtt_cpp::v5::properties) {
                auto sp = wp.lock();
                BOOST_ASSERT(sp);
                log::print_event(_adapter_settings->phy(), _meta[sp].address, _meta[sp].client_id,
                                 OCTOMQ_MQTT_PROTOCOL_NAME, _adapter_settings->port(),
                                 OCTOMQ_MQTT_BROKER_ROLE, network_event_type::receive, "subscribe");
                std::vector<mqtt_cpp::v5::suback_reason_code> res;
                res.reserve(entries.size());
                for (auto const& e : entries) {
                    mqtt_cpp::buffer topic = std::get<0>(e);
                    mqtt_cpp::qos qos_value = std::get<1>(e).get_qos();
                    mqtt_cpp::rap rap_value = std::get<1>(e).get_rap();
                    res.emplace_back(mqtt_cpp::v5::qos_to_suback_reason_code(qos_value));
                    std::lock_guard<std::mutex> _subs_lock(this->_subs_mutex);
                    this->_subs.emplace(std::move(topic), sp, qos_value, rap_value);
                }
                sp->suback(packet_id, res);
                log::print_event(_adapter_settings->phy(), _meta[sp].address, _meta[sp].client_id,
                                 OCTOMQ_MQTT_PROTOCOL_NAME, _adapter_settings->port(),
                                 OCTOMQ_MQTT_BROKER_ROLE, network_event_type::send, "suback");
                return true;
            });

        ep.set_v5_unsubscribe_handler([this, wp](packet_id_t packet_id,
                                                 std::vector<mqtt_cpp::buffer> topics,
                                                 mqtt_cpp::v5::properties) {
            auto sp = wp.lock();
            BOOST_ASSERT(sp);
            log::print_event(_adapter_settings->phy(), _meta[sp].address, _meta[sp].client_id,
                             OCTOMQ_MQTT_PROTOCOL_NAME, _adapter_settings->port(),
                             OCTOMQ_MQTT_BROKER_ROLE, network_event_type::receive, "subscribe");
            std::unique_lock<std::mutex> _subs_lock(this->_subs_mutex);
            for (auto const& topic : topics) {
                this->_subs.erase(topic);
            }
            _subs_lock.unlock();
            sp->unsuback(packet_id);
            log::print_event(_adapter_settings->phy(), _meta[sp].address, _meta[sp].client_id,
                             OCTOMQ_MQTT_PROTOCOL_NAME, _adapter_settings->port(),
                             OCTOMQ_MQTT_BROKER_ROLE, network_event_type::send, "unsuback");
            return true;
        });
    });
}  // namespace octopus_mq::mqtt

template <typename Server>
void broker<Server>::run() {
    _thread = std::thread(&broker<Server>::worker, this);
}

template <typename Server>
void broker<Server>::stop() {
    _server->close();
    _ioc.stop();
    if (_thread.joinable()) _thread.join();
}

template <typename Server>
void broker<Server>::inject_publish(const std::shared_ptr<message> message) {
    mqtt_cpp::buffer topic_name(std::string_view(message->topic().data(), message->topic().size()));
    mqtt_cpp::buffer contents(std::string_view(
        reinterpret_cast<const char*>(message->payload().data()), message->payload().size()));
    mqtt_cpp::publish_options pubopts(message->pubopts());

    std::lock_guard<std::mutex> _subs_lock(_subs_mutex);
    auto const& idx = _subs.template get<topic_tag>();
    auto r = idx.equal_range(topic_name);
    for (; r.first != r.second; ++r.first) {
        r.first->con->publish(topic_name, contents,
                              std::min(r.first->qos_value, pubopts.get_qos()));
    }
}

template <typename Server>
void broker<Server>::inject_publish(const std::shared_ptr<message> message,
                                    mqtt_cpp::v5::properties props) {
    mqtt_cpp::buffer topic_name(std::string_view(message->topic().data(), message->topic().size()));
    mqtt_cpp::buffer contents(std::string_view(
        reinterpret_cast<const char*>(message->payload().data()), message->payload().size()));
    mqtt_cpp::publish_options pubopts(message->pubopts());

    std::lock_guard<std::mutex> _subs_lock(_subs_mutex);
    auto const& idx = _subs.template get<topic_tag>();
    auto r = idx.equal_range(topic_name);
    for (; r.first != r.second; ++r.first) {
        mqtt_cpp::retain retain = (r.first->rap_value == mqtt_cpp::rap::retain)
                                      ? pubopts.get_retain()
                                      : mqtt_cpp::retain::no;
        r.first->con->publish(topic_name, contents,
                              std::min(r.first->qos_value, pubopts.get_qos()) | retain, props);
    }
}

template class broker<mqtt_cpp::server<>>;
template class broker<mqtt_cpp::server_ws<>>;
#ifdef OCTOMQ_ENABLE_TLS
template class broker<mqtt_cpp::server_tls<>>;
template class broker<mqtt_cpp::server_tls_ws<>>;
#endif

}  // namespace octopus_mq::mqtt