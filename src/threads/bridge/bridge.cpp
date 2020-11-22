#include "threads/bridge/bridge.hpp"
#include "core/log.hpp"
#include "core/utility.hpp"

namespace octopus_mq::bridge {

using namespace boost;

implementation::implementation(const octopus_mq::adapter_settings_ptr adapter_settings,
                               message_queue& global_queue)
    : adapter_interface(adapter_settings, global_queue),
      _settings(std::static_pointer_cast<bridge::adapter_settings>(adapter_settings)) {
    // Initialize bridge server
    _server = std::make_unique<server>(
        _ioc, asio::ip::address_v4(_adapter_settings->phy().netmask()),
        asio::ip::udp::endpoint(asio::ip::address_v4(_adapter_settings->phy().ip()),
                                _adapter_settings->port()),
        _settings, _adapter_settings->name());

    // Iniitialize error handlers
    _server->set_network_error_handler([this](const boost::system::error_code& ec) {
        // 'Operation cancelled' occurs when control thread stops the broker
        // in midst of some process
        if (ec != boost::system::errc::operation_canceled)
            log::print(log_type::error, "network error: " + utility::lowercase_string(ec.message()),
                       _adapter_settings->name());
    });

    _server->set_protocol_error_handler([this](const protocol::basic_error& err) {
        log::print(log_type::error, err.what(), _adapter_settings->name());
    });

    // Initialize subscribe/unsubscribe handlers
}

void implementation::worker() {
    _server->run();
    _ioc.run();
}

void implementation::run() { _thread = std::thread(&implementation::worker, this); }

void implementation::stop() {
    if (_thread.joinable()) {
        _ioc.stop();
        _server->stop();
        _thread.join();
    }
}

void implementation::inject_publish(const message_ptr message) { (*message); }

}  // namespace octopus_mq::bridge