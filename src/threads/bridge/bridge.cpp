#include "threads/bridge/bridge.hpp"
#include "core/log.hpp"

#include <boost/lexical_cast.hpp>

namespace octopus_mq::bridge {

using namespace boost::asio;

implementation::implementation(const octopus_mq::adapter_settings_ptr adapter_settings,
                               message_queue& global_queue)
    : adapter_interface(adapter_settings, global_queue),
      _settings(std::static_pointer_cast<bridge::adapter_settings>(adapter_settings)),
      _verbose(_settings->verbose()) {
    // Initialize bridge server
    _server = std::make_unique<server>(
        ip::udp::endpoint(ip::make_address(_adapter_settings->phy().ip_string()),
                          boost::lexical_cast<std::uint16_t>(_adapter_settings->port())),
        _settings->send_port(), _ioc, _settings->transport_mode(), _settings->discovery().format,
        _settings->discovery().endpoints, _adapter_settings->name(), _verbose);

    // Setup delays and timeouts
    _server->set_probe_delay(_settings->timeouts().delay);

    // Setup error handlers
    _server->set_system_error_handler([this](boost::system::error_code ec) {
        // 'Operation cancelled' occurs when control thread stops the broker
        // in midst of some process
        if (ec != boost::system::errc::operation_canceled)
            log::print(log_type::error, "system error: " + utility::lowercase_string(ec.message()),
                       _adapter_settings->name());
    });

    _server->set_protocol_error_handler([this](protocol::basic_error err) {
        log::print(log_type::error, err.what(), _adapter_settings->name());
    });

    // Set message handlers
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