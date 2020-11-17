#ifndef OCTOMQ_ADAPTER_H_
#define OCTOMQ_ADAPTER_H_

#include <condition_variable>
#include <list>
#include <map>
#include <memory>
#include <mutex>
#include <queue>
#include <string>
#include <thread>

#include "json.hpp"
#include "network/message.hpp"
#include "network/network.hpp"

namespace octopus_mq {

namespace adapter {

    namespace field_name {

        constexpr char interface[] = "interface";
        constexpr char protocol[] = "protocol";
        constexpr char port[] = "port";
        constexpr char transport[] = "transport";
        constexpr char scope[] = "scope";
        constexpr char domain[] = "domain";
        constexpr char role[] = "role";
        constexpr char qos[] = "qos";
        constexpr char name[] = "name";
        constexpr char security[] = "security";
        constexpr char certificate[] = "certificate";

    }  // namespace field_name

}  // namespace adapter

using std::string, std::shared_ptr;

class adapter_settings;
using adapter_settings_parser_item = nlohmann::detail::iter_impl<const nlohmann::json>;
using adapter_settings_parser =
    std::map<string, void (*)(adapter_settings *, const adapter_settings_parser_item &)>;

class adapter_settings {
    phy _phy;
    port_int _port;
    protocol_type _protocol;
    string _protocol_name;
    scope _scope;
    string _name;
    bool _generated_name;

   public:
    adapter_settings(const protocol_type &protocol, const nlohmann::json &json);

    void phy(const class phy &phy);
    void phy(const string &phy);
    void port(const port_int &port);
    void scope(const class scope &scope);
    void name(const string &name);
    void name_append(const string &appendix);  // Only works with generated names

    const class phy &phy() const;
    const port_int &port() const;
    const protocol_type &protocol() const;
    const string &protocol_name() const;
    const class scope &scope() const;
    const string &name() const;

    bool compare_binding(const ip_int ip, const port_int port) const;
    const string binging_name() const;
};

using adapter_settings_ptr = std::shared_ptr<adapter_settings>;
using adapter_settings_const_ptr = std::shared_ptr<const adapter_settings>;

class message_queue;

class adapter_interface {
   protected:
    adapter_settings_ptr _adapter_settings;
    // Lifetime of message queue referenced by _global_queue should be greater than lifetime of
    // adapter interface instance.
    message_queue &_global_queue;

   public:
    adapter_interface(const adapter_settings_ptr adapter_settings, message_queue &global_queue);

    virtual void run() = 0;
    virtual void stop() = 0;
    virtual void inject_publish(const message_ptr message) = 0;
    adapter_settings_const_ptr settings() const;
};

using adapter_iface_ptr = std::shared_ptr<adapter_interface>;
using adapter_pool = std::vector<std::pair<adapter_settings_ptr, adapter_iface_ptr>>;
using adapter_message_pair = std::pair<adapter_settings_ptr, message_ptr>;

class message_queue {
    std::queue<adapter_message_pair> _queue;
    std::mutex _queue_mutex;
    std::condition_variable _queue_cv;

   public:
    void push(const adapter_settings_ptr adapter, const message_ptr message);
    bool wait_and_pop(std::chrono::milliseconds timeout, adapter_message_pair &destination);
    size_t wait_and_pop_all(std::chrono::milliseconds timeout, adapter_pool &pool);
};

}  // namespace octopus_mq

#endif
