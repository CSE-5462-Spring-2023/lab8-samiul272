#include "Message.h"
#include "Utility.h"
#include <sstream>
#include <iostream>
#include <utility>


std::string strip_quotes(const std::string &input) {
    // Find the position of the first character that is not a double quote
    size_t start = input.find_first_not_of('"');
    if (start == std::string::npos) {
        // String contains only quotes
        return "";
    }

    // Find the position of the last character that is not a double quote
    size_t end = input.find_last_not_of('"');

    // Extract and return the substring without leading and trailing quotes
    return input.substr(start, end - start + 1);
}

BasePacket::BasePacket(unsigned long time, ushort to_port, ushort from_port, short ttl, short version, short flags,
                       int location, int sequence_number) :
        time(time), version(version), flags(flags), to_port(to_port), from_port(from_port), ttl(ttl),
        location(location),
        sequence_number(sequence_number) {}

BasePacket::~BasePacket() = default;

std::unordered_map<std::string, std::string> BasePacket::to_map() const {
    return {
            {"time",            std::to_string(time)},
            {"to_port",         std::to_string(to_port)},
            {"from_port",       std::to_string(from_port)},
            {"ttl",             std::to_string(ttl)},
            {"version",         std::to_string(version)},
            {"flags",           std::to_string(flags)},
            {"location",        std::to_string(location)},
            {"sequence_number", std::to_string(sequence_number)}
    };
}

void BasePacket::from_map(const std::unordered_map<std::string, std::string> &packet_map) {
    time = std::stoul(packet_map.at("time"));
    to_port = static_cast<ushort>(std::stoi(packet_map.at("to_port")));
    from_port = static_cast<ushort>(std::stoi(packet_map.at("from_port")));
    ttl = static_cast<short>(std::stoi(packet_map.at("ttl")));
    version = static_cast<short>(std::stoi(packet_map.at("version")));
    flags = static_cast<short>(std::stoi(packet_map.at("flags")));
    location = std::stoi(packet_map.at("location"));
    sequence_number = std::stoi(packet_map.at("sequence_number"));
}

std::string BasePacket::serialize() const {
    std::string serializedForm = "time:" + std::to_string(time) +
                                 " to_port:" + std::to_string(to_port) +
                                 " from_port:" + std::to_string(from_port) +
                                 " ttl:" + std::to_string(ttl) +
                                 " version:" + std::to_string(version) +
                                 " flags:" + std::to_string(flags) +
                                 " location:" + std::to_string(location) +
                                 " sequence_number:" + std::to_string(sequence_number);
    return serializedForm;
}

__attribute__((unused)) BasePacket *BasePacket::deserialize(__attribute__((unused)) const std::string &packet_string) {
    return nullptr;
}

// Packet implementation
Packet::Packet(unsigned long time, ushort to_port, ushort from_port, short ttl,
               short version, short flags, int location,
               int seqNumber, std::vector<ushort> send_path)
        : BasePacket(time, to_port, from_port, ttl, version, flags, location, seqNumber),
          send_path(std::move(send_path)) {
}

Packet::~Packet() = default;

void Packet::parse_send_path(const std::string &send_path_string, std::vector<ushort> &path) {
    std::istringstream iss(send_path_string);
    std::string port;
    while (std::getline(iss, port, ',')) {
        path.push_back(static_cast<ushort>(std::stoi(port)));
    }
}

std::unordered_map<std::string, std::string> Packet::to_map() const {
    auto map = BasePacket::to_map();
    std::ostringstream send_path_stream;
    for (const auto &port: send_path) {
        if (send_path_stream.tellp() > 0) {
            send_path_stream << ",";
        }
        send_path_stream << port;
    }
    map["send_path"] = send_path_stream.str();
    return map;
}

void Packet::from_map(const std::unordered_map<std::string, std::string> &packet_map) {
    BasePacket::from_map(packet_map);
    std::vector<ushort> path;
    parse_send_path(packet_map.at("send_path"), send_path);
    send_path = path;
}

std::string Packet::serialize() const {
    std::string base_serialized = BasePacket::serialize();
    std::ostringstream send_path_stream;
    for (const auto &port: send_path) {
        if (send_path_stream.tellp() > 0) {
            send_path_stream << ",";
        }
        send_path_stream << port;
    }
    return base_serialized + " send-path:" + send_path_stream.str();
}

// Placeholder
__attribute__((unused)) Packet *Packet::deserialize(const std::string &packet_string) {
    return nullptr;
}

__attribute__((unused)) Packet
Packet::deserialize(std::unordered_map<std::basic_string<char>, std::basic_string<char>> &map) {
    return {0, 0, 0, 0, 0, 0, 0, 0, std::vector<ushort>()};
}

// Message implementation
Message::Message(unsigned long time, std::string msg, ushort to_port, ushort from_port, short ttl, short version,
                 short flags, int location, int sequence_number, std::vector<ushort> sendPath)
        : Packet(time, to_port, from_port, ttl, version, flags, location, sequence_number, std::move(sendPath)),
          msg(std::move(msg)) {
}

std::unordered_map<std::string, std::string> Message::to_map() const {
    auto packet_map = Packet::to_map();
    packet_map["msg"] = strip_quotes(msg);
    return packet_map;
}

void Message::from_map(const std::unordered_map<std::string, std::string> &packet_map) {
    Packet::from_map(packet_map);
    msg = packet_map.at("msg");
}

std::string Message::serialize() const {
    std::ostringstream oss;
    oss << Packet::serialize() << " msg:\"" << strip_quotes(this->msg) << "\"";
    return oss.str();
}

__attribute__((unused)) Message Message::deserialize(const std::string &message_content) {
    const auto map = parse_message(message_content);
    std::vector<ushort> path = {};
    parse_send_path(map.at("send-path"), path);
    Message message(
            std::stoul(map.at("time")),
            map.at("msg"),
            static_cast<ushort>(std::stoi(map.at("to_port"))),
            static_cast<ushort>(std::stoi(map.at("from_port"))),
            static_cast<short>(std::stoi(map.at("ttl"))),
            static_cast<short>(std::stoi(map.at("version"))),
            static_cast<short>(std::stoi(map.at("flags"))),
            std::stoi(map.at("location")),
            (std::stoi(map.at("sequence_number"))),
            path
    );
    return message;
}

Message Message::deserialize(std::unordered_map<std::basic_string<char>, std::basic_string<char>> &map) {
    std::vector<ushort> path = {};
    parse_send_path(map.at("send-path"), path);
    Message message(
            std::stoul(map.at("time")),
            map.at("msg"),
            static_cast<ushort>(std::stoi(map.at("to_port"))),
            static_cast<ushort>(std::stoi(map.at("from_port"))),
            static_cast<short>(std::stoi(map.at("ttl"))),
            static_cast<short>(std::stoi(map.at("version"))),
            static_cast<short>(std::stoi(map.at("flags"))),
            std::stoi(map.at("location")),
            (std::stoi(map.at("sequence_number"))),
            path
    );
    return message;
}

// Acknowledgement implementation
Acknowledgement::Acknowledgement(unsigned long time, std::string type, ushort to_port, ushort from_port,
                                 short ttl, short version, short flags, int location, int sequence_number,
                                 std::vector<ushort> send_path)
        : Packet(time, to_port, from_port, ttl, version, flags, location, sequence_number, std::move(send_path)),
          type(std::move(type)) {
}

std::unordered_map<std::string, std::string> Acknowledgement::to_map() const {
    auto packet_map = Packet::to_map();
    packet_map["type"] = type;
    return packet_map;
}

void Acknowledgement::from_map(const std::unordered_map<std::string, std::string> &packet_map) {
    Packet::from_map(packet_map);
    type = packet_map.at("type");
}

std::string Acknowledgement::serialize() const {
    std::ostringstream oss;
    // Serialize common fields
    oss << Packet::serialize();
    // Add acknowledgement-specific part
    oss << " type:" << this->type;
    return oss.str();
}

__attribute__((unused)) Acknowledgement Acknowledgement::deserialize(const std::string &acknowledgement_string) {
    auto map = parse_message(
            acknowledgement_string); // You need to implement parse_message based on your serialization format
    std::vector<ushort> path = {};
    parse_send_path(map.at("send-path"), path);
    // Create and return an Acknowledgement object initialized with the extracted values
    Acknowledgement acknowledgement(
            std::stoul(map.at("time")),
            "ACK",
            static_cast<ushort>(std::stoi(map.at("to_port"))),
            static_cast<ushort>(std::stoi(map.at("from_port"))),
            static_cast<short>(std::stoi(map.at("ttl"))),
            static_cast<short>(std::stoi(map.at("version"))),
            static_cast<short>(std::stoi(map.at("flags"))),
            std::stoi(map.at("location")),
            (std::stoi(map.at("sequence_number"))),
            path
    );
    return acknowledgement;
}

Acknowledgement
Acknowledgement::deserialize(std::unordered_map<std::basic_string<char>, std::basic_string<char>> &map) {
    std::vector<ushort> path = {};
    parse_send_path(map.at("send-path"), path);
    // Create and return an Acknowledgement object initialized with the extracted values
    Acknowledgement acknowledgement(
            std::stoul(map.at("time")),
            "ACK",
            static_cast<ushort>(std::stoi(map.at("to_port"))),
            static_cast<ushort>(std::stoi(map.at("from_port"))),
            static_cast<short>(std::stoi(map.at("ttl"))),
            static_cast<short>(std::stoi(map.at("version"))),
            static_cast<short>(std::stoi(map.at("flags"))),
            std::stoi(map.at("location")),
            (std::stoi(map.at("sequence_number"))),
            path
    );
    return acknowledgement;
}


MoveCommand::MoveCommand(unsigned long time, ushort to_port, ushort from_port, short ttl,
                         short version, short flags, int location,
                         int sequence_number, int move)
        : BasePacket(time, to_port, from_port, ttl, version, flags, location, sequence_number), move(move) {}

MoveCommand::~MoveCommand() = default;

std::unordered_map<std::string, std::string> MoveCommand::to_map() const {
    auto map = BasePacket::to_map();
    map["move"] = std::to_string(move);
    return map;
}

void MoveCommand::from_map(const std::unordered_map<std::string, std::string> &packet_map) {
    BasePacket::from_map(packet_map); // Call the base class implementation first
    move = std::stoi(packet_map.at("move"));
}

std::string MoveCommand::serialize() const {
    std::ostringstream oss;
    oss << BasePacket::serialize() << " "
        << "move:" << move;
    return oss.str();
}

__attribute__((unused)) MoveCommand MoveCommand::deserialize(const std::string &packetStr) {
    // Implementation of deserialize that converts a string back into a MoveCommand object.
    // You need to implement the logic based on your serialization format.
    // Example parsing logic needed here
    auto map = parse_message(packetStr); // You need to implement parse_message based on your serialization format

    return {
            std::stoul(map.at("time")),
            static_cast<ushort>(std::stoi(map.at("to_port"))),
            static_cast<ushort>(std::stoi(map.at("from_port"))),
            static_cast<short>(std::stoi(map.at("ttl"))),
            static_cast<short>(std::stoi(map.at("version"))),
            static_cast<short>(std::stoi(map.at("flags"))),
            std::stoi(map.at("location")),
            std::stoi(map.at("sequence_number")),
            std::stoi(map.at("move"))
    };
}

__attribute__((unused)) MoveCommand
MoveCommand::deserialize(std::unordered_map<std::basic_string<char>, std::basic_string<char>> &map) {
    return {
            std::stoul(map.at("time")),
            static_cast<ushort>(std::stoi(map.at("to_port"))),
            static_cast<ushort>(std::stoi(map.at("from_port"))),
            static_cast<short>(std::stoi(map.at("ttl"))),
            static_cast<short>(std::stoi(map.at("version"))),
            static_cast<short>(std::stoi(map.at("flags"))),
            std::stoi(map.at("location")),
            std::stoi(map.at("sequence_number")),
            std::stoi(map.at("move"))
    };
}



