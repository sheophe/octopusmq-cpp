#include "threads/mqtt/broker.hpp"
#include "core/log.hpp"

#include <iomanip>

namespace octopus_mq::mqtt {

template <typename Server>
inline void broker<Server>::close_connection(connection_sp const& con) {
    this->_connections.erase(con);
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
    if (_adapter_settings->phy().ip() == OCTOMQ_NULL_IP) {
        _server =
            std::make_unique<Server>(boost::asio::ip::tcp::endpoint(
                                         boost::asio::ip::tcp::v4(),
                                         boost::lexical_cast<uint16_t>(_adapter_settings->port())),
                                     _ioc);
    } else {
        boost::asio::ip::tcp::resolver resolver(_ioc);
        _server =
            std::make_unique<Server>(*resolver.resolve((_adapter_settings->phy().ip_string(),
                                                        std::to_string(_adapter_settings->port()))),
                                     _ioc);
    }

    _server->set_error_handler(
        [](mqtt_cpp::error_code ec) { log::print(log_type::error, ec.message()); });

    _server->set_accept_handler([this](connection_sp spep) {
        auto& ep = *spep;
        std::weak_ptr<connection> wp(spep);

        log::print(log_type::info,
                   "adapter '" + _adapter_settings->name() + "': new connection accepted.");
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
            log::print(log_type::error,
                       "adapter '" + _adapter_settings->name() + "': " + ec.message());
            auto sp = wp.lock();
            BOOST_ASSERT(sp);
            this->close_connection(sp);
        });

        ep.set_pingreq_handler([wp]() {
            auto sp = wp.lock();
            BOOST_ASSERT(sp);
            sp->pingresp();
            return true;
        });

        // Set handlers for MQTTv3 protocol
        ep.set_connect_handler([this, wp](mqtt_cpp::buffer client_id,
                                          mqtt_cpp::optional<mqtt_cpp::buffer> username,
                                          mqtt_cpp::optional<mqtt_cpp::buffer> password,
                                          mqtt_cpp::optional<mqtt_cpp::will>, bool clean_session,
                                          std::uint16_t keep_alive) {
            using namespace mqtt_cpp::literals;
            std::cout << "[server] client_id    : " << client_id << std::endl;
            std::cout << "[server] username     : " << (username ? username.value() : "none"_mb)
                      << std::endl;
            std::cout << "[server] password     : " << (password ? password.value() : "none"_mb)
                      << std::endl;
            std::cout << "[server] clean_session: " << std::boolalpha << clean_session << std::endl;
            std::cout << "[server] keep_alive   : " << keep_alive << std::endl;
            auto sp = wp.lock();
            BOOST_ASSERT(sp);
            this->_connections.insert(sp);
            sp->connack(false, mqtt_cpp::connect_return_code::accepted);
            return true;
        });

        ep.set_disconnect_handler([this, wp]() {
            std::cout << "[server] disconnect received." << std::endl;
            auto sp = wp.lock();
            BOOST_ASSERT(sp);
            this->close_connection(sp);
        });

        ep.set_puback_handler([](packet_id_t packet_id) {
            std::cout << "[server] puback received. packet_id: " << packet_id << std::endl;
            return true;
        });

        ep.set_pubrec_handler([](packet_id_t packet_id) {
            std::cout << "[server] pubrec received. packet_id: " << packet_id << std::endl;
            return true;
        });

        ep.set_pubrel_handler([](packet_id_t packet_id) {
            std::cout << "[server] pubrel received. packet_id: " << packet_id << std::endl;
            return true;
        });

        ep.set_pubcomp_handler([](packet_id_t packet_id) {
            std::cout << "[server] pubcomp received. packet_id: " << packet_id << std::endl;
            return true;
        });

        ep.set_publish_handler([this](mqtt_cpp::optional<packet_id_t> packet_id,
                                      mqtt_cpp::publish_options pubopts,
                                      mqtt_cpp::buffer topic_name, mqtt_cpp::buffer contents) {
            std::cout << "[server] publish received."
                      << " dup: " << pubopts.get_dup() << " qos: " << pubopts.get_qos()
                      << " retain: " << pubopts.get_retain() << std::endl;
            if (packet_id) std::cout << "[server] packet_id: " << *packet_id << std::endl;
            std::cout << "[server] topic_name: " << topic_name << std::endl;
            std::cout << "[server] contents: " << contents << std::endl;
            auto const& idx = this->_subs.template get<topic_tag>();
            auto r = idx.equal_range(topic_name);
            for (; r.first != r.second; ++r.first) {
                r.first->con->publish(topic_name, contents,
                                      std::min(r.first->qos_value, pubopts.get_qos()));
            }
            return true;
        });

        ep.set_subscribe_handler(
            [this, wp](
                packet_id_t packet_id,
                std::vector<std::tuple<mqtt_cpp::buffer, mqtt_cpp::subscribe_options>> entries) {
                std::cout << "[server] subscribe received. packet_id: " << packet_id << std::endl;
                std::vector<mqtt_cpp::suback_return_code> res;
                res.reserve(entries.size());
                auto sp = wp.lock();
                BOOST_ASSERT(sp);
                for (auto const& e : entries) {
                    mqtt_cpp::buffer topic = std::get<0>(e);
                    mqtt_cpp::qos qos_value = std::get<1>(e).get_qos();
                    std::cout << "[server] topic: " << topic << " qos: " << qos_value << std::endl;
                    res.emplace_back(mqtt_cpp::qos_to_suback_return_code(qos_value));
                    std::lock_guard<std::mutex> _subs_lock(this->_subs_mutex);
                    this->_subs.emplace(std::move(topic), sp, qos_value);
                }
                sp->suback(packet_id, res);
                return true;
            });

        ep.set_unsubscribe_handler(
            [this, wp](packet_id_t packet_id, std::vector<mqtt_cpp::buffer> topics) {
                std::cout << "[server] unsubscribe received. packet_id: " << packet_id << std::endl;
                std::unique_lock<std::mutex> _subs_lock(this->_subs_mutex);
                for (auto const& topic : topics) {
                    this->_subs.erase(topic);
                }
                _subs_lock.unlock();
                auto sp = wp.lock();
                BOOST_ASSERT(sp);
                sp->unsuback(packet_id);
                return true;
            });

        // Set handlers for MQTTv5 protocol
        ep.set_v5_connect_handler([this, wp](mqtt_cpp::buffer client_id,
                                             mqtt_cpp::optional<mqtt_cpp::buffer> const& username,
                                             mqtt_cpp::optional<mqtt_cpp::buffer> const& password,
                                             mqtt_cpp::optional<mqtt_cpp::will>, bool clean_start,
                                             std::uint16_t keep_alive, mqtt_cpp::v5::properties) {
            using namespace mqtt_cpp::literals;
            std::cout << "[server] client_id    : " << client_id << std::endl;
            std::cout << "[server] username     : " << (username ? username.value() : "none"_mb)
                      << std::endl;
            std::cout << "[server] password     : " << (password ? password.value() : "none"_mb)
                      << std::endl;
            std::cout << "[server] clean_start  : " << std::boolalpha << clean_start << std::endl;
            std::cout << "[server] keep_alive   : " << keep_alive << std::endl;
            auto sp = wp.lock();
            BOOST_ASSERT(sp);
            this->_connections.insert(sp);
            sp->connack(false, mqtt_cpp::v5::connect_reason_code::success);
            return true;
        });

        ep.set_v5_disconnect_handler(
            [this, wp](mqtt_cpp::v5::disconnect_reason_code reason_code, mqtt_cpp::v5::properties) {
                std::cout << "[server] disconnect received."
                          << " reason_code: " << reason_code << std::endl;
                auto sp = wp.lock();
                BOOST_ASSERT(sp);
                this->close_connection(sp);
            });

        ep.set_v5_puback_handler([](packet_id_t packet_id,
                                    mqtt_cpp::v5::puback_reason_code reason_code,
                                    mqtt_cpp::v5::properties) {
            std::cout << "[server] puback received. packet_id: " << packet_id
                      << " reason_code: " << reason_code << std::endl;
            return true;
        });

        ep.set_v5_pubrec_handler([](packet_id_t packet_id,
                                    mqtt_cpp::v5::pubrec_reason_code reason_code,
                                    mqtt_cpp::v5::properties) {
            std::cout << "[server] pubrec received. packet_id: " << packet_id
                      << " reason_code: " << reason_code << std::endl;
            return true;
        });

        ep.set_v5_pubrel_handler([](packet_id_t packet_id,
                                    mqtt_cpp::v5::pubrel_reason_code reason_code,
                                    mqtt_cpp::v5::properties) {
            std::cout << "[server] pubrel received. packet_id: " << packet_id
                      << " reason_code: " << reason_code << std::endl;
            return true;
        });

        ep.set_v5_pubcomp_handler([](packet_id_t packet_id,
                                     mqtt_cpp::v5::pubcomp_reason_code reason_code,
                                     mqtt_cpp::v5::properties) {
            std::cout << "[server] pubcomp received. packet_id: " << packet_id
                      << " reason_code: " << reason_code << std::endl;
            return true;
        });

        ep.set_v5_publish_handler([this](mqtt_cpp::optional<packet_id_t> packet_id,
                                         mqtt_cpp::publish_options pubopts,
                                         mqtt_cpp::buffer topic_name, mqtt_cpp::buffer contents,
                                         mqtt_cpp::v5::properties props) {
            std::cout << "[server] publish received."
                      << " dup: " << pubopts.get_dup() << " qos: " << pubopts.get_qos()
                      << " retain: " << pubopts.get_retain() << std::endl;
            if (packet_id) std::cout << "[server] packet_id: " << *packet_id << std::endl;
            std::cout << "[server] topic_name: " << topic_name << std::endl;
            std::cout << "[server] contents: " << contents << std::endl;
            std::lock_guard<std::mutex> _subs_lock(this->_subs_mutex);
            auto const& idx = this->_subs.template get<topic_tag>();
            auto r = idx.equal_range(topic_name);
            for (; r.first != r.second; ++r.first) {
                mqtt_cpp::retain retain = (r.first->rap_value == mqtt_cpp::rap::retain)
                                              ? pubopts.get_retain()
                                              : mqtt_cpp::retain::no;
                r.first->con->publish(topic_name, contents,
                                      std::min(r.first->qos_value, pubopts.get_qos()) | retain,
                                      std::move(props));
            }
            return true;
        });

        ep.set_v5_subscribe_handler(
            [this, wp](
                packet_id_t packet_id,
                std::vector<std::tuple<mqtt_cpp::buffer, mqtt_cpp::subscribe_options>> entries,
                mqtt_cpp::v5::properties) {
                std::cout << "[server] subscribe received. packet_id: " << packet_id << std::endl;
                std::vector<mqtt_cpp::v5::suback_reason_code> res;
                res.reserve(entries.size());
                auto sp = wp.lock();
                BOOST_ASSERT(sp);
                for (auto const& e : entries) {
                    mqtt_cpp::buffer topic = std::get<0>(e);
                    mqtt_cpp::qos qos_value = std::get<1>(e).get_qos();
                    mqtt_cpp::rap rap_value = std::get<1>(e).get_rap();
                    std::cout << "[server] topic: " << topic << " qos: " << qos_value
                              << " rap: " << rap_value << std::endl;
                    res.emplace_back(mqtt_cpp::v5::qos_to_suback_reason_code(qos_value));
                    std::lock_guard<std::mutex> _subs_lock(this->_subs_mutex);
                    this->_subs.emplace(std::move(topic), sp, qos_value, rap_value);
                }
                sp->suback(packet_id, res);
                return true;
            });

        ep.set_v5_unsubscribe_handler([this, wp](packet_id_t packet_id,
                                                 std::vector<mqtt_cpp::buffer> topics,
                                                 mqtt_cpp::v5::properties) {
            std::cout << "[server] unsubscribe received. packet_id: " << packet_id << std::endl;
            std::unique_lock<std::mutex> _subs_lock(this->_subs_mutex);
            for (auto const& topic : topics) {
                this->_subs.erase(topic);
            }
            _subs_lock.unlock();
            auto sp = wp.lock();
            BOOST_ASSERT(sp);
            sp->unsuback(packet_id);
            return true;
        });
    });
}

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