#include "core/utility.hpp"

#include <sstream>
#include <iomanip>

namespace octopus_mq::utility {

std::string size_string(const std::size_t &size) {
    std::ostringstream str_stream;
    if (size < 0x400)
        str_stream << size << " B";
    else if (size < 0x100000)
        str_stream << std::fixed << std::setprecision(1) << (static_cast<double>(size) / 0x400)
                   << " KB";
    else if (size < 0x40000000)
        str_stream << std::fixed << std::setprecision(1) << (static_cast<double>(size) / 0x100000)
                   << " MB";
    else
        str_stream << std::fixed << std::setprecision(1) << (static_cast<double>(size) / 0x40000000)
                   << " GB";
    return str_stream.str();
}

std::string lowercase_string(const std::string &input_string) {
    std::string lowercase = input_string;
    std::transform(lowercase.begin(), lowercase.end(), lowercase.begin(),
                   [](unsigned char c) { return std::tolower(c); });
    return lowercase;
}

// Converting only errors for which the default message is too unobvious.
std::string boost_error_message(const boost::system::error_code &error) {
    switch (error.value()) {
        case boost::asio::error::eof:
            return "connection lost";
        default:
            return utility::lowercase_string(error.message());
    }
}

}  // namespace octopus_mq::utility