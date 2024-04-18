#include "Utility.h"
#include "Message.h"
#include <iostream>
#include <fstream>
#include <sstream>
#include <ctime>
#include <cstring>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <set>
#include <cmath>
#include <memory>

std::unordered_map<std::string, std::string> parse_message(const std::string &message) {
    std::unordered_map<std::string, std::string> map;
    std::set<std::string> seen_keys;
    std::string key, value;
    bool in_quote = false;
    bool is_key = true;

    std::set<std::string> allowed_keys = {
            "time",
            "msg",
            "type",
            "move",
            "to_port",
            "from_port",
            "ttl",
            "version",
            "flags",
            "location",
            "sequence_number",
            "send-path"
    };\
    std::set<std::string> required_keys = {
            "time", "to_port", "from_port", "ttl", "version", "flags", "location", "sequence_number"
    };

    for (size_t i = 0; i < message.length(); ++i) {
        if (message[i] == '"' && (i == 0 || message[i - 1] != '\\')) {
            in_quote = !in_quote;
        } else if (message[i] == ':' && !in_quote) {
            is_key = false;
        } else if ((message[i] == ' ' && !in_quote && is_key) || (message[i] == ' ' && in_quote)) {
            // Allow spaces within quotes for msg value
            value += message[i]; // Add space to value even outside quotes for key
        }

        // Handle end of message or end of value segment
        if (!in_quote && (message[i] == ' ' || i == message.length() - 1)) {
            if (i == message.length() - 1 && message[i] != ' ') {
                value += message[i]; // Ensure last character is included if not space
            }
            if (!key.empty()) {
                if (seen_keys.find(key) != seen_keys.end()) {
                    std::cerr << key << " found more than once" << std::endl;
                    return {}; // Duplicate key
                }
                if (allowed_keys.find(key) == allowed_keys.end()) {
                    std::cerr << key << " is not in protocol" << std::endl;
                    return {}; // Unknown key
                }
                map[key] = value;
                seen_keys.insert(key);
            }
            key.clear();
            value.clear();
            is_key = true; // Ready to read next key
        } else {
            // Accumulate characters into key or value
            if (is_key) key += message[i];
            else if (message[i] == ':' && !in_quote) continue;
            else if (message[i] != 32) value += message[i];
        }
    }

    // Check if any required key is missing
    for (const auto &required_key: required_keys) {
        if (seen_keys.find(required_key) == seen_keys.end()) {
            std::cerr << required_key << " is missing" << std::endl;
            return {}; // Missing key
        }
    }

    return map;
}


std::vector<ConfigEntry> read_config(const std::string &file_path) {
    std::vector<ConfigEntry> config;
    std::ifstream file(file_path);
    if (!file) {
        std::cerr << "Could not open file: " << file_path << std::endl;
        return config; // Return the empty vector
    }
    std::string line;
    while (std::getline(file, line)) {
        std::istringstream iss(line);
        std::string ip;
        ushort port;
        int location;
        if (iss >> ip >> port >> location) {
            config.emplace_back(ip, port, location, 0, 0);
        }
    }
    return config;
}

long unsigned int get_current_UTC_time() {
    return static_cast<unsigned long>(std::time(nullptr));
}


void send_message(const std::string &ip, ushort port, const std::string &message) {
    int socket_file_descriptor = socket(AF_INET, SOCK_DGRAM, 0);
    if (socket_file_descriptor < 0) {
        std::cerr << "Error opening socket" << std::endl;
        return;
    }

    struct sockaddr_in server_address{};
    memset(&server_address, 0, sizeof(server_address));
    server_address.sin_family = AF_INET;
    server_address.sin_port = htons(port);
    inet_pton(AF_INET, ip.c_str(), &server_address.sin_addr);

    sendto(socket_file_descriptor, message.c_str(), message.length(), 0, (const struct sockaddr *) &server_address,
           sizeof(server_address));
    close(socket_file_descriptor);
}


int setup_listen_socket(int listen_port) {
    int socket_file_descriptor = socket(AF_INET, SOCK_DGRAM, 0);
    if (socket_file_descriptor < 0) {
        std::cerr << "Error opening socket" << std::endl;
        exit(EXIT_FAILURE);
    }

    struct sockaddr_in server_address{};
    memset(&server_address, 0, sizeof(server_address));
    server_address.sin_family = AF_INET;
    server_address.sin_addr.s_addr = htonl(INADDR_ANY);
    server_address.sin_port = htons(listen_port);

    if (bind(socket_file_descriptor, (struct sockaddr *) &server_address, sizeof(server_address)) < 0) {
        std::cerr << "Bind failed" << std::endl;
        exit(EXIT_FAILURE);
    }

    return socket_file_descriptor;
}

void send_message_to_entry(const ConfigEntry &entry, const BasePacket &packet) {
    std::string formatted_message = packet.serialize();
    send_message(entry.ip, entry.port, formatted_message);
}

std::pair<std::unique_ptr<Packet>, bool>
process_incoming_message(std::unordered_map<std::basic_string<char>, std::basic_string<char>> map) {
    try {
        // Attempt to deserialize as a Message
        Message message = Message::deserialize(map);
        // Successfully deserialized a Message, return it with true
        // C++11 does not have std::make_unique, so use std::unique_ptr constructor directly
        return std::make_pair(std::unique_ptr<Message>(new Message(std::move(message))), true);
    } catch (const std::exception &e) {
        // Log or handle Message deserialization failure
        std::cerr << "Message deserialization failed: " << e.what() << std::endl;
    }

    try {
        // If Message deserialization fails, try as an Acknowledgement
        Acknowledgement ack = Acknowledgement::deserialize(map);
        // Successfully deserialized an Acknowledgement, return it with false
        return std::make_pair(std::unique_ptr<Acknowledgement>(new Acknowledgement(std::move(ack))), false);
    } catch (const std::exception &e) {
        // Log or handle Acknowledgement deserialization failure
        std::cerr << "Acknowledgement deserialization failed: " << e.what() << std::endl;
    }
    std::cerr << "Deserialization failed: " << std::endl;


    // If both deserialization operations fail, return nullptr with false
    return std::make_pair(std::unique_ptr<Packet>(nullptr), false);
}
