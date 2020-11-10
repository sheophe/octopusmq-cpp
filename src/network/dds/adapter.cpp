#include "network/dds/adapter.hpp"

#include "core/error.hpp"
#include "core/log.hpp"

namespace octopus_mq::dds {

static octopus_mq::adapter_settings_parser adapter_settings_parser = {
    { OCTOMQ_ADAPTER_FIELD_TRANSPORT,
      [](octopus_mq::adapter_settings *self, const adapter_settings_parser_item &item) {
          if (item->is_string())
              static_cast<dds::adapter_settings *>(self)->transport(item->get<string>());
          else
              throw field_type_error(OCTOMQ_ADAPTER_FIELD_TRANSPORT);
      } },
    { OCTOMQ_ADAPTER_FIELD_SCOPE,
      [](octopus_mq::adapter_settings *self, const adapter_settings_parser_item &item) {
          if (item->is_string())
              static_cast<dds::adapter_settings *>(self)->scope(item->get<string>());
          else if (item->is_array())
              static_cast<dds::adapter_settings *>(self)->scope(item->get<std::list<string>>());
          else
              throw field_type_error(OCTOMQ_ADAPTER_FIELD_TRANSPORT);
      } }
};

adapter_settings::adapter_settings()
    : octopus_mq::adapter_settings(protocol_type::dds),
      _transport(transport_type::udp),
      _scope({ "#" }) {}

adapter_settings::adapter_settings(const nlohmann::json &json)
    : octopus_mq::adapter_settings(protocol_type::dds, json) {
    // Parse protocol-specific fields from JSON
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
        throw std::runtime_error("unsupported transport for dds adapter: " + transport);
}

void adapter_settings::scope(const string &scope) {
    // Check the actual string
    _scope.push_back(scope);
}

void adapter_settings::scope(const std::list<string> &scope) {
    // Check the actual strings
    _scope = scope;
}

const transport_type &adapter_settings::transport() const { return _transport; }

const std::list<string> &adapter_settings::scope() const { return _scope; }

}  // namespace octopus_mq::dds