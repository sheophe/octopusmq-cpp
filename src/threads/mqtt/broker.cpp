#include "threads/mqtt/broker.hpp"
#include "core/log.hpp"

#include <boost/asio/ip/address.hpp>

#define OCTOMQ_MQTT_CONNECT_S "connect"
#define OCTOMQ_MQTT_CONNACK_S "connack"
#define OCTOMQ_MQTT_PUBLISH_S "publish"
#define OCTOMQ_MQTT_PUBACK_S "puback"
#define OCTOMQ_MQTT_PUBREC_S "pubrec"
#define OCTOMQ_MQTT_PUBREL_S "pubrel"
#define OCTOMQ_MQTT_PUBCOMP_S "pubcomp"
#define OCTOMQ_MQTT_SUBSCRIBE_S "subscribe"
#define OCTOMQ_MQTT_SUBACK_S "suback"
#define OCTOMQ_MQTT_UNSUBSCRIBE_S "unsubscribe"
#define OCTOMQ_MQTT_UNSUBACK_S "unsuback"
#define OCTOMQ_MQTT_PINGREQ_S "pingreq"
#define OCTOMQ_MQTT_PINGRESP_S "pingresp"
#define OCTOMQ_MQTT_DISCONNECT_S "disconnect"

namespace octopus_mq::mqtt {

using namespace boost::asio;

static string boost_error_to_string(const int& error) {
    switch (error) {
        case boost::asio::error::eof:
            return "connection lost";
        case boost::asio::error::connection_aborted:
            return "connection aborted";
        case boost::asio::error::connection_refused:
            return "connection refused";
        case boost::asio::error::connection_reset:
            return "connection reset";
        case boost::asio::error::broken_pipe:
            return "broken pipe";
        case boost::asio::error::host_unreachable:
            return "no route to host";
        case boost::asio::error::message_size:
            return "message too long";
        case boost::asio::error::network_down:
            return "network is down";
        default:
            return "error " + std::to_string(error);
    }
}

template <typename Server>
inline void broker<Server>::close_connection(connection_sp const& con) {
    _connections.erase(con);
    _meta.erase(con);
    std::lock_guard<std::mutex> subs_lock(_subs_mutex);
    _connections.erase(con);
    auto& idx = _subs.template get<connection_tag>();
    auto r = idx.equal_range(con);
    idx.erase(r.first, r.second);
}

template <typename Server>
inline void broker<Server>::share(const mqtt_cpp::buffer& topic_name,
                                  const mqtt_cpp::buffer& contents,
                                  const mqtt_cpp::publish_options& pubopts,
                                  const mqtt::version version,
                                  const mqtt_cpp::v5::properties& props) {
    message_payload payload(contents.begin(), contents.end());
    std::string topic(topic_name);
    message_ptr shared_message =
        std::make_shared<message>(std::move(payload), topic, std::uint8_t(pubopts), version, props);
    _global_queue.push(_adapter_settings, shared_message);
}

template <typename Server>
inline void broker<Server>::worker() {
    _server->listen();
    _ioc.run();
}

template <typename Server>
broker<Server>::broker(const octopus_mq::adapter_settings_ptr adapter_settings,
                       message_queue& global_queue)
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
        // 'Operation cancelled' occurs when control thread stops the broker
        // in midst of some process
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
            log::print(log_type::info, _adapter_settings->name() + ": connection closed.");
            auto sp = wp.lock();
            BOOST_ASSERT(sp);
            this->close_connection(sp);
        });

        ep.set_error_handler([this, wp](mqtt_cpp::error_code ec) {
            auto sp = wp.lock();
            BOOST_ASSERT(sp);
            // Connection may be already closed by close_handler
            // In this case socket error may pop up, but that is expected
            if (_connections.find(sp) != _connections.end()) {
                std::string message =
                    boost_error_to_string(ec.value()) + " at " + _meta[sp].address.to_string();
                if (not _meta[sp].client_id.empty()) message += " (" + _meta[sp].client_id + ").";
                log::print(log_type::error, _adapter_settings->name() + ": " + message);
                this->close_connection(sp);
            }
        });

        ep.set_pingreq_handler([this, wp]() {
            auto sp = wp.lock();
            BOOST_ASSERT(sp);
            log::print_event(_adapter_settings->name(), _meta[sp].address, _meta[sp].client_id,
                             network_event_type::receive, OCTOMQ_MQTT_PINGREQ_S);
            sp->pingresp();
            log::print_event(_adapter_settings->name(), _meta[sp].address, _meta[sp].client_id,
                             network_event_type::send, OCTOMQ_MQTT_PINGRESP_S);
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
            log::print_event(_adapter_settings->name(), _meta[sp].address, _meta[sp].client_id,
                             network_event_type::receive, OCTOMQ_MQTT_CONNECT_S);
            sp->connack(false, mqtt_cpp::connect_return_code::accepted);
            log::print_event(_adapter_settings->name(), _meta[sp].address, _meta[sp].client_id,
                             network_event_type::send, OCTOMQ_MQTT_CONNACK_S);
            return true;
        });

        ep.set_disconnect_handler([this, wp]() {
            auto sp = wp.lock();
            BOOST_ASSERT(sp);
            log::print_event(_adapter_settings->name(), _meta[sp].address, _meta[sp].client_id,
                             network_event_type::receive, OCTOMQ_MQTT_DISCONNECT_S);
            this->close_connection(sp);
            return true;
        });

        ep.set_puback_handler([this, wp](packet_id_t /*packet_id*/) {
            auto sp = wp.lock();
            BOOST_ASSERT(sp);
            log::print_event(_adapter_settings->name(), _meta[sp].address, _meta[sp].client_id,
                             network_event_type::receive, OCTOMQ_MQTT_PUBACK_S);
            return true;
        });

        ep.set_pubrec_handler([this, wp](packet_id_t /*packet_id*/) {
            auto sp = wp.lock();
            BOOST_ASSERT(sp);
            log::print_event(_adapter_settings->name(), _meta[sp].address, _meta[sp].client_id,
                             network_event_type::receive, OCTOMQ_MQTT_PUBREC_S);
            return true;
        });

        ep.set_pubrel_handler([this, wp](packet_id_t /*packet_id*/) {
            auto sp = wp.lock();
            BOOST_ASSERT(sp);
            log::print_event(_adapter_settings->name(), _meta[sp].address, _meta[sp].client_id,
                             network_event_type::receive, OCTOMQ_MQTT_PUBREL_S);
            return true;
        });

        ep.set_pubcomp_handler([this, wp](packet_id_t /*packet_id*/) {
            auto sp = wp.lock();
            BOOST_ASSERT(sp);
            log::print_event(_adapter_settings->name(), _meta[sp].address, _meta[sp].client_id,
                             network_event_type::receive, OCTOMQ_MQTT_PUBCOMP_S);
            return true;
        });

        ep.set_publish_handler([this, wp](mqtt_cpp::optional<packet_id_t> /*packet_id*/,
                                          mqtt_cpp::publish_options pubopts,
                                          mqtt_cpp::buffer topic_name, mqtt_cpp::buffer contents) {
            auto sp = wp.lock();
            BOOST_ASSERT(sp);
            log::print_event(
                _adapter_settings->name(), _meta[sp].address, _meta[sp].client_id,
                network_event_type::receive,
                OCTOMQ_MQTT_PUBLISH_S " (" + log::size_to_string(contents.size()) + ')');
            std::unique_lock<std::mutex> _subs_lock(this->_subs_mutex);
            auto const& idx = this->_subs.template get<topic_tag>();
            for (auto& sub : idx) {
                if (scope::matches_filter(sub.topic_filter, topic_name)) {
                    sub.con->publish(topic_name, contents,
                                     std::min(sub.qos_value, pubopts.get_qos()));
                    auto llre = sub.con->socket().lowest_layer().remote_endpoint();
                    address remote_address(llre.address().to_string(), llre.port());
                    log::print_event(
                        _adapter_settings->name(), remote_address, _meta[sub.con].client_id,
                        network_event_type::send,
                        OCTOMQ_MQTT_PUBLISH_S " (" + log::size_to_string(contents.size()) + ')');
                }
            }
            _subs_lock.unlock();
            this->share(topic_name, contents, pubopts, mqtt::version::v3);
            return true;
        });

        ep.set_subscribe_handler(
            [this, wp](
                packet_id_t packet_id,
                std::vector<std::tuple<mqtt_cpp::buffer, mqtt_cpp::subscribe_options>> entries) {
                auto sp = wp.lock();
                BOOST_ASSERT(sp);
                log::print_event(_adapter_settings->name(), _meta[sp].address, _meta[sp].client_id,
                                 network_event_type::receive, OCTOMQ_MQTT_SUBSCRIBE_S);
                std::vector<mqtt_cpp::suback_return_code> res;
                res.reserve(entries.size());
                for (auto const& e : entries) {
                    mqtt_cpp::buffer topic_filter = std::get<0>(e);
                    mqtt_cpp::qos qos_value = std::get<1>(e).get_qos();
                    if (scope::valid_topic_filter(topic_filter)) {
                        res.emplace_back(mqtt_cpp::qos_to_suback_return_code(qos_value));
                        std::lock_guard<std::mutex> _subs_lock(this->_subs_mutex);
                        this->_subs.emplace(std::move(topic_filter), sp, qos_value);
                    } else
                        res.emplace_back(mqtt_cpp::suback_return_code::failure);
                }
                sp->suback(packet_id, res);
                log::print_event(_adapter_settings->name(), _meta[sp].address, _meta[sp].client_id,
                                 network_event_type::send, OCTOMQ_MQTT_SUBACK_S);
                return true;
            });

        ep.set_unsubscribe_handler(
            [this, wp](packet_id_t packet_id, std::vector<mqtt_cpp::buffer> topics) {
                auto sp = wp.lock();
                BOOST_ASSERT(sp);
                log::print_event(_adapter_settings->name(), _meta[sp].address, _meta[sp].client_id,
                                 network_event_type::receive, OCTOMQ_MQTT_UNSUBSCRIBE_S);
                std::unique_lock<std::mutex> _subs_lock(this->_subs_mutex);
                for (auto const& topic : topics) this->_subs.erase(topic);
                _subs_lock.unlock();
                sp->unsuback(packet_id);
                log::print_event(_adapter_settings->name(), _meta[sp].address, _meta[sp].client_id,
                                 network_event_type::send, OCTOMQ_MQTT_UNSUBACK_S);
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
                this->_connections.insert(sp);
                this->_meta[sp].client_id = client_id;
                this->_meta[sp].protocol_version = version::v5;
                log::print_event(_adapter_settings->name(), _meta[sp].address, _meta[sp].client_id,
                                 network_event_type::receive, OCTOMQ_MQTT_CONNECT_S);
                sp->connack(false, mqtt_cpp::v5::connect_reason_code::success);
                log::print_event(_adapter_settings->name(), _meta[sp].address, _meta[sp].client_id,
                                 network_event_type::send, OCTOMQ_MQTT_CONNACK_S);
                return true;
            });

        ep.set_v5_disconnect_handler(
            [this, wp](mqtt_cpp::v5::disconnect_reason_code /*reason_code*/,
                       mqtt_cpp::v5::properties) {
                auto sp = wp.lock();
                BOOST_ASSERT(sp);
                log::print_event(_adapter_settings->name(), _meta[sp].address, _meta[sp].client_id,
                                 network_event_type::receive, OCTOMQ_MQTT_DISCONNECT_S);
                this->close_connection(sp);
                return true;
            });

        ep.set_v5_puback_handler([this, wp](packet_id_t /*packet_id*/,
                                            mqtt_cpp::v5::puback_reason_code /*reason_code*/,
                                            mqtt_cpp::v5::properties) {
            auto sp = wp.lock();
            BOOST_ASSERT(sp);
            log::print_event(_adapter_settings->name(), _meta[sp].address, _meta[sp].client_id,
                             network_event_type::receive, OCTOMQ_MQTT_PUBACK_S);
            return true;
        });

        ep.set_v5_pubrec_handler([this, wp](packet_id_t /*packet_id*/,
                                            mqtt_cpp::v5::pubrec_reason_code /*reason_code*/,
                                            mqtt_cpp::v5::properties) {
            auto sp = wp.lock();
            BOOST_ASSERT(sp);
            log::print_event(_adapter_settings->name(), _meta[sp].address, _meta[sp].client_id,
                             network_event_type::receive, OCTOMQ_MQTT_PUBREC_S);
            return true;
        });

        ep.set_v5_pubrel_handler([this, wp](packet_id_t /*packet_id*/,
                                            mqtt_cpp::v5::pubrel_reason_code /*reason_code*/,
                                            mqtt_cpp::v5::properties) {
            auto sp = wp.lock();
            BOOST_ASSERT(sp);
            log::print_event(_adapter_settings->name(), _meta[sp].address, _meta[sp].client_id,
                             network_event_type::receive, OCTOMQ_MQTT_PUBREL_S);
            return true;
        });

        ep.set_v5_pubcomp_handler([this, wp](packet_id_t /*packet_id*/,
                                             mqtt_cpp::v5::pubcomp_reason_code /*reason_code*/,
                                             mqtt_cpp::v5::properties) {
            auto sp = wp.lock();
            BOOST_ASSERT(sp);
            log::print_event(_adapter_settings->name(), _meta[sp].address, _meta[sp].client_id,
                             network_event_type::receive, OCTOMQ_MQTT_PUBCOMP_S);
            return true;
        });

        ep.set_v5_publish_handler([this, wp](mqtt_cpp::optional<packet_id_t> /*packet_id*/,
                                             mqtt_cpp::publish_options pubopts,
                                             mqtt_cpp::buffer topic_name, mqtt_cpp::buffer contents,
                                             mqtt_cpp::v5::properties props) {
            auto sp = wp.lock();
            BOOST_ASSERT(sp);
            log::print_event(
                _adapter_settings->name(), _meta[sp].address, _meta[sp].client_id,
                network_event_type::receive,
                OCTOMQ_MQTT_PUBLISH_S " (" + log::size_to_string(contents.size()) + ')');
            std::unique_lock<std::mutex> _subs_lock(this->_subs_mutex);
            auto const& idx = this->_subs.template get<topic_tag>();
            for (auto& sub : idx) {
                if (scope::matches_filter(sub.topic_filter, topic_name)) {
                    mqtt_cpp::retain retain = (sub.rap_value == mqtt_cpp::rap::retain)
                                                  ? pubopts.get_retain()
                                                  : mqtt_cpp::retain::no;
                    sub.con->publish(topic_name, contents,
                                     std::min(sub.qos_value, pubopts.get_qos()) | retain, props);
                    auto llre = sub.con->socket().lowest_layer().remote_endpoint();
                    address remote_address(llre.address().to_string(), llre.port());
                    log::print_event(
                        _adapter_settings->name(), remote_address, _meta[sub.con].client_id,
                        network_event_type::send,
                        OCTOMQ_MQTT_PUBLISH_S " (" + log::size_to_string(contents.size()) + ')');
                }
            }
            _subs_lock.unlock();
            this->share(topic_name, contents, pubopts, mqtt::version::v5, props);
            return true;
        });

        ep.set_v5_subscribe_handler(
            [this, wp](
                packet_id_t packet_id,
                std::vector<std::tuple<mqtt_cpp::buffer, mqtt_cpp::subscribe_options>> entries,
                mqtt_cpp::v5::properties) {
                auto sp = wp.lock();
                BOOST_ASSERT(sp);
                log::print_event(_adapter_settings->name(), _meta[sp].address, _meta[sp].client_id,
                                 network_event_type::receive, OCTOMQ_MQTT_SUBSCRIBE_S);
                std::vector<mqtt_cpp::v5::suback_reason_code> res;
                res.reserve(entries.size());
                for (auto const& e : entries) {
                    mqtt_cpp::buffer topic_filter = std::get<0>(e);
                    if (scope::valid_topic_filter(topic_filter)) {
                        mqtt_cpp::qos qos_value = std::get<1>(e).get_qos();
                        mqtt_cpp::rap rap_value = std::get<1>(e).get_rap();
                        res.emplace_back(mqtt_cpp::v5::qos_to_suback_reason_code(qos_value));
                        std::lock_guard<std::mutex> _subs_lock(this->_subs_mutex);
                        this->_subs.emplace(std::move(topic_filter), sp, qos_value, rap_value);
                    } else
                        res.emplace_back(mqtt_cpp::v5::suback_reason_code::topic_filter_invalid);
                }
                sp->suback(packet_id, res);
                log::print_event(_adapter_settings->name(), _meta[sp].address, _meta[sp].client_id,
                                 network_event_type::send, OCTOMQ_MQTT_SUBACK_S);
                return true;
            });

        ep.set_v5_unsubscribe_handler([this, wp](packet_id_t packet_id,
                                                 std::vector<mqtt_cpp::buffer> topics,
                                                 mqtt_cpp::v5::properties) {
            auto sp = wp.lock();
            BOOST_ASSERT(sp);
            log::print_event(_adapter_settings->name(), _meta[sp].address, _meta[sp].client_id,
                             network_event_type::receive, OCTOMQ_MQTT_UNSUBSCRIBE_S);
            std::unique_lock<std::mutex> _subs_lock(this->_subs_mutex);
            for (auto const& topic : topics) this->_subs.erase(topic);
            _subs_lock.unlock();
            sp->unsuback(packet_id);
            log::print_event(_adapter_settings->name(), _meta[sp].address, _meta[sp].client_id,
                             network_event_type::send, OCTOMQ_MQTT_UNSUBACK_S);
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
    if (_thread.joinable()) {
        _ioc.stop();
        _server->close();
        _thread.join();
    }
}

template <typename Server>
void broker<Server>::inject_publish(const message_ptr message) {
    mqtt_cpp::buffer topic_name(std::string_view(message->topic().data(), message->topic().size()));
    mqtt_cpp::buffer contents(
        std::string_view(message->payload().data(), message->payload().size()));
    mqtt_cpp::publish_options pubopts(message->pubopts());

    std::lock_guard<std::mutex> _subs_lock(_subs_mutex);
    auto const& idx = this->_subs.template get<topic_tag>();
    for (auto& sub : idx) {
        if (scope::matches_filter(sub.topic_filter, topic_name)) {
            if (message->mqtt_version() == mqtt::version::v3)
                sub.con->publish(topic_name, contents, std::min(sub.qos_value, pubopts.get_qos()));
            else {
                mqtt_cpp::retain retain = (sub.rap_value == mqtt_cpp::rap::retain)
                                              ? pubopts.get_retain()
                                              : mqtt_cpp::retain::no;
                sub.con->publish(topic_name, contents,
                                 std::min(sub.qos_value, pubopts.get_qos()) | retain,
                                 message->props());
            }
            auto llre = sub.con->socket().lowest_layer().remote_endpoint();
            address remote_address(llre.address().to_string(), llre.port());
            log::print_event(
                _adapter_settings->name(), remote_address, _meta[sub.con].client_id,
                network_event_type::send,
                OCTOMQ_MQTT_PUBLISH_S " (" + log::size_to_string(contents.size()) + ')');
        }
    }
}

template class broker<mqtt_cpp::server<>>;
template class broker<mqtt_cpp::server_ws<>>;
#ifdef OCTOMQ_ENABLE_TLS
template class broker<mqtt_cpp::server_tls<>>;
template class broker<mqtt_cpp::server_tls_ws<>>;
#endif

}  // namespace octopus_mq::mqtt