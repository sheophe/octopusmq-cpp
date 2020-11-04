#ifndef OCTOMQ_MQTT_TLS_WS_BROKER_H_
#define OCTOMQ_MQTT_TLS_WS_BROKER_H_

#define MQTT_USE_WS
#define MQTT_USE_TLS

#include "threads/mqtt/broker.hpp"

namespace octopus_mq::mqtt {

using connection = mqtt_cpp::server_tls_ws<>::endpoint_t;
using connection_sp = std::shared_ptr<connection>;

class tls_ws_broker final : public broker {
   public:
    tls_ws_broker();
};

}  // namespace octopus_mq::mqtt

#endif
