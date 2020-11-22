#ifndef OCTOMQ_SETTINGS_H_
#define OCTOMQ_SETTINGS_H_

#include <map>
#include <stdexcept>
#include <string>
#include <vector>

#include "json.hpp"
#include "network/adapter.hpp"
#include "network/network.hpp"
#include "threads/control.hpp"

namespace octopus_mq {

class settings {
    static inline nlohmann::json _settings_json;

    static void parse_setting(const nlohmann::json &json);
    static void check_bindings(adapter_pool &adapter_pool);
    static void check_transport();
    static void parse(adapter_pool &adapter_pool);

   public:
    static void load(const std::string &file_name, adapter_pool &adapter_pool);

    static const nlohmann::json json();
};

}  // namespace octopus_mq

#endif
