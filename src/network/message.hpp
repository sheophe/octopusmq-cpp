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

   public:
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
    void mqtt_version(const mqtt::version version);

    const message_payload &payload() const;
    const string &topic() const;
    const string &origin() const;
    const uint8_t &pubopts() const;
    const mqtt::version &mqtt_version() const;
};

using message_ptr = std::shared_ptr<message>;

class scope {
    using topic_tokens = std::vector<string>;
    std::vector<topic_tokens> _scope;
    bool _is_global_wildcard;

    static inline const char hash_sign[2] = { '#', 0 };
    static inline const char plus_sign[2] = { '+', 0 };
    static inline const char slash_sign[2] = { '/', 0 };

    static topic_tokens tokenize_topic_filter(const string &topic_filter);
    static topic_tokens tokenize_topic(const string &topic);
    static bool compare_topics(const topic_tokens &filter, const topic_tokens &topic);

   public:
    scope();
    scope(const string &scope_string);
    scope(const std::vector<string> &scope_vector);

    bool includes(const string &topic) const;

    static bool valid_topic_filter(const std::string_view &topic_filter);
    static bool matches_filter(const std::string_view &filter, const std::string_view &topic);
};

}  // namespace octopus_mq

#endif
