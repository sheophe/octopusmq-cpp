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

void settings::check_bindings(adapter_pool &adapter_pool) {
    // Compare binding of the last element to bindings of all previous elements
    const adapter_pool::iterator back = adapter_pool.end() - 1;

    for (auto iter = adapter_pool.begin(); iter < back; ++iter)
        if (back->first->compare_binding(iter->first->phy().ip(), iter->first->port()))
            throw adapter_binding_error(back->first->binging_name(), back->first->name(),
                                        iter->first->name());
}

void settings::parse(adapter_pool &adapter_pool) {
    if ((not _settings_json.contains("adapters")) or (not _settings_json["adapters"].is_array()))
        throw std::runtime_error("configuration file does not contain 'adapters' list.");
    if (_settings_json["adapters"].empty())
        throw std::runtime_error("configuration file contains an empty 'adapters' list.");
    for (auto &adapter_json : _settings_json["adapters"]) {
        adapter_pool.push_back({ adapter_settings_factory::from_json(adapter_json), nullptr });
        if (adapter_pool.size() > 1) check_bindings(adapter_pool);
    }
}

void settings::load(const string &file_name, adapter_pool &adapter_pool) {
    if (file_name.empty()) throw std::runtime_error("empty configuration file name.");
    if (std::ifstream ifs = std::ifstream(file_name, std::fstream::in); not ifs.is_open())
        throw std::runtime_error("cannot open configuration file: " + string(strerror(errno)) +
                                 ": " + file_name);
    else {
        try {
            _settings_json = nlohmann::json::parse(ifs);
            settings::parse(adapter_pool);
        } catch (const std::runtime_error &re) {
            throw std::runtime_error("settings error: " + string(re.what()));
        }
    }
}

const nlohmann::json settings::json() { return _settings_json; }

}  // namespace octopus_mq
