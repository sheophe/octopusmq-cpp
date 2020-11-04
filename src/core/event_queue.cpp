#include "core/event_queue.hpp"

namespace octopus_mq {

event_queue_handle::event_queue_handle()
    : _mutex(std::make_unique<std::mutex>()),
      _queue(std::make_unique<std::queue<event>>()),
      _cv(std::make_unique<std::condition_variable>()),
      _event(false) {}

event_queue_handle::event_queue_handle(event_queue_handle &&eqh)
    : _mutex(std::move(eqh._mutex)),
      _queue(std::move(eqh._queue)),
      _cv(std::move(eqh._cv)),
      _event(eqh._event) {}

event_queue_handle &event_queue_handle::operator=(event_queue_handle &&eqh) {
    if (this != &eqh) {
        _mutex = std::move(eqh._mutex);
        _queue = std::move(eqh._queue);
        _cv = std::move(eqh._cv);
        _event = eqh._event;
    }
    return *this;
}

std::unique_ptr<std::mutex> &event_queue_handle::mutex() { return _mutex; }

std::unique_ptr<std::queue<event>> &event_queue_handle::queue() { return _queue; }

std::unique_ptr<std::condition_variable> &event_queue_handle::cv() { return _cv; }

bool &event_queue_handle::event_avaliable() { return _event; }

void event_queue_handle::event_avaliable(const bool &available) { _event = available; }

event::event(const event_type &event_type, const protocol_type &protocol_type)
    : _event_type(event_type), _protocol_type(protocol_type) {}

void event::type(const event_type &type) { _event_type = type; }

void event::protocol(const protocol_type &type) { _protocol_type = type; }

void event::topic(const class topic &topic) { _topic = topic; }

void event::topic(const string &name) { _topic.name(name); }

void event::topic(const uint16_t &id) { _topic.id(id); }

void event::topic_update(const string &name) { _topic.name(name); }

void event::topic_update(string &&name) { _topic.name(name); }

void event::topic_update(const uint16_t &id) { _topic.id(id); }

void event::payload(const unsigned char *begin, const unsigned char *end) {
    if (begin == nullptr or end == nullptr or begin >= end)
        throw std::invalid_argument("invalid payload memory read range");
    if (!_payload.empty()) _payload.clear();
    _payload.insert(_payload.begin(), begin, end);
}

void event::payload(const event_payload &payload) { _payload = payload; }

void event::payload(event_payload &&payload) { _payload = move(payload); }

const event_type &event::type() const { return _event_type; }

const protocol_type &event::protocol() const { return _protocol_type; }

const topic &event::topic() const { return _topic; }

event_queue::event_queue(const std::list<protocol_type> &protocol_list) {
    _queue_map[{ event_type::control_packet, protocol_type::mqtt }] = event_queue_handle();
    for (const auto &protocol_item : protocol_list) {
        _queue_map[{ event_type::incoming_packet, protocol_item }] = event_queue_handle();
        _queue_map[{ event_type::outgoing_packet, protocol_item }] = event_queue_handle();
    }
}

void event_queue::event_available(const event &event) {
    const std::pair<event_type, protocol_type> type_pair(event.type(), event.protocol());
    auto iter = _queue_map.find(type_pair);
    if (iter != _queue_map.end()) iter->second.event_avaliable(true);
}

void event_queue::notify(const event &event) {
    const std::pair<event_type, protocol_type> type_pair(event.type(), event.protocol());
    auto iter = _queue_map.find(type_pair);
    if (iter != _queue_map.end()) iter->second.cv()->notify_all();
}

std::unique_ptr<std::mutex> &event_queue::mutex(const event_type &type,
                                                const protocol_type &protocol) {
    const std::pair<event_type, protocol_type> type_pair(type, protocol);
    auto iter = _queue_map.find(type_pair);
    if (iter != _queue_map.end()) return iter->second.mutex();
    static const std::pair<event_type, protocol_type> default_type_pair(event_type::control_packet,
                                                                        protocol_type::mqtt);
    return _queue_map[default_type_pair].mutex();
}

std::unique_ptr<std::mutex> &event_queue::mutex(const event &event) {
    return mutex(event.type(), event.protocol());
}

void event_queue::enqueue(const event &event) {
    const std::pair<event_type, protocol_type> type_pair(event.type(), event.protocol());
    auto iter = _queue_map.find(type_pair);
    if (iter != _queue_map.end()) iter->second.queue()->push(event);
}

void event_queue::pop(const event_type &type, const protocol_type &protocol, event &event) {
    const std::pair<event_type, protocol_type> type_pair(type, protocol);
    auto iter = _queue_map.find(type_pair);
    if (iter != _queue_map.end()) {
        std::unique_lock<std::mutex> queue_lock(*mutex(type, protocol));
        iter->second.cv()->wait(queue_lock, [iter] { return iter->second.event_avaliable(); });
        event = iter->second.queue()->front();
        iter->second.queue()->pop();
        iter->second.event_avaliable(false);
    }
}

void event_queue::push(const event &event) {
    std::unique_lock<std::mutex> queue_lock(*mutex(event));
    enqueue(event);
    event_available(event);
    queue_lock.unlock();
    notify(event);
}

void event_queue::push_stop() {
    const event stop_event(event_type::stop, protocol_type::stop);
    auto iter = _queue_map.begin();
    while (iter != _queue_map.end()) {
        std::unique_lock<std::mutex> queue_lock(*iter->second.mutex());
        iter->second.queue()->push(stop_event);
        iter->second.event_avaliable(true);
        queue_lock.unlock();
        iter->second.cv()->notify_one();
        ++iter;
    }
}

}  // namespace octopus_mq
