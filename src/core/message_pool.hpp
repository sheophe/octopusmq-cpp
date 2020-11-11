#ifndef OCTOMQ_MESSAGE_QUEUE_H_
#define OCTOMQ_MESSAGE_QUEUE_H_

#include <condition_variable>
#include <list>
#include <map>
#include <mutex>
#include <queue>
#include <string>
#include <vector>

#include "core/topic.hpp"
#include "network/network.hpp"

namespace octopus_mq {

using std::string;

using message_payload = std::vector<unsigned char>;

class message {
    message_payload
        _payload;  // Only the actual message without flags and properties of any protocol
    string _topic;
    string _origin_client_id;
    mqtt::version _mqtt_version;
    uint8_t _origin_pubopts;

    int _packet_type;

   public:
    message();
    explicit message(const int &packet_type);
    explicit message(message_payload &&payload);
    message(const message_payload &payload) = delete;
    message(message_payload &&payload, const string &origin_client_id);
    message(message_payload &&payload, const uint8_t pubopts);

    void payload(const message_payload &payload);
    void payload(message_payload &&payload);
    void topic(const string &topic);
    void origin(const string &origin_client_id);
    void pubopts(const uint8_t pubopts);

    const message_payload &payload() const;
    const string &topic() const;
    const string &origin() const;
    uint8_t pubopts() const;
};

class message_notify_handle {
    std::unique_ptr<std::condition_variable> _cv;
    bool _event;

   public:
    message_notify_handle();
    message_notify_handle(const message_notify_handle &) = delete;
    message_notify_handle(message_notify_handle &&mnh);

    message_notify_handle &operator=(const message_notify_handle &) = delete;
    message_notify_handle &operator=(message_notify_handle &&mnh);

    void event_avaliable(const bool &available);

    std::unique_ptr<std::condition_variable> &cv();
    bool &event_avaliable();
};

using message_notify_map = std::map<string, std::list<message_notify_handle>>;

class message_pool {
    std::mutex _mutex;
    std::queue<message> _queue;
    message_notify_map _notify_map;
    std::mutex _notify_mutex;

   public:
    void push(const message &payload);
    bool pop(message &payload);
    message_notify_handle &add_notify_handle(const string &topic);

    std::list<message_notify_handle> &get_notify_handles(const string &topic);
};

}  // namespace octopus_mq

#endif
