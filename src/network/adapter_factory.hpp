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

class adapter_factory {
   public:
    static shared_ptr<adapter_settings> from_json(const nlohmann::json &json);
    // static const nlohmann::json create_json(const shared_ptr<adapter_settings> &adapter_settings,
    //                                         const protocol_type &protocol);

    static const protocol_type &protocol_from_name(const string &name);
};

}  // namespace octopus_mq

#endif
