#ifndef OCTOMQ_BRIDGE_IMPLEMENTATION_H_
#define OCTOMQ_BRIDGE_IMPLEMENTATION_H_

#include <chrono>
#include <condition_variable>
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

class connection_meta {
   public:
    std::chrono::milliseconds discovery_started_timestamp;
    std::chrono::milliseconds discovery_ended_timestamp;
    std::chrono::milliseconds last_seen_timestamp;
};

class implementation final : public adapter_interface {
    std::thread _discoverer_thread;
    std::atomic<bool> _discoverer_should_stop;

    std::mutex _connections_mutex;
    std::set<connection_ptr> _connections;
    std::map<connection_ptr, connection_meta> _meta;
    std::condition_variable _connections_available_cv;
    // bool _connections_available;

    const adapter_settings_ptr _settings;

    void list_discoverer(const std::chrono::milliseconds& discovery_timeout,
                         const std::chrono::milliseconds& rescan_timeout, const port_int port,
                         const discovery_list& list);
    void range_discoverer(const std::chrono::milliseconds& discovery_timeout,
                          const std::chrono::milliseconds& rescan_timeout, const port_int port,
                          const discovery_range& range);

   public:
    implementation(const octopus_mq::adapter_settings_ptr adapter_settings,
                   message_queue& global_queue);

    void run();
    void stop();
    void inject_publish(const message_ptr message);
};

}  // namespace octopus_mq::bridge

#endif