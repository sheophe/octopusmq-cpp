#include "network/bridge/protocol.hpp"

#include <algorithm>
#include <cstring>
#include <map>
#include <utility>

#include <boost/iostreams/filtering_streambuf.hpp>
#include <boost/iostreams/filter/bzip2.hpp>
#include <boost/iostreams/filter/gzip.hpp>
#include <boost/iostreams/filter/zlib.hpp>

namespace octopus_mq::bridge::protocol::v1 {

static std::map<const packet_type, const std::string> packet_name_map = {
    { packet_type::probe, packet_name::probe },
    { packet_type::publish, packet_name::publish },
    { packet_type::heartbeat, packet_name::heartbeat },
    { packet_type::acknack, packet_name::acknack }
};

const std::string packet_name::from_type(const packet_type& type) { return packet_name_map[type]; }

opayload_stream::opayload_stream(network_payload& payload) : _payload(payload) {}

opayload_stream& opayload_stream::operator<<(const std::uint8_t& value) {
    _payload.push_back(static_cast<char>(value));
    return *this;
}

opayload_stream& opayload_stream::operator<<(const std::uint16_t& value) {
    _payload.push_back(static_cast<char>(value & 0xff));
    _payload.push_back(static_cast<char>((value >> 8) & 0xff));
    return *this;
}

opayload_stream& opayload_stream::operator<<(const std::uint32_t& value) {
    _payload.push_back(static_cast<char>(value & 0xff));
    _payload.push_back(static_cast<char>((value >> 8) & 0xff));
    _payload.push_back(static_cast<char>((value >> 16) & 0xff));
    _payload.push_back(static_cast<char>((value >> 24) & 0xff));
    return *this;
}

opayload_stream& opayload_stream::operator<<(const std::uint64_t& value) {
    _payload.push_back(static_cast<char>(value & 0xff));
    _payload.push_back(static_cast<char>((value >> 8) & 0xff));
    _payload.push_back(static_cast<char>((value >> 16) & 0xff));
    _payload.push_back(static_cast<char>((value >> 24) & 0xff));
    _payload.push_back(static_cast<char>((value >> 32) & 0xff));
    _payload.push_back(static_cast<char>((value >> 40) & 0xff));
    _payload.push_back(static_cast<char>((value >> 48) & 0xff));
    _payload.push_back(static_cast<char>((value >> 56) & 0xff));
    return *this;
}

opayload_stream& opayload_stream::operator<<(const version& value) {
    _payload.push_back(static_cast<char>(value));
    return *this;
}

opayload_stream& opayload_stream::operator<<(const packet_type& value) {
    _payload.push_back(static_cast<char>(value));
    return *this;
}

opayload_stream& opayload_stream::operator<<(const std::string& value) {
    const std::size_t original_size = _payload.size();
    _payload.resize(original_size + value.size());
    std::copy(value.begin(), value.end(), _payload.begin() + original_size);
    _payload.push_back('\0');
    return *this;
}

opayload_stream& opayload_stream::operator<<(const network_payload_ptr& value) {
    const std::size_t original_size = _payload.size();
    _payload.resize(original_size + value->size());
    std::copy(value->begin(), value->end(), _payload.begin() + original_size);
    return *this;
}

void opayload_stream::push_payload(const network_payload_iter_pair& iter_pair) {
    const std::size_t payload_size = iter_pair.second - iter_pair.first;
    network_payload::iterator start_iter = _payload.end();
    _payload.resize(_payload.size() + payload_size);
    std::copy(iter_pair.first, iter_pair.second, start_iter);
}

ipayload_stream::ipayload_stream(const network_payload& payload)
    : _payload(payload), _iterator(_payload.begin()) {}

ipayload_stream& ipayload_stream::operator>>(std::uint8_t& value) {
    if (_iterator + sizeof(std::uint8_t) > _payload.end())
        throw ipayload_stream_out_of_range("uint8_t");
    value = static_cast<std::uint8_t>(*_iterator++);
    return *this;
}

ipayload_stream& ipayload_stream::operator>>(std::uint16_t& value) {
    if (_iterator + sizeof(std::uint16_t) > _payload.end())
        throw ipayload_stream_out_of_range("uint16_t");
    char* value_array = reinterpret_cast<char*>(&value);
    value_array[0] = (*_iterator++);
    value_array[1] = (*_iterator++);
    return *this;
}

ipayload_stream& ipayload_stream::operator>>(std::uint32_t& value) {
    if (_iterator + sizeof(std::uint32_t) > _payload.end())
        throw ipayload_stream_out_of_range("uint32_t");
    char* value_array = reinterpret_cast<char*>(&value);
    value_array[0] = (*_iterator++);
    value_array[1] = (*_iterator++);
    value_array[2] = (*_iterator++);
    value_array[3] = (*_iterator++);
    return *this;
}

ipayload_stream& ipayload_stream::operator>>(std::uint64_t& value) {
    if (_iterator + sizeof(std::uint64_t) > _payload.end())
        throw ipayload_stream_out_of_range("uint64_t");
    char* value_array = reinterpret_cast<char*>(&value);
    value_array[0] = (*_iterator++);
    value_array[1] = (*_iterator++);
    value_array[2] = (*_iterator++);
    value_array[3] = (*_iterator++);
    value_array[4] = (*_iterator++);
    value_array[5] = (*_iterator++);
    value_array[6] = (*_iterator++);
    value_array[7] = (*_iterator++);
    return *this;
}

ipayload_stream& ipayload_stream::operator>>(version& value) {
    if (_iterator + sizeof(version) > _payload.end()) throw ipayload_stream_out_of_range("version");
    value = static_cast<version>(*_iterator++);
    return *this;
}

ipayload_stream& ipayload_stream::operator>>(packet_type& value) {
    if (_iterator + sizeof(packet_type) > _payload.end())
        throw ipayload_stream_out_of_range("packet_type");
    value = static_cast<packet_type>(*_iterator++);
    return *this;
}

ipayload_stream& ipayload_stream::operator>>(std::string& value) {
    value.clear();
    for (char c = *_iterator; c != 0 and _iterator < _payload.end(); c = *_iterator++)
        value.push_back(c);
    return *this;
}

void ipayload_stream::skip_header() { _iterator += constants::header_size; }

void ipayload_stream::read(network_payload& destination, const std::size_t size) {
    destination.resize(size);
    std::copy(_iterator, _iterator + size, destination.begin());
    _iterator += size;
}

const network_payload::const_iterator& ipayload_stream::current_iterator() const {
    return _iterator;
}

packet::packet(const packet_type& packet_type, const address& sender_address)
    : magic(constants::magic_number),
      version(version::v1),
      type(packet_type),
      sender_address(sender_address) {
    opayload_stream ops(payload);
    ops << this->magic << this->version << this->type << this->sender_address.ip()
        << this->sender_address.port();
}

packet::packet(const packet_type& packet_type, network_payload_ptr payload)
    : magic(0), version(version::v1), type(packet_type) {
    std::uint8_t version_n;
    std::uint8_t type_n;
    ipayload_stream ips(*payload);
    ips >> magic >> version_n >> type_n;
    if (magic != constants::magic_number) throw invalid_magic_number();
    if (version_n < static_cast<std::uint8_t>(min_version) or
        version_n > static_cast<std::uint8_t>(max_version))
        throw unsupported_version();
    version = static_cast<protocol::version>(version_n);
    ip_int sender_ip = network::constants::null_ip;
    port_int sender_port = network::constants::null_port;
    ips >> sender_ip >> sender_port;
    sender_address = address(sender_ip, sender_port);
}

const std::string packet::type_name() const { return packet_name::from_type(type); }

std::pair<packet_type, address> packet::meta_from_payload(const network_payload_ptr& payload) {
    const char* payload_data = payload->data();
    const std::uint8_t type_n =
        static_cast<const std::uint8_t>(*(payload_data + constants::type_offset));
    ip_int ip = network::constants::null_ip;
    port_int port = network::constants::null_port;
    std::copy(payload_data + constants::ip_offset,
              payload_data + constants::ip_offset + constants::ip_size, &ip);
    std::copy(payload_data + constants::port_offset,
              payload_data + constants::port_offset + constants::port_size, &port);
    return std::make_pair(static_cast<packet_type>(type_n), address(ip, port));
}

probe::probe(const address& sender_address, const scope& sender_scope)
    : packet(packet_type::probe, sender_address), sender_scope(sender_scope) {
    opayload_stream ops(payload);
    ops << static_cast<std::uint32_t>(this->sender_scope.size());
    for (auto& scope_string : this->sender_scope.scope_strings()) ops << scope_string;
}

probe::probe(network_payload_ptr payload) : packet(packet_type::probe, payload) {
    ipayload_stream ips(*payload);
    ips.skip_header();
    std::uint32_t scope_size = 0;
    ips >> scope_size;
    std::string scope_string;
    while (scope_size) {
        ips >> scope_string;
        sender_scope.add(scope_string);
        --scope_size;
    }
}

heartbeat::heartbeat(const address& sender_address, const std::uint32_t& packet_id,
                     const std::chrono::milliseconds& interval,
                     const std::vector<std::uint64_t>& published_hashes)
    : packet(packet_type::heartbeat, sender_address),
      packet_id(packet_id),
      interval(static_cast<std::uint32_t>(interval.count())),
      published_n(published_hashes.size()) {
    opayload_stream ops(payload);
    ops << this->packet_id << this->interval << this->published_n;
    for (auto& hash : published_hashes) ops << hash;
}

heartbeat::heartbeat(network_payload_ptr payload) : packet(packet_type::heartbeat, payload) {
    ipayload_stream ips(*payload);
    ips.skip_header();
    ips >> packet_id >> interval >> published_n;
    std::uint64_t published_hash = 0;
    while (published_n) {
        ips >> published_hash;
        published_hashes.push_back(published_hash);
        --published_n;
    }
    published_n = published_hashes.size();
}

acknack::acknack(const address& sender_address, const std::uint32_t& packet_id)
    : packet(packet_type::acknack, sender_address), packet_id(packet_id) {
    opayload_stream ops(payload);
    ops << this->packet_id;
}

acknack::acknack(network_payload_ptr payload) : packet(packet_type::acknack, payload) {
    ipayload_stream ips(*payload);
    ips.skip_header();
    ips >> packet_id;
}

void publication::emplace(const message_ptr message) {
    if (message) _messages.push_back(message);
}

void publication::clear() { _messages.clear(); }

bool publication::empty() { return _messages.empty(); }

std::size_t publication::total_blocks() const { return _total_blocks; }

compression_type publication::compression() const { return _compression; }

void publication::read(network_payload_iter_pair& block_iterators) {
    if (_payload.empty()) {
        block_iterators.first = _payload.end();
        block_iterators.second = block_iterators.first;
    } else {
        network_payload::const_iterator block_end =
            (_iter > _payload.end()) ? _payload.end()
                                     : _iter + constants::packet_size::max_publish_payload;
        block_iterators = std::make_pair(_iter, block_end);
        _iter += constants::packet_size::max_publish_payload;
    }
}

void publication::generate_payload(const compression_type& compression) {
    _compression = compression;
    network_payload uncompressed_payload;
    opayload_stream uops(uncompressed_payload);
    uops << static_cast<std::uint32_t>(_messages.size());
    for (auto& message : _messages) {
        const mqtt::version& mqtt_version = message->mqtt_version();
        const address& message_origin = message->origin_addr();
        uops << message_origin.ip() << message_origin.port() << message->origin_clid()
             << message->topic() << static_cast<std::uint8_t>(mqtt_version) << message->pubopts()
             << static_cast<std::uint32_t>(message->payload().size());
        if (mqtt_version == mqtt::version::v5) {
            // TODO: add props to payload.
        }
        uops.push_payload(std::make_pair(message->payload().begin(), message->payload().end()));
    }
    if (_compression == compression_type::none)
        _payload = uncompressed_payload;
    else {
        boost::iostreams::filtering_streambuf<boost::iostreams::input> compression_sb;
        switch (_compression) {
            case compression_type::bzip2:
                compression_sb.push(boost::iostreams::bzip2_compressor());
                break;
            case compression_type::gzip:
                compression_sb.push(boost::iostreams::gzip_compressor());
                break;
            case compression_type::zlib:
                compression_sb.push(boost::iostreams::zlib_compressor());
                break;
            default:
                break;
        }
        auto source = boost::iostreams::array_source(uncompressed_payload.data(),
                                                     uncompressed_payload.size());
        compression_sb.push(source);
        _payload.assign(std::istreambuf_iterator<char>{ &compression_sb }, {});
    }
    _iter = _payload.begin();
}

void publication::from_payload(network_payload_ptr payload, const compression_type& compression) {
    _compression = compression;
    if (_compression == compression_type::none)
        _payload = *payload;
    else {
        boost::iostreams::filtering_streambuf<boost::iostreams::input> decompression_sb;
        switch (_compression) {
            case compression_type::bzip2:
                decompression_sb.push(boost::iostreams::bzip2_decompressor());
                break;
            case compression_type::gzip:
                decompression_sb.push(boost::iostreams::gzip_decompressor());
                break;
            case compression_type::zlib:
                decompression_sb.push(boost::iostreams::zlib_decompressor());
                break;
            default:
                break;
        }
        auto source = boost::iostreams::array_source(payload->data(), payload->size());
        decompression_sb.push(source);
        _payload.assign(std::istreambuf_iterator<char>{ &decompression_sb }, {});
    }
    ipayload_stream ips(_payload);
    std::uint32_t messages_n = 0;
    ips >> messages_n;
    ip_int origin_ip = network::constants::null_ip;
    port_int origin_port = network::constants::null_port;
    std::string origin_clid;
    std::string topic;
    std::uint8_t mqtt_version_n = 0;
    std::uint8_t mqtt_pubopts = 0;
    std::uint32_t message_size = 0;
    while (messages_n) {
        message_payload message_data;
        ips >> origin_ip >> origin_port >> origin_clid >> topic >> mqtt_version_n >> mqtt_pubopts >>
            message_size;
        ips.read(message_data, message_size);
        address origin_address = address(origin_ip, origin_port);
        mqtt::version version = static_cast<mqtt::version>(mqtt_version_n);
        _messages.push_back(std::make_shared<message>(std::move(message_data), topic, mqtt_pubopts,
                                                      origin_address, origin_clid, version));
        --messages_n;
    }
}

publish::publish(const address& sender_address, const std::uint32_t& packet_id, publication_ptr pub,
                 const std::size_t& block_number)
    : packet(packet_type::publish, sender_address),
      packet_id(packet_id),
      compression(pub->compression()),
      total_blocks(pub->total_blocks()),
      block_n(block_number),
      block_size(0) {
    network_payload_iter_pair iters;
    pub->read(iters);
    opayload_stream ops(payload);
    ops << this->packet_id << static_cast<std::uint8_t>(this->compression) << this->total_blocks
        << this->block_n << static_cast<std::uint32_t>(iters.second - iters.first);
    ops.push_payload(iters);
}

publish::publish(network_payload_ptr payload) : packet(packet_type::publish, payload) {
    ipayload_stream ips(*payload);
    ips.skip_header();
    std::uint8_t compression_int = 0;
    ips >> this->packet_id >> compression_int >> this->total_blocks >> this->block_n >>
        this->block_size;
    compression = static_cast<compression_type>(compression_int);
    const network_payload::const_iterator begin_iterator = ips.current_iterator();
    const network_payload::const_iterator end_iterator = begin_iterator + this->block_size;
    if (end_iterator > payload->end()) throw bridge_malformed_packet(packet_name::publish);
    compressed_payload_block.resize(this->block_size);
    std::copy(begin_iterator, end_iterator, compressed_payload_block.begin());
}

packet_ptr packet_factory::from_payload(const network_payload_ptr& payload,
                                        const std::size_t& size) {
    if (size < constants::header_size) throw packet_too_small();

    const std::uint8_t type_int =
        static_cast<std::uint8_t>(*(payload.get()->data() + constants::type_offset));
    if (type_int < constants::min_type or type_int > constants::max_type)
        throw invalid_packet_type();

    switch (static_cast<packet_type>(type_int)) {
        case packet_type::probe:
            return std::make_unique<probe>(payload);
        case packet_type::publish:
            return std::make_unique<publish>(payload);
        case packet_type::heartbeat:
            return std::make_unique<heartbeat>(payload);
        case packet_type::acknack:
            return std::make_unique<acknack>(payload);
        default:
            return nullptr;
    }
}

}  // namespace octopus_mq::bridge::protocol::v1
