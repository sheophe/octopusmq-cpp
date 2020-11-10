#ifndef OCTOMQ_DDS_ADAPTER_H_
#define OCTOMQ_DDS_ADAPTER_H_

#include <list>
#include <map>
#include <string>

#include "network/adapter.hpp"
#include "network/network.hpp"

namespace octopus_mq::dds {

using std::string;

class adapter_settings : public octopus_mq::adapter_settings {
    transport_type _transport;
    std::list<string> _scope;

    static inline const std::map<string, transport_type> _transport_from_name = {
        { OCTOMQ_ADAPTER_TRANSPORT_UDP, transport_type::udp },
        { OCTOMQ_ADAPTER_TRANSPORT_TCP, transport_type::tcp }
    };

   public:
    adapter_settings();
    adapter_settings(const nlohmann::json &json);

    void transport(const transport_type &transport);
    void transport(const string &transport);
    void scope(const string &scope);
    void scope(const std::list<string> &scope);

    const transport_type &transport() const;
    const std::list<string> &scope() const;
};

using adapter_settings_ptr = std::shared_ptr<dds::adapter_settings>;

}  // namespace octopus_mq::dds

#endif
