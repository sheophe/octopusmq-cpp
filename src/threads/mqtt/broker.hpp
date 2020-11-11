#ifndef OCTOMQ_MQTT_BROKER_H_
#define OCTOMQ_MQTT_BROKER_H_

#include "core/message.hpp"
#include "core/topic.hpp"
#include "network/mqtt/adapter.hpp"
#include "network/adapter.hpp"
#include "network/network.hpp"
#include "threads/mqtt/config.hpp"

#include "mqtt_server_cpp.hpp"

#include <set>
#include <mutex>
#include <thread>
#include <boost/lexical_cast.hpp>
#include <boost/multi_index_container.hpp>
#include <boost/multi_index/ordered_index.hpp>
#include <boost/multi_index/member.hpp>
#include <boost/static_assert.hpp>

namespace octopus_mq::mqtt {

namespace multi_index = boost::multi_index;

struct topic_tag {};
struct connection_tag {};

struct metadata {
    address address;
    std::string client_id;
    uint64_t n_publishes;
    mqtt::version protocol_version;
};

// Class Server must be one of the following:
// mqtt_cpp::server<>
// mqtt_cpp::server_ws<>
// mqtt_cpp::server_tls<>
// mqtt_cpp::server_tls_ws<>
template <typename Server>
class broker final : public adapter_interface {
    static_assert(std::is_same<Server, mqtt_cpp::server<>>::value or
                      std::is_same<Server, mqtt_cpp::server_ws<>>::value,
                  "unsupported server class");

#ifdef OCTOMQ_ENABLE_TLS
    static_assert(std::is_same<Server, mqtt_cpp::server_tls<>>::value or
                      std::is_same<Server, mqtt_cpp::server_tls_ws<>>::value,
                  "unsupported server class");
#endif

    using connection = typename Server::endpoint_t;
    using connection_sp = std::shared_ptr<connection>;

    class subscription {
       public:
        mqtt_cpp::buffer topic;
        connection_sp con;
        mqtt_cpp::qos qos_value;
        mqtt_cpp::rap rap_value;

        subscription(mqtt_cpp::buffer topic, connection_sp con, mqtt_cpp::qos qos_value)
            : topic(std::move(topic)),
              con(std::move(con)),
              qos_value(qos_value),
              rap_value(mqtt_cpp::rap::dont) {}  // MQTT v3 constructor

        subscription(mqtt_cpp::buffer topic, connection_sp con, mqtt_cpp::qos qos_value,
                     mqtt_cpp::rap rap_value)
            : topic(std::move(topic)),
              con(std::move(con)),
              qos_value(qos_value),
              rap_value(rap_value) {}  // MQTT v5 constructor
    };

    using subscription_container = multi_index::multi_index_container<
        subscription, multi_index::indexed_by<
                          multi_index::ordered_non_unique<  // Topic index
                              multi_index::tag<topic_tag>,
                              BOOST_MULTI_INDEX_MEMBER(subscription, mqtt_cpp::buffer, topic)>,
                          multi_index::ordered_non_unique<  // Connection index
                              multi_index::tag<connection_tag>,
                              BOOST_MULTI_INDEX_MEMBER(subscription, connection_sp, con)>>>;

   private:
    boost::asio::io_context _ioc;
    std::unique_ptr<Server> _server;
    std::thread _thread;
    std::set<connection_sp> _connections;
    std::map<connection_sp, struct metadata> _meta;
    subscription_container _subs;
    std::mutex _subs_mutex;

    inline void close_connection(connection_sp const& con);
    inline void worker();

   public:
    broker(const octopus_mq::adapter_settings_ptr adapter_settings, message_queue& global_queue);

    void run();
    void stop();

    void inject_publish(const message_ptr message);
    void inject_publish(const message_ptr message, mqtt_cpp::v5::properties props);
};

}  // namespace octopus_mq::mqtt

#endif
