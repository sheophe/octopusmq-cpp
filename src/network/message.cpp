#include "network/message.hpp"

#include "core/error.hpp"

namespace octopus_mq {

message::message(message_payload &&payload) : _payload(move(payload)) {}

message::message(message_payload &&payload, const string &origin_client_id)
    : _payload(move(payload)), _origin_client_id(origin_client_id), _origin_pubopts(0) {}

message::message(message_payload &&payload, const string &topic, const uint8_t pubopts)
    : _payload(move(payload)), _topic(topic), _origin_pubopts(pubopts) {}

message::message(message_payload &&payload, const uint8_t pubopts)
    : _payload(move(payload)), _origin_pubopts(pubopts) {}

void message::payload(const message_payload &payload) { _payload = payload; }

void message::payload(message_payload &&payload) { _payload = move(payload); }

void message::topic(const string &topic) { _topic = topic; }

void message::origin(const string &origin_client_id) { _origin_client_id = origin_client_id; }

void message::pubopts(const uint8_t pubopts) { _origin_pubopts = pubopts; }

void message::mqtt_version(const mqtt::version version) { _mqtt_version = version; }

const message_payload &message::payload() const { return _payload; }

const string &message::topic() const { return _topic; }

const string &message::origin() const { return _origin_client_id; }

const uint8_t &message::pubopts() const { return _origin_pubopts; }

const mqtt::version &message::mqtt_version() const { return _mqtt_version; }

scope::scope() : _is_global_wildcard(true) {}

scope::scope(const string &scope_string) : _is_global_wildcard(false) {
    if (scope_string == hash_sign)
        _is_global_wildcard = true;
    else {
        if (topic_tokens tokens = tokenize_topic_filter(scope_string); not tokens.empty())
            _scope.push_back(tokens);
        else
            throw invalid_topic_filter(scope_string);
    }
}

scope::scope(const std::vector<string> &scope_vector) : _is_global_wildcard(false) {
    for (auto &scope_string : scope_vector) {
        if (scope_string == hash_sign) {
            _is_global_wildcard = true;
            break;
        }
        if (topic_tokens tokens = tokenize_topic_filter(scope_string); not tokens.empty())
            _scope.push_back(tokens);
        else
            throw invalid_topic_filter(scope_string);
    }
}

scope::topic_tokens scope::tokenize_topic_filter(const string &topic_filter) {
    const topic_tokens empty;
    topic_tokens tokens;

    // If topic_filter is empty, return empty vector
    if (topic_filter.empty()) return empty;

    // If topic_filter is just "#" or "+" return vector with this one token
    if (topic_filter == hash_sign or topic_filter == plus_sign) {
        tokens.push_back(topic_filter);
        return tokens;
    }

    for (size_t first = 0; first <= topic_filter.size();) {
        size_t second = topic_filter.find_first_of(slash_sign[0], first);
        // first has index of start of token
        // second has index of end of token + 1;
        if (second == string::npos) second = topic_filter.size();
        string token = topic_filter.substr(first, second - first);
        // Check the token
        // If token == "#" but is not the final token in sequence, the topic filter is invalid
        if (token == hash_sign && second != topic_filter.size()) return empty;
        // If token does not equal to hash sign or plus sign, but does contain
        // hash sign or plus sign somewhere inside, the topic filter is invalid
        if (token != hash_sign and token != plus_sign and
            (token.find_first_of(hash_sign[0]) != string::npos or
             token.find_first_of(plus_sign[0]) != string::npos))
            return empty;
        tokens.push_back(token);
        first = second + 1;
    }

    return tokens;
}

scope::topic_tokens scope::tokenize_topic(const string &topic) {
    const topic_tokens empty;
    topic_tokens tokens;

    // If topic is empty, return empty vector
    if (topic.empty()) return empty;

    // If topic contains "#" or "+" it may be a topic filter, but not a topic.
    // Return empty vector
    if (topic.find_first_of(hash_sign[0]) != string::npos or
        topic.find_first_of(plus_sign[0]) != string::npos)
        return empty;

    for (size_t first = 0; first <= topic.size();) {
        size_t second = topic.find_first_of(slash_sign[0], first);
        // first has index of start of token
        // second has index of end of token + 1;
        if (second == string::npos) second = topic.size();
        string token = topic.substr(first, second - first);
        tokens.push_back(token);
        first = second + 1;
    }

    return tokens;
}

bool scope::compare_topics(const topic_tokens &filter, const topic_tokens &topic) {
    if (filter.size() == 1 and filter.front() == hash_sign) return true;
    if (filter.size() > topic.size()) return false;

    size_t i = 0;
    for (auto &filter_token : filter) {
        if (filter_token == topic[i] or filter_token == plus_sign)
            ++i;
        else
            break;
    }
    if (i < topic.size() and filter[i] != hash_sign) return false;
    return true;
}

bool scope::includes(const string &topic) const {
    if (_is_global_wildcard) return true;

    topic_tokens topic_tokens = tokenize_topic(topic);
    if (topic_tokens.empty()) return false;

    bool match = false;
    // Loop over all topic filters and find matching
    for (auto &filter_tokens : _scope) {
        match = compare_topics(filter_tokens, topic_tokens);
        if (match) break;
    }

    return match;
}

bool scope::valid_topic_filter(const std::string_view &topic_filter) {
    if (topic_filter == hash_sign or topic_filter == plus_sign or topic_filter == slash_sign)
        return true;

    topic_tokens tokens = tokenize_topic_filter(std::string(topic_filter));
    return not tokens.empty();
}

bool scope::matches_filter(const std::string_view &filter, const std::string_view &topic) {
    if (filter == hash_sign) return true;

    topic_tokens filter_tokens = tokenize_topic_filter(std::string(filter));
    topic_tokens topic_tokens = tokenize_topic(std::string(topic));

    if (filter_tokens.empty() or topic_tokens.empty()) return false;
    return compare_topics(filter_tokens, topic_tokens);
}

}  // namespace octopus_mq
