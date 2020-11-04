#include "network/adapter_factory.hpp"

#include "core/error.hpp"

namespace octopus_mq {

static inline const std::map<string, protocol_type> _protocol_from_name = {
    { "mqtt", protocol_type::mqtt }, { "dds", protocol_type::dds }
};

const shared_ptr<adapter_settings> adapter_factory::from_json(const nlohmann::json &json) {
    if (not json.contains(OCTOMQ_ADAPTER_FIELD_PROTOCOL))
        throw missing_field_error(OCTOMQ_ADAPTER_FIELD_PROTOCOL);

    const nlohmann::json &item = json[OCTOMQ_ADAPTER_FIELD_PROTOCOL];
    if (item.is_string()) {
        string protocol_name = item.get<string>();
        if (auto iter = _protocol_from_name.find(protocol_name);
            iter != _protocol_from_name.end()) {
            const protocol_type protocol = iter->second;
            // Calling protocol-specific constructor
            switch (protocol) {
                case protocol_type::mqtt:
                    return std::make_shared<mqtt_adapter_settings>(json);
                case protocol_type::dds:
                    return std::make_shared<dds_adapter_settings>(json);
                default:
                    throw unknown_protocol_error(protocol_name);
            }
        } else
            throw unknown_protocol_error(protocol_name);
    } else
        throw field_type_error(OCTOMQ_ADAPTER_FIELD_PROTOCOL);
}

}  // namespace octopus_mq