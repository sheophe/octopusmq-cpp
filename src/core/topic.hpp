#ifndef OCTOMQ_TOPIC_H_
#define OCTOMQ_TOPIC_H_

#include <condition_variable>
#include <cstdint>
#include <map>
#include <mutex>
#include <string>

#define OCTOMQ_NULL_TOPICID (0)

namespace octopus_mq {

using std::string;

enum class topic_type { blank, name, id, both };

using topic_id = uint16_t;

class topic {
    string _name;
    topic_id _id;
    topic_type _type;
    uint8_t _qos;
    void add_type(const topic_type &new_type);
    void remove_type(const topic_type &type);
    size_t weight() const;

   public:
    topic();
    explicit topic(const string &name);
    explicit topic(const topic_id &id);
    topic(const string &name, const topic_id &id);

    bool operator==(const class topic &b) const;
    bool operator<(const class topic &b) const;

    void reset();
    void name(const string &name);
    void name(string &&name);
    void id(const topic_id &id);
    bool match(const topic &topic);
    void qos(const uint8_t &qos);

    const string &name() const;
    const topic_id &id() const;
    const uint8_t &qos() const;
    bool empty() const;
};

using topic_map = std::map<topic, uint32_t>;

class topic_list {
    topic_map _map;

   public:
    topic_list();

    void add(const topic &topic);
    void remove(const topic &topic);

    topic_map::iterator find(const topic &topic);
    topic_map::iterator begin();
    topic_map::const_iterator begin() const;
    topic_map::iterator end();
    topic_map::const_iterator end() const;
};

}  // namespace octopus_mq

#endif
