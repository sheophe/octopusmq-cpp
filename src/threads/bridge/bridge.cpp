#include "threads/bridge/bridge.hpp"

namespace octopus_mq::bridge {

implementation::implementation(const octopus_mq::adapter_settings_ptr adapter_settings,
                               message_queue& global_queue)
    : adapter_interface(adapter_settings, global_queue) {}

void implementation::discoverer(const std::chrono::milliseconds& discovery_timeout) {
    std::this_thread::sleep_for(discovery_timeout);
}

void implementation::run() {}

void implementation::stop() {}

void implementation::inject_publish(const message_ptr message) { (*message); }

}  // namespace octopus_mq::bridge