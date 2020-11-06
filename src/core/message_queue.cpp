#include "core/message_queue.hpp"

namespace octopus_mq {

message::message(const int &packet_type) : _packet_type(packet_type) {}

message::message(message_payload &&payload) : _payload(move(payload)) {}

message::message(message_payload &&payload, const string &origin_client_id)
    : _payload(move(payload)), _origin_client_id(origin_client_id), _origin_pubopts(0) {}

message::message(message_payload &&payload, const uint8_t pubopts)
    : _payload(move(payload)), _origin_pubopts(pubopts) {}

void message::payload(const message_payload &payload) { _payload = payload; }

void message::payload(message_payload &&payload) { _payload = move(payload); }

void message::topic(const string &topic) { _topic = topic; }

void message::origin(const string &origin_client_id) { _origin_client_id = origin_client_id; }

void message::pubopts(const uint8_t pubopts) { _origin_pubopts = pubopts; }

const message_payload &message::payload() const { return _payload; }

const string &message::topic() const { return _topic; }

const string &message::origin() const { return _origin_client_id; }

uint8_t message::pubopts() const { return _origin_pubopts; }

message_notify_handle::message_notify_handle()
    : _cv(std::make_unique<std::condition_variable>()), _event(false) {}

message_notify_handle::message_notify_handle(message_notify_handle &&mnh)
    : _cv(move(mnh._cv)), _event(mnh._event) {}

message_notify_handle &message_notify_handle::operator=(message_notify_handle &&mnh) {
    if (this != &mnh) {
        _cv = move(mnh._cv);
        _event = mnh._event;
    }
    return *this;
}

void message_queue::push(const message &message) {
    std::lock_guard<std::mutex> queue_lock(_mutex);
    _queue.push(message);
}

bool message_queue::pop(message &message) {
    std::lock_guard<std::mutex> queue_lock(_mutex);
    if (_queue.empty()) return false;
    message = _queue.front();
    _queue.pop();
    return true;
}

message_notify_handle &message_queue::add_notify_handle(const string &topic) {
    std::unique_lock<std::mutex> notify_lock(_notify_mutex);
    auto iter = _notify_map.find(topic);
    if (iter == _notify_map.end()) _notify_map[topic].push_back(message_notify_handle());
    notify_lock.unlock();
    return _notify_map[topic].back();
}

std::list<message_notify_handle> &message_queue::get_notify_handles(const string &topic) {
    return _notify_map[topic];
}

}  // namespace octopus_mq
