#ifndef OCTOMQ_ERRORS_H_
#define OCTOMQ_ERRORS_H_

#include <string>

namespace octopus_mq {

class field_type_error : public std::runtime_error {
   public:
    explicit field_type_error(const string &what_arg)
        : std::runtime_error("field '" + what_arg + "' has a wrong type.") {}
};

class field_range_error : public std::range_error {
   public:
    explicit field_range_error(const string &what_arg)
        : std::range_error("value of '" + what_arg + "' is out of range.") {}
};

class field_encapsulated_error : public std::runtime_error {
   public:
    explicit field_encapsulated_error(const string field_name, const string &what_arg)
        : std::runtime_error("error in field '" + field_name + "': " + what_arg + '.') {}
};

class missing_field_error : public std::runtime_error {
   public:
    explicit missing_field_error(const string &field_name)
        : std::runtime_error("filed '" + field_name + "' is missing from settings.") {}
};

class unknown_protocol_error : public std::runtime_error {
   public:
    explicit unknown_protocol_error(const string &protocol_name)
        : std::runtime_error("unknown protocol: '" + protocol_name + "'.") {}
};

class adapter_binding_error : public std::runtime_error {
   public:
    explicit adapter_binding_error(const string &binding_name, const string &first_adapter_name,
                                   const string &second_adapter_name)
        : std::runtime_error("binding collision in '" + second_adapter_name + "' and '" +
                             first_adapter_name + "': [" + binding_name + "].") {}
};

class adapter_transport_error : public std::runtime_error {
   public:
    explicit adapter_transport_error(const string &adapter_name)
        : std::runtime_error("adapter '" + adapter_name +
                             "': selected protocol does not support specified transport.") {}
};

class invalid_invocation : public std::runtime_error {
   public:
    explicit invalid_invocation(const string &fname)
        : std::runtime_error("invalid invocation of function '" + fname + "'.") {}
};

}  // namespace octopus_mq

#endif
