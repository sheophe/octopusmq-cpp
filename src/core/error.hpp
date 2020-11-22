#ifndef OCTOMQ_ERRORS_H_
#define OCTOMQ_ERRORS_H_

#include <string>
#include <stdexcept>

namespace octopus_mq {

class adapter_not_initialized : public std::runtime_error {
   public:
    explicit adapter_not_initialized() : std::runtime_error("adapter is not initialized.") {}

    explicit adapter_not_initialized(const std::string &name)
        : std::runtime_error("adapter '" + name + "' is not initialized.") {}
};

class field_type_error : public std::runtime_error {
   public:
    explicit field_type_error(const std::string &what_arg)
        : std::runtime_error("field '" + what_arg + "' has a wrong type.") {}

    explicit field_type_error(const std::string &field, const std::string &subfield)
        : std::runtime_error("field '" + field + '.' + subfield + "' has a wrong type.") {}
};

class field_range_error : public std::range_error {
   public:
    explicit field_range_error(const std::string &what_arg)
        : std::range_error("value of '" + what_arg + "' is out of range.") {}
};

class field_encapsulated_error : public std::runtime_error {
   public:
    explicit field_encapsulated_error(const std::string field_name, const std::string &what_arg)
        : std::runtime_error("error in field '" + field_name + "': " + what_arg + '.') {}
};

class missing_field_error : public std::runtime_error {
   public:
    explicit missing_field_error(const std::string &field_name)
        : std::runtime_error("field '" + field_name + "' is missing from settings.") {}

    explicit missing_field_error(const std::string &field_name, const std::string &subfield_name)
        : std::runtime_error("field '" + field_name + '.' + subfield_name +
                             "' is missing from settings.") {}
};

class unknown_protocol_error : public std::runtime_error {
   public:
    explicit unknown_protocol_error(const std::string &protocol_name)
        : std::runtime_error("unknown protocol: '" + protocol_name + "'.") {}
};

class adapter_binding_error : public std::runtime_error {
   public:
    explicit adapter_binding_error(const std::string &binding_name,
                                   const std::string &first_adapter_name,
                                   const std::string &second_adapter_name)
        : std::runtime_error("binding collision in '" + second_adapter_name + "' and '" +
                             first_adapter_name + "': " + binding_name + '.') {}
};

class adapter_transport_error : public std::runtime_error {
   public:
    explicit adapter_transport_error(const std::string &adapter_name)
        : std::runtime_error("adapter '" + adapter_name +
                             "': selected protocol does not support specified transport.") {}

    explicit adapter_transport_error(const std::string &adapter_name,
                                     const std::string &protocol_name)
        : std::runtime_error("adapter '" + adapter_name + "': " + protocol_name +
                             " protocol does not support specified transport.") {}

    explicit adapter_transport_error(const std::string &adapter_name,
                                     const std::string &protocol_name,
                                     const std::string &transport_name)
        : std::runtime_error("adapter '" + adapter_name + "': " + protocol_name +
                             " protocol does not support " + transport_name + " transport.") {}
};

class invalid_invocation : public std::runtime_error {
   public:
    explicit invalid_invocation(const std::string &fname)
        : std::runtime_error("invalid invocation of function '" + fname + "'.") {}
};

class invalid_topic_filter : public std::runtime_error {
   public:
    explicit invalid_topic_filter(const std::string &topic)
        : std::runtime_error("invalid topic filter '" + topic + "'.") {}
};

class invalid_bridge_interface : public std::runtime_error {
   public:
    explicit invalid_bridge_interface()
        : std::runtime_error("bridge adapter: cannot use wildcard interface name.") {}
};

class invalid_bridge_discovery_mode : public std::runtime_error {
   public:
    explicit invalid_bridge_discovery_mode(const std::string &mode)
        : std::runtime_error("bridge adapter: transport mode '" + mode + "' is not supported.") {}
};

class invalid_bridge_endpoint : public std::runtime_error {
   public:
    explicit invalid_bridge_endpoint(const std::string &endpoint)
        : std::runtime_error("bridge adapter: invalid discovery endpoint '" + endpoint + "'.") {}
};

class invalid_bridge_range : public std::runtime_error {
   public:
    explicit invalid_bridge_range()
        : std::runtime_error("bridge adapter: invalid address range.") {}
};

class bridge_range_different_nets : public std::runtime_error {
   public:
    explicit bridge_range_different_nets()
        : std::runtime_error(
              "bridge adapter: cannot use endpoint address from different network.") {}
};

class bridge_discovery_not_set : public std::runtime_error {
   public:
    explicit bridge_discovery_not_set()
        : std::runtime_error("bridge adapter: discovery endpoints are not defined.") {}
};

class bridge_malformed_packet : public std::runtime_error {
   public:
    explicit bridge_malformed_packet(const std::string &packet_name)
        : std::runtime_error("bridge adapter: received malformed packet '" + packet_name + '\'') {}

    explicit bridge_malformed_packet()
        : std::runtime_error("bridge adapter: received malformed packet") {}
};

class bridge_malformed_packet_constructed : public std::runtime_error {
   public:
    explicit bridge_malformed_packet_constructed()
        : std::runtime_error("bridge adapter: tried to construct malformed packet") {}
};

class bridge_recursive_config : public std::runtime_error {
   public:
    explicit bridge_recursive_config()
        : std::runtime_error(
              "bridge adapter: settings imply recursive bridge, which is impossible.") {}
};

}  // namespace octopus_mq

#endif
