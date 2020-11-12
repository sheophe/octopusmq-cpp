#ifndef OCTOMQ_MESSAGE_H_
#define OCTOMQ_MESSAGE_H_

#include <string>
#include <vector>

#include "network/network.hpp"

namespace octopus_mq {

using std::string;

using message_payload = std::vector<char>;

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
    message(message_payload &&payload, const string &topic, const uint8_t pubopts);
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

using message_ptr = std::shared_ptr<message>;

}  // namespace octopus_mq

#endif
