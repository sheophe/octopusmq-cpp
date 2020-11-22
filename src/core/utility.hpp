#ifndef OCTOMQ_UTILITY_H_
#define OCTOMQ_UTILITY_H_

#include <boost/asio.hpp>

namespace octopus_mq::utility {

std::string size_string(const std::size_t &size);
std::string lowercase_string(const std::string &input_string);
std::string boost_error_message(const boost::system::error_code &error);

}  // namespace octopus_mq::utility

#endif
