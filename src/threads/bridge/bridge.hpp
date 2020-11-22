#ifndef OCTOMQ_BRIDGE_IMPLEMENTATION_H_
#define OCTOMQ_BRIDGE_IMPLEMENTATION_H_

#include "network/adapter.hpp"
#include "network/bridge/adapter.hpp"
#include "network/bridge/protocol.hpp"
#include "network/bridge/server.hpp"
#include "network/message.hpp"
#include "network/network.hpp"

#include <memory>
#include <mutex>
#include <set>
#include <thread>

#include <boost/asio.hpp>

namespace octopus_mq::bridge {

class implementation final : public adapter_interface {
    const adapter_settings_ptr _settings;
    boost::asio::io_context _ioc;
    std::unique_ptr<server> _server;
    std::thread _thread;
    std::mutex _connections_mutex;
    std::set<connection_ptr> _connections;

    void worker();

   public:
    implementation(const octopus_mq::adapter_settings_ptr adapter_settings,
                   message_queue& global_queue);

    void run();
    void stop();
    void inject_publish(const message_ptr message);
};

}  // namespace octopus_mq::bridge

#endif