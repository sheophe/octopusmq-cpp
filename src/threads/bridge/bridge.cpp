#include "threads/bridge/bridge.hpp"

#include <boost/asio.hpp>
#include <boost/lexical_cast.hpp>

namespace octopus_mq::bridge {

using namespace boost::asio;

implementation::implementation(const octopus_mq::adapter_settings_ptr adapter_settings,
                               message_queue& global_queue)
    : adapter_interface(adapter_settings, global_queue),
      //_connections_available(false),
      _settings(std::static_pointer_cast<bridge::adapter_settings>(adapter_settings)) {}

void implementation::list_discoverer(
    [[maybe_unused]] const std::chrono::milliseconds& discovery_timeout,
    [[maybe_unused]] const std::chrono::milliseconds& rescan_timeout, const port_int port,
    const discovery_list& list) {
    while (not _discoverer_should_stop) {
        for (auto& ip : list) {
            address endpoint_address(ip, port);
        }
        if (_discoverer_should_stop) break;
    }
}

void implementation::range_discoverer(
    [[maybe_unused]] const std::chrono::milliseconds& discovery_timeout,
    [[maybe_unused]] const std::chrono::milliseconds& rescan_timeout, const port_int port,
    const discovery_range& range) {
    while (not _discoverer_should_stop) {
        if (_discoverer_should_stop) break;
        if (range.from == range.to) break;
        address endpoint_address(network::constants::null_ip, port);
    }
}

void implementation::run() {
    _discoverer_should_stop = false;
    if (_settings->discovery().format == discovery_endpoints_format::list)
        _discoverer_thread =
            std::thread(&implementation::list_discoverer, this, _settings->timeouts().discovery,
                        _settings->timeouts().rescan, _settings->port(),
                        std::get<discovery_list>(_settings->discovery().endpoints));
    else
        _discoverer_thread =
            std::thread(&implementation::range_discoverer, this, _settings->timeouts().discovery,
                        _settings->timeouts().rescan, _settings->port(),
                        std::get<discovery_range>(_settings->discovery().endpoints));
}

void implementation::stop() {
    _discoverer_should_stop = true;
    if (_discoverer_thread.joinable()) _discoverer_thread.join();
}

void implementation::inject_publish(const message_ptr message) { (*message); }

}  // namespace octopus_mq::bridge