#ifndef OCTOMQ_MQTT_WS_BROKER_H_
#define OCTOMQ_MQTT_WS_BROKER_H_

#define MQTT_USE_WS

#include "threads/mqtt/broker.hpp"

namespace octopus_mq::mqtt {

using connection = mqtt_cpp::server_ws<>::endpoint_t;
using connection_sp = std::shared_ptr<connection>;

class ws_broker final : public broker {
   public:
    ws_broker();
};

}  // namespace octopus_mq::mqtt

#endif
