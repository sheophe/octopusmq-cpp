#include "core/topic.hpp"

namespace octopus_mq {

topic::topic() : _name(), _id(OCTOMQ_NULL_TOPICID), _type(topic_type::blank) {}

topic::topic(const string &name) : _name(name), _id(OCTOMQ_NULL_TOPICID), _type(topic_type::name) {}

topic::topic(const topic_id &id) : _name(), _id(id), _type(topic_type::id) {}

topic::topic(const string &name, const topic_id &id)
    : _name(name), _id(id), _type(topic_type::both) {}

void topic::add_type(const topic_type &new_type) {
    if ((new_type == topic_type::name and _type == topic_type::id) or
        (new_type == topic_type::id and _type == topic_type::name))
        _type = topic_type::both;
    else if (new_type != topic_type::blank)
        _type = new_type;
}

void topic::remove_type(const topic_type &type) {
    if (type == _type or type == topic_type::both)
        _type = topic_type::blank;
    else if (_type == topic_type::both) {
        if (type == topic_type::name)
            _type = topic_type::id;
        else if (type == topic_type::id)
            _type = topic_type::name;
    }
}

size_t topic::weight() const {
    size_t weight = 0;
    if (_name.empty())
        weight = static_cast<size_t>(_id);
    else
        for (auto iter = _name.begin(); iter < _name.end(); ++iter)
            weight += static_cast<size_t>(*iter.base());
    return weight;
}

bool topic::operator==(const class topic &b) const {
    if (_name.empty())
        return _id == b.id();
    else
        return _name.compare(b.name()) == 0;
}

bool topic::operator<(const class topic &b) const { return weight() < b.weight(); }

void topic::reset() {
    _name.clear();
    _id = 0;
    _type = topic_type::blank;
}

void topic::name(const string &name) {
    _name = name;
    if (name.empty())
        remove_type(topic_type::name);
    else
        add_type(topic_type::name);
}

void topic::name(string &&name) {
    _name = std::move(name);
    if (name.empty())
        remove_type(topic_type::name);
    else
        add_type(topic_type::name);
}

void topic::id(const uint16_t &id) {
    _id = id;
    if (id == OCTOMQ_NULL_TOPICID)
        remove_type(topic_type::id);
    else
        add_type(topic_type::id);
}

void topic::qos(const uint8_t &qos) {
    if (qos > 2)
        _qos = 2;
    else
        _qos = qos;
}

const string &topic::name() const { return _name; }

const uint16_t &topic::id() const { return _id; }

const uint8_t &topic::qos() const { return _qos; }

bool topic::empty() const { return _name == "" and _id == OCTOMQ_NULL_TOPICID; }

topic_list::topic_list() {}

void topic_list::add(const topic &topic) {
    topic_map::iterator iter = _map.find(topic);
    if (iter != _map.end())
        ++(iter->second);
    else
        _map.emplace(topic, 0);
}

void topic_list::remove(const topic &topic) {
    topic_map::iterator iter = _map.find(topic);
    if (iter != _map.end()) {
        if (iter->second == 1)
            _map.erase(iter);
        else
            --(iter->second);
    }
}

topic_map::iterator topic_list::find(const topic &topic) { return _map.find(topic); }

topic_map::iterator topic_list::begin() { return _map.begin(); }

topic_map::const_iterator topic_list::begin() const { return _map.begin(); }

topic_map::iterator topic_list::end() { return _map.end(); }

topic_map::const_iterator topic_list::end() const { return _map.end(); }

}  // namespace octopus_mq
