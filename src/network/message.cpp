#include "network/message.hpp"

namespace octopus_mq {

message::message(const int &packet_type) : _packet_type(packet_type) {}

message::message(message_payload &&payload) : _payload(move(payload)) {}

message::message(message_payload &&payload, const string &origin_client_id)
    : _payload(move(payload)), _origin_client_id(origin_client_id), _origin_pubopts(0) {}

message::message(message_payload &&payload, const string &topic, const uint8_t pubopts)
    : _payload(move(payload)), _topic(topic), _origin_pubopts(pubopts) {}

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

}  // namespace octopus_mq
