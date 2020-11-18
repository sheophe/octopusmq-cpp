#ifndef OCTOMQ_MQTT_BROKER_H_
#define OCTOMQ_MQTT_BROKER_H_

#include "network/adapter.hpp"
#include "network/message.hpp"
#include "network/mqtt/adapter.hpp"
#include "network/network.hpp"
#include "threads/mqtt/config.hpp"

#include "mqtt_server_cpp.hpp"

#include <algorithm>
#include <memory>
#include <mutex>
#include <set>
#include <thread>

#include <boost/lexical_cast.hpp>
#include <boost/multi_index_container.hpp>
#include <boost/multi_index/ordered_index.hpp>
#include <boost/multi_index/member.hpp>
#include <boost/static_assert.hpp>

namespace octopus_mq::mqtt {

namespace packet_names {

    constexpr char connect[] = "connect";
    constexpr char connack[] = "connack";
    constexpr char publish[] = "publish";
    constexpr char puback[] = "puback";
    constexpr char pubrec[] = "pubrec";
    constexpr char pubrel[] = "pubrel";
    constexpr char pubcomp[] = "pubcomp";
    constexpr char subscribe[] = "subscribe";
    constexpr char suback[] = "suback";
    constexpr char unsubscribe[] = "unsubscribe";
    constexpr char unsuback[] = "unsuback";
    constexpr char pingreq[] = "pingreq";
    constexpr char pingresp[] = "pingresp";
    constexpr char disconnect[] = "disconnect";

}  // namespace packet_names

namespace multi_index = boost::multi_index;

struct topic_tag {};
struct connection_tag {};
struct topic_connection_tag {};

struct metadata {
    address address;
    std::string client_id;
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
        mqtt_cpp::buffer topic_filter;
        connection_sp con;
        mqtt_cpp::qos qos_value;
        mqtt_cpp::rap rap_value;
        mqtt_cpp::nl nl_value;

        subscription(mqtt_cpp::buffer topic_filter, connection_sp con, mqtt_cpp::qos qos_value)
            : topic_filter(std::move(topic_filter)),
              con(std::move(con)),
              qos_value(qos_value),
              rap_value(mqtt_cpp::rap::dont),
              nl_value(mqtt_cpp::nl::no) {}  // MQTT v3 constructor

        subscription(mqtt_cpp::buffer topic_filter, connection_sp con, mqtt_cpp::qos qos_value,
                     mqtt_cpp::rap rap_value, mqtt_cpp::nl nl_value)
            : topic_filter(std::move(topic_filter)),
              con(std::move(con)),
              qos_value(qos_value),
              rap_value(rap_value),
              nl_value(nl_value) {}  // MQTT v5 constructor
    };

    using subscription_container = multi_index::multi_index_container<
        subscription,
        multi_index::indexed_by<
            multi_index::ordered_non_unique<  // Topic index
                multi_index::tag<topic_tag>,
                BOOST_MULTI_INDEX_MEMBER(subscription, mqtt_cpp::buffer, topic_filter)>,
            multi_index::ordered_non_unique<  // Connection index
                multi_index::tag<connection_tag>,
                BOOST_MULTI_INDEX_MEMBER(subscription, connection_sp, con)>,
            // Don't allow the same connection object to have the same topic multiple times.
            // Note that this index does not get used by any code in the broker
            // other than to enforce the uniqueness constraints.
            multi_index::ordered_unique<
                multi_index::tag<topic_connection_tag>,
                multi_index::composite_key<
                    subscription, BOOST_MULTI_INDEX_MEMBER(subscription, connection_sp, con),
                    BOOST_MULTI_INDEX_MEMBER(subscription, mqtt_cpp::buffer, topic_filter)>>>>;

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

    inline void share(const mqtt_cpp::buffer& topic_name, const mqtt_cpp::buffer& contents,
                      const mqtt_cpp::publish_options& pubopts, const address& origin_address,
                      const std::string& origin_clid,
                      const mqtt::version version = mqtt::version::v3,
                      const mqtt_cpp::v5::properties& props = mqtt_cpp::v5::properties());

   public:
    broker(const octopus_mq::adapter_settings_ptr adapter_settings, message_queue& global_queue);

    void run();
    void stop();

    void inject_publish(const message_ptr message);
};

}  // namespace octopus_mq::mqtt

#endif
