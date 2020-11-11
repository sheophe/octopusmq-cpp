#ifndef OCTOMQ_ADAPTER_FACTORY_H_
#define OCTOMQ_ADAPTER_FACTORY_H_

#include <list>
#include <map>
#include <string>

#include "json.hpp"
#include "network/adapter.hpp"
#include "network/adapter_headers.hpp"
#include "network/network.hpp"

namespace octopus_mq {

using std::string, std::shared_ptr;

class adapter_settings_factory {
   public:
    static adapter_settings_ptr from_json(const nlohmann::json &json);

    static const protocol_type &protocol_from_name(const string &name);
};

class adapter_interface_factory {
   public:
    static adapter_iface_ptr from_settings(adapter_settings_ptr settings,
                                           message_queue &message_queue);
};

}  // namespace octopus_mq

#endif
