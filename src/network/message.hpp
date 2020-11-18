#ifndef OCTOMQ_MESSAGE_H_
#define OCTOMQ_MESSAGE_H_

#include <algorithm>
#include <list>
#include <memory>
#include <string>
#include <vector>

#define MQTT_STD_OPTIONAL
#define MQTT_STD_VARIANT
#define MQTT_STD_STRING_VIEW
#define MQTT_STD_ANY
#define MQTT_NS mqtt_cpp

#include "network/network.hpp"
#include "mqtt/property_variant.hpp"

namespace octopus_mq {

using std::string;

using message_payload = std::vector<char>;

using message_payload_ptr = std::shared_ptr<message_payload>;

class message {
    message_payload
        _payload;  // Only the actual message without flags and properties of any protocol
    string _topic;
    mqtt::version _mqtt_version;
    address _origin_address;
    string _origin_client_id;
    uint8_t _origin_pubopts;
    mqtt_cpp::v5::properties _origin_props;

   public:
    explicit message(message_payload &&payload);
    message(const message_payload &payload) = delete;
    message(message_payload &&payload, const string &origin_client_id);
    message(message_payload &&payload, const string &topic, const uint8_t pubopts,
            const address &origin_addr, const string &origin_clid,
            const mqtt::version &version = mqtt::version::v3,
            const mqtt_cpp::v5::properties &props = mqtt_cpp::v5::properties());
    message(message_payload &&payload, const uint8_t pubopts);

    void payload(const message_payload &payload);
    void payload(message_payload &&payload);
    void topic(const string &topic);
    void origin_addr(const address &origin_address);
    void origin_clid(const string &origin_client_id);
    void pubopts(const uint8_t pubopts);
    void props(const mqtt_cpp::v5::properties &props);
    void mqtt_version(const mqtt::version version);

    const message_payload &payload() const;
    const string &topic() const;
    const address &origin_addr() const;
    const string &origin_clid() const;
    const uint8_t &pubopts() const;
    const mqtt_cpp::v5::properties &props() const;
    const mqtt::version &mqtt_version() const;
};

using message_ptr = std::shared_ptr<message>;

class scope {
    using topic_tokens = std::vector<string>;
    std::list<topic_tokens> _scope;
    bool _is_absolute_wildcard;

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

    bool add(const string &topic_filter);
    void remove(const string &topic_filter);

    bool empty() const;
    bool includes(const string &topic) const;
    bool contains(const string &topic_filter) const;

    static bool valid_topic_filter(const std::string_view &topic_filter);
    static bool matches_filter(const std::string_view &filter, const std::string_view &topic);
};

}  // namespace octopus_mq

#endif
