#ifndef OCTOMQ_MQTT_BROKER_H_
#define OCTOMQ_MQTT_BROKER_H_

#include "network/network.hpp"
#include "threads/mqtt/config.hpp"

#include "mqtt_server_cpp.hpp"

#include <set>
#include <boost/lexical_cast.hpp>
#include <boost/multi_index_container.hpp>
#include <boost/multi_index/ordered_index.hpp>
#include <boost/multi_index/member.hpp>

namespace octopus_mq::mqtt {

namespace multi_index = boost::multi_index;

struct topic_tag {};
struct connection_tag {};

class broker_base {
   protected:
    adapter_params _adapter_params;

   public:
    broker_base(const class adapter_params& adapter);

    virtual void run() = 0;
    virtual void stop() = 0;

    const adapter_params& adapter_params() const;
};

// Class Server must be one of the following:
// mqtt_cpp::server<>
// mqtt_cpp::server_ws<>
// mqtt_cpp::server_tls<>
// mqtt_cpp::server_tls_ws<>
template <class Server>
class broker : public broker_base {
    using connection = typename Server::endpoint_t;
    using connection_sp = std::shared_ptr<connection>;

    class sub_con {
       public:
        mqtt_cpp::buffer topic;
        connection_sp con;
        mqtt_cpp::qos qos_value;
        mqtt_cpp::rap rap_value;

        sub_con(mqtt_cpp::buffer topic, connection_sp con, mqtt_cpp::qos qos_value)
            : topic(std::move(topic)),
              con(std::move(con)),
              qos_value(qos_value),
              rap_value(mqtt_cpp::rap::dont) {}  // MQTT v3.1.1 constructor

        sub_con(mqtt_cpp::buffer topic, connection_sp con, mqtt_cpp::qos qos_value,
                mqtt_cpp::rap rap_value)
            : topic(std::move(topic)),
              con(std::move(con)),
              qos_value(qos_value),
              rap_value(rap_value) {}  // MQTT v5 constructor
    };

    using mi_sub_con = multi_index::multi_index_container<
        sub_con,
        multi_index::indexed_by<multi_index::ordered_non_unique<
                                    multi_index::tag<topic_tag>,  // Topic tag
                                    BOOST_MULTI_INDEX_MEMBER(sub_con, mqtt_cpp::buffer, topic)>,
                                multi_index::ordered_non_unique<  // Connection tag
                                    multi_index::tag<connection_tag>,
                                    BOOST_MULTI_INDEX_MEMBER(sub_con, connection_sp, con)>>>;

   private:
    boost::asio::io_context _ioc;
    Server _server;
    std::set<connection_sp> _connections;
    mi_sub_con _subs;

    inline void close_proc(connection_sp const& con);

   public:
    broker(const class adapter_params& adapter);

    void run();
    void stop();
};

}  // namespace octopus_mq::mqtt

#endif
