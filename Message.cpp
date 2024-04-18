#include "Message.h"
#include "Utility.h"
#include <sstream>
#include <iostream>
#include <utility>


std::string stripQuotes(const std::string &input) {
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

BasePacket::BasePacket(unsigned long time, ushort toPort, ushort fromPort, short TTL, short version, short flags,
                       int location, int seqNumber) :
        time(time), version(version), flags(flags), toPort(toPort), fromPort(fromPort), TTL(TTL), location(location),
        seqNumber(seqNumber) {}

BasePacket::~BasePacket() = default;

std::unordered_map<std::string, std::string> BasePacket::toMap() const {
    return {
            {"time", std::to_string(time)},
            {"toPort", std::to_string(toPort)},
            {"fromPort", std::to_string(fromPort)},
            {"TTL", std::to_string(TTL)},
            {"version", std::to_string(version)},
            {"flags", std::to_string(flags)},
            {"location", std::to_string(location)},
            {"seqNumber", std::to_string(seqNumber)}
    };
}

void BasePacket::fromMap(const std::unordered_map<std::string, std::string> &packetMap) {
    time = std::stoul(packetMap.at("time"));
    toPort = static_cast<ushort>(std::stoi(packetMap.at("toPort")));
    fromPort = static_cast<ushort>(std::stoi(packetMap.at("fromPort")));
    TTL = static_cast<short>(std::stoi(packetMap.at("TTL")));
    version = static_cast<short>(std::stoi(packetMap.at("version")));
    flags = static_cast<short>(std::stoi(packetMap.at("flags")));
    location = std::stoi(packetMap.at("location"));
    seqNumber = std::stoi(packetMap.at("seqNumber"));
}

std::string BasePacket::serialize() const {
    std::string serializedForm = "time:" + std::to_string(time) +
                                 " toPort:" + std::to_string(toPort) +
                                 " fromPort:" + std::to_string(fromPort) +
                                 " TTL:" + std::to_string(TTL) +
                                 " version:" + std::to_string(version) +
                                 " flags:" + std::to_string(flags) +
                                 " location:" + std::to_string(location) +
                                 " seqNumber:" + std::to_string(seqNumber);
    return serializedForm;
}

BasePacket *BasePacket::deserialize(const std::string &packetStr) {
    return nullptr;
}

// Packet implementation
Packet::Packet(unsigned long time, ushort toPort, ushort fromPort, short TTL,
               short version, short flags, int location,
               int seqNumber, std::vector<ushort> sendPath)
        : BasePacket(time, toPort, fromPort, TTL, version, flags, location, seqNumber), sendPath(std::move(sendPath)) {
}

Packet::~Packet() = default;

void Packet::parseSendPath(const std::string &sendPathStr, std::vector<ushort> &path) {
    std::istringstream iss(sendPathStr);
    std::string port;
    while (std::getline(iss, port, ',')) {
        path.push_back(static_cast<ushort>(std::stoi(port)));
    }
}

std::unordered_map<std::string, std::string> Packet::toMap() const {
    auto map = BasePacket::toMap();
    std::ostringstream sendPathStr;
    for (const auto& port : sendPath) {
        if (sendPathStr.tellp() > 0) {
            sendPathStr << ",";
        }
        sendPathStr << port;
    }
    map["sendPath"] = sendPathStr.str();
    return map;
}

void Packet::fromMap(const std::unordered_map<std::string, std::string> &packetMap) {
    BasePacket::fromMap(packetMap);
    std::vector<ushort> path;
    parseSendPath(packetMap.at("sendPath"), sendPath);
    sendPath = path;
}

std::string Packet::serialize() const {
    std::string baseSerialized = BasePacket::serialize();
    std::ostringstream sendPathStr;
    for (const auto &port: sendPath) {
        if (sendPathStr.tellp() > 0) {
            sendPathStr << ",";
        }
        sendPathStr << port;
    }
    return baseSerialized + " send-path:" + sendPathStr.str();
}
// Placeholder for demonstration; the actual function should parse the string based on the serialization format
Packet *Packet::deserialize(const std::string &packetStr) {
    return nullptr;
}

Packet Packet::deserialize(std::unordered_map<std::basic_string<char>, std::basic_string<char>> &map) {
    return {0, 0, 0, 0, 0, 0, 0, 0, std::vector<ushort>()};
}

// Message implementation
Message::Message(unsigned long time, std::string msg, ushort toPort, ushort fromPort, short TTL, short version,
                 short flags, int location, int seqNumber, std::vector<ushort> sendPath)
        : Packet(time, toPort, fromPort, TTL, version, flags, location, seqNumber, std::move(sendPath)),
          msg(std::move(msg)) {
}

std::unordered_map<std::string, std::string> Message::toMap() const {
    auto packetMap = Packet::toMap();
    packetMap["msg"] = stripQuotes(msg);
    return packetMap;
}

void Message::fromMap(const std::unordered_map<std::string, std::string> &packetMap) {
    Packet::fromMap(packetMap);
    msg = packetMap.at("msg");
}

std::string Message::serialize() const {
    std::ostringstream oss;
    oss << Packet::serialize() << " msg:\"" << stripQuotes(this->msg) << "\"";
    return oss.str();
}

Message Message::deserialize(const std::string &messageStr) {
    const auto map = parseMessage(messageStr);
    std::vector<ushort> path = {};
    parseSendPath(map.at("send-path"), path);
    Message message(
            std::stoul(map.at("time")),
            map.at("msg"),
            static_cast<ushort>(std::stoi(map.at("toPort"))),
            static_cast<ushort>(std::stoi(map.at("fromPort"))),
            static_cast<short>(std::stoi(map.at("TTL"))),
            static_cast<short>(std::stoi(map.at("version"))),
            static_cast<short>(std::stoi(map.at("flags"))),
            std::stoi(map.at("location")),
            (std::stoi(map.at("seqNumber"))),
            path
    );
    return message;
}

Message Message::deserialize(std::unordered_map<std::basic_string<char>, std::basic_string<char>> &map) {
    std::vector<ushort> path = {};
    parseSendPath(map.at("send-path"), path);
    Message message(
            std::stoul(map.at("time")),
            map.at("msg"),
            static_cast<ushort>(std::stoi(map.at("toPort"))),
            static_cast<ushort>(std::stoi(map.at("fromPort"))),
            static_cast<short>(std::stoi(map.at("TTL"))),
            static_cast<short>(std::stoi(map.at("version"))),
            static_cast<short>(std::stoi(map.at("flags"))),
            std::stoi(map.at("location")),
            (std::stoi(map.at("seqNumber"))),
            path
    );
    return message;
}

// Acknowledgement implementation
Acknowledgement::Acknowledgement(unsigned long time, std::string type, ushort toPort, ushort fromPort,
                                 short TTL, short version, short flags, int location, int seqNumber,
                                 std::vector<ushort> sendPath)
        : Packet(time, toPort, fromPort, TTL, version, flags, location, seqNumber, std::move(sendPath)),
          type(std::move(type)) {
}

std::unordered_map<std::string, std::string> Acknowledgement::toMap() const {
    auto packetMap = Packet::toMap();
    packetMap["type"] = type;
    return packetMap;
}

void Acknowledgement::fromMap(const std::unordered_map<std::string, std::string> &packetMap) {
    Packet::fromMap(packetMap);
    type = packetMap.at("type");
}

std::string Acknowledgement::serialize() const {
    std::ostringstream oss;
    // Serialize common fields
    oss << Packet::serialize();
    // Add acknowledgement-specific part
    oss << " type:" << this->type;
    return oss.str();
}

Acknowledgement Acknowledgement::deserialize(const std::string &ackStr) {
    auto map = parseMessage(ackStr); // You need to implement parseMessage based on your serialization format
    std::vector<ushort> path = {};
    parseSendPath(map.at("send-path"), path);
    // Create and return an Acknowledgement object initialized with the extracted values
    Acknowledgement acknowledgement(
            std::stoul(map.at("time")),
            "ACK",
            static_cast<ushort>(std::stoi(map.at("toPort"))),
            static_cast<ushort>(std::stoi(map.at("fromPort"))),
            static_cast<short>(std::stoi(map.at("TTL"))),
            static_cast<short>(std::stoi(map.at("version"))),
            static_cast<short>(std::stoi(map.at("flags"))),
            std::stoi(map.at("location")),
            (std::stoi(map.at("seqNumber"))),
            path
    );
    return acknowledgement;
}

Acknowledgement
Acknowledgement::deserialize(std::unordered_map<std::basic_string<char>, std::basic_string<char>> &map) {
    std::vector<ushort> path = {};
    parseSendPath(map.at("send-path"), path);
    // Create and return an Acknowledgement object initialized with the extracted values
    Acknowledgement acknowledgement(
            std::stoul(map.at("time")),
            "ACK",
            static_cast<ushort>(std::stoi(map.at("toPort"))),
            static_cast<ushort>(std::stoi(map.at("fromPort"))),
            static_cast<short>(std::stoi(map.at("TTL"))),
            static_cast<short>(std::stoi(map.at("version"))),
            static_cast<short>(std::stoi(map.at("flags"))),
            std::stoi(map.at("location")),
            (std::stoi(map.at("seqNumber"))),
            path
    );
    return acknowledgement;
}


MoveCommand::MoveCommand(unsigned long time, ushort toPort, ushort fromPort, short TTL,
                         short version, short flags, int location,
                         int seqNumber, int move)
        : BasePacket(time, toPort, fromPort, TTL, version, flags, location, seqNumber), move(move) {}

MoveCommand::~MoveCommand() = default;

std::unordered_map<std::string, std::string> MoveCommand::toMap() const {
    auto map = BasePacket::toMap();
    map["move"] = std::to_string(move);
    return map;
}

void MoveCommand::fromMap(const std::unordered_map<std::string, std::string>& packetMap) {
    BasePacket::fromMap(packetMap); // Call the base class implementation first
    move = std::stoi(packetMap.at("move"));
}

std::string MoveCommand::serialize() const {
    std::ostringstream oss;
    oss << BasePacket::serialize() << " "
        << "move:" << move;
    return oss.str();
}

MoveCommand MoveCommand::deserialize(const std::string& packetStr) {
    // Implementation of deserialize that converts a string back into a MoveCommand object.
    // You need to implement the logic based on your serialization format.
    // Example parsing logic needed here
    auto map = parseMessage(packetStr); // You need to implement parseMessage based on your serialization format

    return {
            std::stoul(map.at("time")),
            static_cast<ushort>(std::stoi(map.at("toPort"))),
            static_cast<ushort>(std::stoi(map.at("fromPort"))),
            static_cast<short>(std::stoi(map.at("TTL"))),
            static_cast<short>(std::stoi(map.at("version"))),
            static_cast<short>(std::stoi(map.at("flags"))),
            std::stoi(map.at("location")),
            std::stoi(map.at("seqNumber")),
            std::stoi(map.at("move"))
    };
}

MoveCommand MoveCommand::deserialize(std::unordered_map<std::basic_string<char>, std::basic_string<char>> &map) {
    return {
            std::stoul(map.at("time")),
            static_cast<ushort>(std::stoi(map.at("toPort"))),
            static_cast<ushort>(std::stoi(map.at("fromPort"))),
            static_cast<short>(std::stoi(map.at("TTL"))),
            static_cast<short>(std::stoi(map.at("version"))),
            static_cast<short>(std::stoi(map.at("flags"))),
            std::stoi(map.at("location")),
            std::stoi(map.at("seqNumber")),
            std::stoi(map.at("move"))
    };
}



