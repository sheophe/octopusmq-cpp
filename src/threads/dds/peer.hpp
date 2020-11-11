#ifndef OCTOMQ_DDS_PEER_H_
#define OCTOMQ_DDS_PEER_H_

#include "core/message.hpp"
#include "core/topic.hpp"
#include "network/dds/adapter.hpp"
#include "network/adapter.hpp"
#include "network/network.hpp"

namespace octopus_mq::dds {

class peer final : public adapter_interface {
   public:
    peer(const octopus_mq::adapter_settings_ptr adapter_settings, message_queue& global_queue);

    void run();
    void stop();
    void inject_publish(const message_ptr message);
};

}  // namespace octopus_mq::dds

#endif
