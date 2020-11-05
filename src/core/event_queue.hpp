#ifndef OCTOMQ_EVENT_QUEUE_H_
#define OCTOMQ_EVENT_QUEUE_H_

#include <condition_variable>
#include <cstdint>
#include <list>
#include <map>
#include <mutex>
#include <queue>
#include <string>
#include <vector>

#include "core/topic.hpp"
#include "network/network.hpp"

namespace octopus_mq {

using std::string, std::unique_ptr;

enum class event_type { incoming_packet, outgoing_packet, control_packet, stop };

using event_payload = std::vector<unsigned char>;

class event {
    event_type _event_type;
    protocol_type _protocol_type;
    topic _topic;
    event_payload _payload;

   public:
    event(const event_type &event_type, const protocol_type &protocol_type);

    void type(const event_type &type);
    void protocol(const protocol_type &type);
    void topic(const topic &topic);
    void topic(const string &name);
    void topic(const uint16_t &id);
    void topic_update(const string &name);
    void topic_update(string &&name);
    void topic_update(const uint16_t &id);
    void payload(const unsigned char *begin, const unsigned char *end);
    void payload(const event_payload &payload);
    void payload(event_payload &&payload);

    const event_type &type() const;
    const protocol_type &protocol() const;
    const class topic &topic() const;
    const event_payload &payload() const;
};

class event_queue_handle {
    unique_ptr<std::mutex> _mutex;
    unique_ptr<std::queue<event>> _queue;
    unique_ptr<std::condition_variable> _cv;
    bool _event;

   public:
    event_queue_handle();
    event_queue_handle(const event_queue_handle &) = delete;
    event_queue_handle(event_queue_handle &&eqh);

    event_queue_handle &operator=(const event_queue_handle &) = delete;
    event_queue_handle &operator=(event_queue_handle &&eqh);

    void event_avaliable(const bool &available);

    unique_ptr<std::mutex> &mutex();
    unique_ptr<std::queue<event>> &queue();
    unique_ptr<std::condition_variable> &cv();
    bool &event_avaliable();
};

using event_queue_map = std::map<std::pair<event_type, protocol_type>, event_queue_handle>;

class event_queue {
    event_queue_map _queue_map;

    void event_available(const event &event);
    void notify(const event &event);
    unique_ptr<std::mutex> &mutex(const event_type &type, const protocol_type &protocol);
    unique_ptr<std::mutex> &mutex(const event &event);
    void enqueue(const event &event);

   public:
    event_queue(const std::list<protocol_type> &protocol_list = { protocol_type::mqtt });

    void pop(const event_type &type, const protocol_type &protocol, event &event);
    void push(const event &event);
    void push_stop();
};

}  // namespace octopus_mq

#endif
