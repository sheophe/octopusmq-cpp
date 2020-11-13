#include "network/adapter.hpp"

#include "core/error.hpp"

namespace octopus_mq {

using std::string;

static const string unknwon_protocol = "(unknown)";

adapter_settings::adapter_settings(const protocol_type &protocol, const nlohmann::json &json)
    : _phy(), _port(OCTOMQ_NULL_PORT), _protocol(protocol), _generated_name(false) {
    // Parsing protocol name
    // It exists and is string. That was already checked by adapter_factory
    _protocol_name = json[OCTOMQ_ADAPTER_FIELD_PROTOCOL].get<string>();

    // Parsing interface
    if (not json.contains(OCTOMQ_ADAPTER_FIELD_INTERFACE))
        throw missing_field_error(OCTOMQ_ADAPTER_FIELD_INTERFACE);

    const nlohmann::json &interface_field = json[OCTOMQ_ADAPTER_FIELD_INTERFACE];
    if (interface_field.is_string())
        _phy = octopus_mq::phy(interface_field.get<string>());
    else
        throw field_type_error(OCTOMQ_ADAPTER_FIELD_INTERFACE);

    // Parsing port
    if (not json.contains(OCTOMQ_ADAPTER_FIELD_PORT))
        throw missing_field_error(OCTOMQ_ADAPTER_FIELD_PORT);

    const nlohmann::json &port_field = json[OCTOMQ_ADAPTER_FIELD_PORT];
    if (port_field.is_number_unsigned())
        _port = port_field.get<port_int>();
    else
        throw field_type_error(OCTOMQ_ADAPTER_FIELD_PORT);

    // Parsing optional 'name' field
    // Derived classes may additionally set the name
    if (json.contains(OCTOMQ_ADAPTER_FIELD_NAME)) {
        const nlohmann::json &name_field = json[OCTOMQ_ADAPTER_FIELD_NAME];
        if (name_field.is_string())
            _name = name_field.get<string>();
        else
            throw field_type_error(OCTOMQ_ADAPTER_FIELD_NAME);
    } else {
        _name = '[' + _phy.name() + ':' + std::to_string(_port) + "] " + _protocol_name;
        _generated_name = true;
    }
}

void adapter_settings::phy(const class phy &phy) { _phy = phy; }

void adapter_settings::phy(const string &phy) { _phy = octopus_mq::phy(phy); }

void adapter_settings::port(const port_int &port) { _port = port; }

void adapter_settings::name(const string &name) { _name = name; }

void adapter_settings::name_append(const string &appendix) {
    if (_generated_name) _name += ' ' + appendix;
}

const class phy &adapter_settings::phy() const { return _phy; }

const port_int &adapter_settings::port() const { return _port; }

const protocol_type &adapter_settings::protocol() const { return _protocol; }

const string &adapter_settings::name() const { return _name; }

const string &adapter_settings::protocol_name() const { return _protocol_name; }

bool adapter_settings::compare_binding(const ip_int ip, const port_int port) const {
    const ip_int phy_ip = _phy.ip();
    return ((ip == phy_ip) or (ip == OCTOMQ_LOOPBACK_IP) or (phy_ip == OCTOMQ_LOOPBACK_IP)) and
           (port == _port);
}

const string adapter_settings::binging_name() const {
    const address adapter_address(_phy.ip(), _port);
    return adapter_address.to_string();
}

adapter_interface::adapter_interface(const adapter_settings_ptr adapter_settings,
                                     message_queue &global_queue)
    : _adapter_settings(adapter_settings), _global_queue(global_queue) {}

void message_queue::push(const adapter_settings_ptr adapter, const message_ptr message) {
    std::unique_lock<std::mutex> queue_lock(_queue_mutex);
    _queue.push(std::make_pair(adapter, message));
    queue_lock.unlock();
    _queue_cv.notify_one();
}

bool message_queue::wait_and_pop(std::chrono::milliseconds timeout,
                                 adapter_message_pair &destination) {
    std::unique_lock<std::mutex> queue_lock(_queue_mutex);
    if (_queue_cv.wait_for(queue_lock, timeout, [this] { return not _queue.empty(); })) {
        destination = _queue.front();
        _queue.pop();
        return true;
    } else
        return false;
}

size_t message_queue::wait_and_pop_all(std::chrono::milliseconds timeout, adapter_pool &pool) {
    std::unique_lock<std::mutex> queue_lock(_queue_mutex);
    size_t popped = 0;
    if (_queue_cv.wait_for(queue_lock, timeout, [this] { return not _queue.empty(); })) {
        popped = _queue.size();
        while (not _queue.empty()) {
            adapter_message_pair item = _queue.front();
            _queue.pop();
            for (auto &adapter : pool)
                if (adapter.first != item.first) adapter.second->inject_publish(item.second);
        }
    }
    return popped;
}

}  // namespace octopus_mq
