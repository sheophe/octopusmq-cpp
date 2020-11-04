#include "core/settings.hpp"

#include <fcntl.h>
#include <signal.h>
#include <sys/stat.h>
#include <unistd.h>

#include <fstream>

#include "core/error.hpp"
#include "core/log.hpp"
#include "network/adapter_factory.hpp"

namespace octopus_mq {

void settings::check_bindings() {
    // Compare binding of the last element to bindings of all previous elements
    const adapter_settings_list::iterator last = _adapter_settings_list.end() - 1;

    for (auto iter = _adapter_settings_list.begin(); iter < last; ++iter)
        if (last->get()->compare_binding(iter->get()->phy().ip(), iter->get()->port()))
            throw adapter_binding_error(last->get()->binging_name(), last->get()->name(),
                                        iter->get()->name());
}

void settings::parse() {
    if ((not _settings_json.contains("adapters")) or (not _settings_json["adapters"].is_array()))
        throw std::runtime_error("configuration file does not contain 'adapters' list.");
    for (auto &adapter_json : _settings_json["adapters"]) {
        _adapter_settings_list.push_back(adapter_factory::from_json(adapter_json));
        if (_adapter_settings_list.size() > 1) check_bindings();
    }
}

void settings::load(const string &file_name) {
    if (file_name.empty()) throw std::runtime_error("empty configuration file name.");
    if (std::ifstream ifs = std::ifstream(file_name, std::fstream::in); not ifs.is_open())
        throw std::runtime_error("cannot open configuration file: " + string(strerror(errno)) +
                                 ": " + file_name);
    else {
        try {
            _settings_json = nlohmann::json::parse(ifs);
            settings::parse();
        } catch (const std::runtime_error &re) {
            throw std::runtime_error("settings parsing error: " + string(re.what()));
        }
    }
}

const adapter_settings_list &settings::read_adapter_settings() { return _adapter_settings_list; }

}  // namespace octopus_mq
