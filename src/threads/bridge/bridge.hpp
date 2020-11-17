#ifndef OCTOMQ_BRIDGE_IMPLEMENTATION_H_
#define OCTOMQ_BRIDGE_IMPLEMENTATION_H_

#include <chrono>
#include <map>
#include <memory>
#include <mutex>
#include <set>
#include <thread>

#include "network/adapter.hpp"
#include "network/bridge/adapter.hpp"
#include "network/bridge/protocol.hpp"
#include "network/message.hpp"
#include "network/network.hpp"

namespace octopus_mq::bridge {

class endpoints_meta {
   public:
    std::chrono::milliseconds discovery_started_timestamp;
    std::chrono::milliseconds discovery_ended_timestamp;
    std::chrono::milliseconds last_seen_timestamp;
};

class implementation final : public adapter_interface {
    std::thread _discoverer_thread;
    std::mutex _endpoints_mutex;
    std::set<endpoints_ptr> _endpoints;
    std::map<endpoints_ptr, endpoints_meta> _meta;

    void discoverer(const std::chrono::milliseconds& discovery_timeout);

   public:
    implementation(const octopus_mq::adapter_settings_ptr adapter_settings,
                   message_queue& global_queue);

    void run();
    void stop();
    void inject_publish(const message_ptr message);
};

}  // namespace octopus_mq::bridge

#endif