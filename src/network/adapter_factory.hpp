#ifndef OCTOMQ_ADAPTER_FACTORY_H_
#define OCTOMQ_ADAPTER_FACTORY_H_

#include <list>
#include <map>
#include <memory>
#include <string>

#include "json.hpp"
#include "network/adapter.hpp"
#include "network/network.hpp"

#include "network/mqtt/adapter.hpp"
#include "network/bridge/adapter.hpp"
#ifdef OCTOMQ_ENABLE_DDS
#include "network/dds/adapter.hpp"
#endif

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
