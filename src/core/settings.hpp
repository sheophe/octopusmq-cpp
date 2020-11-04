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

using std::string;

using adapter_settings_list = std::vector<shared_ptr<adapter_settings>>;

class settings {
    static inline nlohmann::json _settings_json;
    static inline adapter_settings_list _adapter_settings_list;

    static void parse_setting(const nlohmann::json &json);
    static void check_bindings();
    static void check_transport();
    static void parse();

   public:
    static void load(const string &file_name);
    static const adapter_settings_list &read_adapter_settings();
};

}  // namespace octopus_mq

#endif
