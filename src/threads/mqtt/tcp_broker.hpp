#ifndef OCTOMQ_MQTT_TCP_BROKER_H_
#define OCTOMQ_MQTT_TCP_BROKER_H_

#include "threads/mqtt/broker.hpp"

namespace octopus_mq::mqtt {

using connection = mqtt_cpp::server<>::endpoint_t;
using connection_sp = std::shared_ptr<connection>;

class tcp_broker final : public broker {
   public:
    tcp_broker();
};

}  // namespace octopus_mq::mqtt

#endif
