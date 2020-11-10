#include "threads/dds/peer.hpp"

namespace octopus_mq::dds {

peer::peer(const octopus_mq::adapter_settings_ptr adapter_settings, message_pool& global_queue)
    : adapter_interface(adapter_settings, global_queue) {}

void peer::run() {}

void peer::stop() {}

void peer::inject_publish(const std::shared_ptr<message> message) { (*message); }

}  // namespace octopus_mq::dds