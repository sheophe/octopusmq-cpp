#include "network/mqtt/adapter.hpp"

#include "core/error.hpp"
#include "core/log.hpp"

namespace octopus_mq::mqtt {

static octopus_mq::adapter_settings_parser adapter_settings_parser = {
    { adapter::field_name::transport,
      [](octopus_mq::adapter_settings *self, const adapter_settings_parser_item &item) {
          if (item->is_string()) {
              std::string transport_str = item->get<string>();
              static_cast<adapter_settings *>(self)->transport(transport_str);
              static_cast<adapter_settings *>(self)->name_append('(' + transport_str + ')');
          } else
              throw field_type_error(adapter::field_name::transport);
      } },
#ifdef OCTOMQ_ENABLE_MQTT_CLIENT
    { adapter::field_name::role,
      [](octopus_mq::adapter_settings *self, const adapter_settings_parser_item &item) {
          if (item->is_string()) {
              std::string role_str = item->get<string>();
              static_cast<adapter_settings *>(self)->role(role_str);
              static_cast<adapter_settings *>(self)->name_append(role_str);
          } else
              throw field_type_error(adapter::field_name::role);
      } }
#endif
};

adapter_settings::adapter_settings(const nlohmann::json &json)
    : octopus_mq::adapter_settings(protocol_type::mqtt, json) {
    // Parse protocol-specific fields from JSON
#ifndef OCTOMQ_ENABLE_MQTT_CLIENT
    name_append(mqtt::adapter_role_name::broker);
#endif
    for (auto item_parser : adapter_settings_parser)
        if (auto json_item = json.find(item_parser.first); json_item != json.end())
            item_parser.second(this, json_item);
        else
            throw missing_field_error(item_parser.first);
}

void adapter_settings::transport(const transport_type &transport) { _transport = transport; }

void adapter_settings::transport(const string &transport) {
    if (auto iter = _transport_from_name.find(transport); iter != _transport_from_name.end())
        _transport = iter->second;
    else
        throw std::runtime_error("unsupported transport for mqtt adapter: " + transport);
}

const transport_type &adapter_settings::transport() const { return _transport; }

#ifdef OCTOMQ_ENABLE_MQTT_CLIENT
void adapter_settings::role(const adapter_role &role) { _role = role; }

void adapter_settings::role(const string &role) {
    if (auto iter = _role_from_name.find(role); iter != _role_from_name.end())
        _role = iter->second;
    else
        throw std::runtime_error("unknown mqtt adapter role: " + role);
}

const adapter_role &adapter_settings::role() const { return _role; }
#endif

}  // namespace octopus_mq::mqtt
