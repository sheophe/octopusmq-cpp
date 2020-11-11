#include "threads/dds/peer.hpp"

namespace octopus_mq::dds {

peer::peer(const octopus_mq::adapter_settings_ptr adapter_settings, message_queue& global_queue)
    : adapter_interface(adapter_settings, global_queue) {}

void peer::run() {}

void peer::stop() {}

void peer::inject_publish(const message_ptr message) { (*message); }

}  // namespace octopus_mq::dds