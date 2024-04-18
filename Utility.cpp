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
#include <math.h>
#include <memory>

std::unordered_map<std::string, std::string> parseMessage(const std::string &message) {
    std::unordered_map<std::string, std::string> map;
    std::set<std::string> seenKeys;
    std::string key, value;
    bool inQuote = false;
    bool isKey = true;

    std::set<std::string> allowedKeys = {
        "time", "msg", "type", "move", "toPort", "fromPort", "TTL", "version", "flags", "location", "seqNumber", "send-path"
    };
    std::set<std::string> requiredKeys = {
        "time", "toPort", "fromPort", "TTL", "version", "flags", "location", "seqNumber"
    };

    for (size_t i = 0; i < message.length(); ++i) {
        if (message[i] == '"' && (i == 0 || message[i - 1] != '\\')) {
            inQuote = !inQuote;
        } else if (message[i] == ':' && !inQuote) {
            isKey = false;
        }  else if ((message[i] == ' ' && !inQuote && isKey) || (message[i] == ' ' && inQuote)) {
            // Allow spaces within quotes for msg value
            value += message[i]; // Add space to value even outside quotes for key
        }

        // Handle end of message or end of value segment
        if (!inQuote && (message[i] == ' ' || i == message.length() - 1)) {
            if (i == message.length() - 1 && message[i] != ' ') {
                value += message[i]; // Ensure last character is included if not space
            }
            if (!key.empty()) {
                if (seenKeys.find(key) != seenKeys.end()) {
                    std::cerr << key << " found more than once" << std::endl;
                    return std::unordered_map<std::string, std::string>(); // Duplicate key
                }
                if (allowedKeys.find(key) == allowedKeys.end()) {
                    std::cerr << key << " is not in protocol" << std::endl;
                    return std::unordered_map<std::string, std::string>(); // Unknown key
                }
                map[key] = value;
                seenKeys.insert(key);
            }
            key.clear();
            value.clear();
            isKey = true; // Ready to read next key
        } else {
            // Accumulate characters into key or value
            if (isKey) key += message[i];
            else if (!isKey && (message[i] == ':' && !inQuote)) continue;
            else if (message[i] != 32) value += message[i];
        }
    }

    // Check if any required key is missing
    for (const auto &reqKey: requiredKeys) {
        if (seenKeys.find(reqKey) == seenKeys.end()) {
            std::cerr << reqKey << " is missing" << std::endl;
            return std::unordered_map<std::string, std::string>(); // Missing key
        }
    }
//    if ((seenKeys.find("msg") == seenKeys.end()) && (seenKeys.find("type") == seenKeys.end())) {
//        std::cout << "Neither msg or type key is present" << std::endl;
//        return std::unordered_map<std::string, std::string>(); // Missing key
//    }

    return map;
}


std::vector<ConfigEntry> readConfig(const std::string &filePath) {
    std::vector<ConfigEntry> config;
    std::ifstream file(filePath);
    if (!file) {
        std::cerr << "Could not open file: " << filePath << std::endl;
        return config; // Return the empty vector
    }
    std::string line;
    while (std::getline(file, line)) {
        // std::cout << "Read line: " << line << std::endl;
        std::istringstream iss(line);
        std::string ip;
        ushort port;
        int location;
        if (iss >> ip >> port >> location) {
            config.emplace_back(ConfigEntry{ip, port, location, 0, 0});
        }
    }
    return config;
}

long unsigned int getCurrentUTCTime() {
    return static_cast<unsigned long>(std::time(nullptr));
}


void sendMessage(const std::string &ip, short port, const std::string &message) {
    int sockfd = socket(AF_INET, SOCK_DGRAM, 0);
    if (sockfd < 0) {
        std::cerr << "Error opening socket" << std::endl;
        return;
    }

    struct sockaddr_in servaddr;
    memset(&servaddr, 0, sizeof(servaddr));
    servaddr.sin_family = AF_INET;
    servaddr.sin_port = htons(port);
    inet_pton(AF_INET, ip.c_str(), &servaddr.sin_addr);

    sendto(sockfd, message.c_str(), message.length(), 0, (const struct sockaddr *) &servaddr, sizeof(servaddr));
    close(sockfd);
}


int setupListenSocket(int listenPort) {
    int sockfd = socket(AF_INET, SOCK_DGRAM, 0);
    if (sockfd < 0) {
        std::cerr << "Error opening socket" << std::endl;
        exit(EXIT_FAILURE);
    }

    struct sockaddr_in servaddr;
    memset(&servaddr, 0, sizeof(servaddr));
    servaddr.sin_family = AF_INET;
    servaddr.sin_addr.s_addr = htonl(INADDR_ANY);
    servaddr.sin_port = htons(listenPort);

    if (bind(sockfd, (struct sockaddr *) &servaddr, sizeof(servaddr)) < 0) {
        std::cerr << "Bind failed" << std::endl;
        exit(EXIT_FAILURE);
    }

    return sockfd;
}

void sendMessageToEntry(const ConfigEntry &entry, const BasePacket &packet) {
    std::string formattedMessage = packet.serialize();
    sendMessage(entry.ip, entry.port, formattedMessage);
}

std::pair<std::unique_ptr<Packet>, bool> processIncomingMessage(std::unordered_map<std::basic_string<char>, std::basic_string<char>> map) {
    try {
        // Attempt to deserialize as a Message
        Message message = Message::deserialize(map);
        // Successfully deserialized a Message, return it with true
        // C++11 does not have std::make_unique, so use std::unique_ptr constructor directly
        return std::make_pair(std::unique_ptr<Message>(new Message(std::move(message))), true);
    } catch (const std::exception &e) {
        // Log or handle Message deserialization failure
        // std::cerr << "Message deserialization failed: " << e.what() << std::endl;
    }

    try {
        // If Message deserialization fails, try as an Acknowledgement
        Acknowledgement ack = Acknowledgement::deserialize(map);
        // Successfully deserialized an Acknowledgement, return it with false
        return std::make_pair(std::unique_ptr<Acknowledgement>(new Acknowledgement(std::move(ack))), false);
    } catch (const std::exception &e) {
        // Log or handle Acknowledgement deserialization failure
        // std::cerr << "Acknowledgement deserialization failed: " << e.what() << std::endl;
    }
    std::cerr << "Deserialization failed: " << std::endl;


    // If both deserializations fail, return nullptr with false
    return std::make_pair(std::unique_ptr<Packet>(nullptr), false);
}
