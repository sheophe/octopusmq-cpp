#ifndef OCTOMQ_MQTT_BROKER_H_
#define OCTOMQ_MQTT_BROKER_H_

#include "threads/mqtt/config.hpp"

#include "mqtt_server_cpp.hpp"

#include <boost/lexical_cast.hpp>
#include <boost/multi_index_container.hpp>
#include <boost/multi_index/ordered_index.hpp>
#include <boost/multi_index/member.hpp>

namespace octopus_mq::mqtt {

namespace multi_index = boost::multi_index;

class broker {
   public:
    broker();
};

}  // namespace octopus_mq::mqtt

#endif
