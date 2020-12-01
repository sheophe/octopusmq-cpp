#ifndef OCTOMQ_MESSAGE_H_
#define OCTOMQ_MESSAGE_H_

#include <algorithm>
#include <memory>
#include <set>
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

namespace mqtt {

    enum class version : std::uint8_t { v3 = 0x03, v5 = 0x05 };

    enum class adapter_role { broker, client };

    namespace adapter_role_name {

        constexpr char broker[] = "broker";
        constexpr char client[] = "client";

    }  // namespace adapter_role_name

}  // namespace mqtt

using message_payload = std::vector<char>;
using message_payload_ptr = std::shared_ptr<message_payload>;

class message {
    message_payload
        _payload;  // Only the actual message without flags and properties of any protocol
    std::string _topic;
    mqtt::version _mqtt_version;
    address _origin_address;
    std::string _origin_client_id;
    std::uint8_t _origin_pubopts;
    mqtt_cpp::v5::properties _origin_props;

   public:
    explicit message(message_payload &&payload);
    message(const message_payload &payload) = delete;
    message(message_payload &&payload, const std::string &origin_client_id);
    message(message_payload &&payload, const std::string &topic, const std::uint8_t pubopts,
            const address &origin_addr, const std::string &origin_clid,
            const mqtt::version &version = mqtt::version::v3,
            const mqtt_cpp::v5::properties &props = mqtt_cpp::v5::properties());
    message(message_payload &&payload, const std::uint8_t pubopts);

    void payload(const message_payload &payload);
    void payload(message_payload &&payload);
    void topic(const std::string &topic);
    void origin_addr(const address &origin_address);
    void origin_clid(const std::string &origin_client_id);
    void pubopts(const std::uint8_t pubopts);
    void props(const mqtt_cpp::v5::properties &props);
    void mqtt_version(const mqtt::version version);

    const message_payload &payload() const;
    const std::string &topic() const;
    const address &origin_addr() const;
    const std::string &origin_clid() const;
    const std::uint8_t &pubopts() const;
    const mqtt_cpp::v5::properties &props() const;
    const mqtt::version &mqtt_version() const;
};

using message_ptr = std::shared_ptr<message>;

class scope {
    using topic_tokens = std::vector<std::string>;
    std::set<topic_tokens> _scope;
    std::set<std::string> _scope_strings;
    bool _is_absolute_wildcard;

    static constexpr char hash_sign[] = "#";
    static constexpr char plus_sign[] = "+";
    static constexpr char slash_sign[] = "/";
    static constexpr char dollar_sign = '$';

    static topic_tokens tokenize_topic_filter(const std::string &topic_filter);
    static topic_tokens tokenize_topic(const std::string &topic);
    static bool compare_topics(const topic_tokens &filter, const topic_tokens &topic);

   public:
    scope();
    scope(const std::string &scope_string);
    scope(const std::vector<std::string> &scope_vector);

    bool add(const std::string &topic_filter);
    void remove(const std::string &topic_filter);
    void clear();
    void clear_internal();

    bool empty() const;
    std::size_t size() const;
    bool includes(const std::string &topic) const;
    bool contains(const std::string &topic_filter) const;
    const std::set<std::string> &scope_strings() const;

    static bool valid_topic(const std::string_view &topic);
    static bool valid_topic_filter(const std::string_view &topic_filter);
    static bool matches_filter(const std::string_view &filter, const std::string_view &topic);
    static bool is_internal(const std::string_view &topic);
};

}  // namespace octopus_mq

#endif
