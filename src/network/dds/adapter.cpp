#include "network/dds/adapter.hpp"

#include "core/error.hpp"
#include "core/log.hpp"

namespace octopus_mq::dds {

adapter_settings::adapter_settings(const nlohmann::json &json)
    : octopus_mq::adapter_settings(protocol_type::dds, json) {
    if (not json.contains(adapter::field_name::transport))
        throw missing_field_error(adapter::field_name::transport);

    const nlohmann::json &transport_field = json[adapter::field_name::transport];
    if (not transport_field.is_string()) throw field_type_error(adapter::field_name::transport);

    std::string transport_str = transport_field.get<std::string>();
    transport(transport_str);
    name_append(mqtt::adapter_role_name::client + '(' + transport_str + ')');
}

void adapter_settings::transport(const transport_type &transport) { _transport = transport; }

void adapter_settings::transport(const std::string &transport) {
    if (auto iter = _transport_from_name.find(transport); iter != _transport_from_name.end())
        _transport = iter->second;
    else
        throw std::runtime_error("unsupported transport for dds adapter: " + transport);
}

const transport_type &adapter_settings::transport() const { return _transport; }

}  // namespace octopus_mq::dds