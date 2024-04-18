#include <algorithm>
#include <cmath>

#include "Utility.h"
#include "Message.h"
#include <iostream>
#include <limits>
#include <sys/select.h>
#include <unistd.h> // For STDIN_FILENO, close
#include <netinet/in.h> // For sockaddr_in, socklen_t
#include <functional>

#define BUFFER_SIZE 1024
#define PROTOCOL_VERSION 7
#define PROTOCOL_TTL 4
#define ROWS 4
#define COLS 4


void doForEachIf(
        const std::vector<ConfigEntry> &configEntries,
        const BasePacket &packet,
        int listenPort,
        const std::function<bool(const ConfigEntry &, int listenPort)> &condition,
        const std::function<void(const ConfigEntry &, const BasePacket &)> &action
) {
    for (const auto &entry: configEntries) {
        if (condition(entry, listenPort)) {
            action(entry, packet);
        }
    }
}

int load_and_validate_config(const std::string &configFilePath, std::vector<ConfigEntry> &config, ushort &listenPort,
                             ushort &sendtoPort, int &location) {
    config = readConfig(configFilePath);

    sendtoPort = 0;
    location = -1;
    std::cout << "Config:" << std::endl;
    bool isValid = false;
    for (const auto &entry: config) {
        if (entry.port == listenPort) {
            location = entry.location;
            isValid = true;
            std::cout << entry.port << " " << entry.port << " " << entry.location << " *" << std::endl;
        } else std::cout << entry.port << " " << entry.port << " " << entry.location << std::endl;
    }
    if (isValid) return 0;
    std::cerr << "Port not in config" << std::endl;
    return 1;

}

void print_config(std::vector<ConfigEntry> &config, ushort &listenPort) {
    std::cout << "Config:" << std::endl;
    for (const auto &entry: config) {
        if (entry.port == listenPort) {
            std::cout << entry.port << " " << entry.port << " " << entry.location << " *" << std::endl;
        } else std::cout << entry.port << " " << entry.port << " " << entry.location << std::endl;
    }
}

int find_distance(const int rows, const int cols, const int location, const Packet &packet) {
    const int r0 = (location - 1) / rows;
    const int c0 = (location - 1) % cols;

    const int r = (packet.location - 1) / rows;
    const int c = (packet.location - 1) % cols;

    return static_cast<int>(std::sqrt((r - r0) * (r - r0) + (c - c0) * (c - c0)));
}

void forward_packet(std::vector<ConfigEntry> &configEntries, ushort listenPort, int location, Packet &packet) {
    packet.location = location;
    packet.TTL -= 1;
    packet.sendPath.push_back(listenPort);
    for (const auto &entry: configEntries) {
        bool shouldForward = true;
        for (auto port: packet.sendPath) {
            if (port == entry.port) {
                shouldForward = false;
                break;
            }
        }
        if (entry.port != listenPort && shouldForward) {
            std::cout << "Forwarding to " << entry.port << ": " << packet.serialize() << std::endl;
            sendMessageToEntry(entry, packet);
        }
    }
}

int main(int argc, char *argv[]) {
    if (argc != 2) {
        std::cerr << "Usage: " << argv[0] << " <ListenPort>" << std::endl;
        return 1;
    }
    std::string configFilePath = "Samiul Alam-config.file";


    int rows = ROWS, cols = COLS;
    std::cout << "Grid is " << rows << " x " << cols << std::endl;

    std::vector<ConfigEntry> configEntries;
    ushort listenPort = static_cast<ushort>(std::stoi(argv[1]));
    ushort sendtoPort;
    int location;
    if (load_and_validate_config(configFilePath, configEntries, listenPort, sendtoPort, location) != 0) return 1;

    int sockfd = setupListenSocket(listenPort);
    fd_set readfds;
    int max_sd = sockfd;


    std::string msg;
    auto listenerConfig = std::find_if(configEntries.begin(), configEntries.end(), [listenPort](const ConfigEntry &e) {
        return e.port == listenPort;
    });


    while (true) {
        FD_ZERO(&readfds);
        FD_SET(STDIN_FILENO, &readfds);
        FD_SET(sockfd, &readfds);

        if (select(max_sd + 1, &readfds, nullptr, nullptr, nullptr) < 0) {
            std::cerr << "Select error." << std::endl;
            break; // Exit the loop in case of select error
        }
        if (FD_ISSET(sockfd, &readfds)) {
            char buffer[BUFFER_SIZE];
            sockaddr_in cliaddr{};
            socklen_t len = sizeof(cliaddr);


            long n = recvfrom(sockfd, buffer, BUFFER_SIZE, 0, reinterpret_cast<struct sockaddr *>(&cliaddr), &len);
            if (n > 0) {
                buffer[n] = '\0'; // Null-terminate the received data
                std::string receivedMessage(buffer);
                // Got a message
                std::pair<std::unique_ptr<Packet>, bool> result;
                std::unordered_map<std::basic_string<char>, std::basic_string<char>> map;
                try {
                    map = parseMessage(receivedMessage);

                } catch (const std::exception &e) {
                    std::cout << "Parsing Error" << std::endl;
                    continue;
                }
                if (map.find("move") != map.end()) {
                    if (listenPort == std::stoi(map["toPort"])) {
                        location = std::stoi(map["move"]);
                    }
                    for (auto &entry: configEntries) {
                        if (entry.port == std::stoi(map["toPort"])) {
                            entry.location = std::stoi(map["move"]);
                        }

                    }
                    std::cout << "Moving: " << receivedMessage << std::endl;
                    print_config(configEntries, listenPort);
                    continue;
                }
                result = processIncomingMessage(map);
                Packet &packet = *result.first;
                bool isMsg = result.second;
                if (result.first == nullptr) continue; // Check if message is valid
                bool isDuplicate = false;
                int forwardDistance = find_distance(rows, cols, location, packet);
                if (forwardDistance > 2 || packet.TTL < 0) continue; // Check if message is within range and TTL

                if (listenerConfig->port == packet.toPort) {
                    // Check if message meant for current location
                    if (isMsg) {
                        // Check if it is a duplicate message
                        auto p = packet.fromPort;
                        auto senderConfig = std::find_if(configEntries.begin(), configEntries.end(),
                                                         [p](const ConfigEntry &e) {
                                                             return e.port == p;
                                                         });
                        isDuplicate = packet.seqNumber <= senderConfig->sendSeq;
                        if (isDuplicate)
                            std::cout << "Duplicate Message: " << senderConfig->sendSeq << ", " << packet.serialize()
                                      << std::endl;
                            // std::cout << "Duplicate Message" << std::endl;
                        else senderConfig->sendSeq = packet.seqNumber;
                    } else {
                        // Check if it is a duplicate acknowledgement
                        auto p = packet.fromPort;
                        auto senderConfig = std::find_if(configEntries.begin(), configEntries.end(),
                                                         [p](const ConfigEntry &e) {
                                                             return e.port == p;
                                                         });
                        isDuplicate = packet.seqNumber <= senderConfig->recvSeq;
                        if (isDuplicate)
                            std::cout << "Duplicate Acknowledgement: " << senderConfig->recvSeq << ", "
                                      << packet.serialize() << std::endl;
                            // std::cout << "Duplicate Acknowledgement" << std::endl;
                        else senderConfig->recvSeq = packet.seqNumber;
                    }
                }
                if (isDuplicate) continue; // If duplicate then continue
                if (listenPort == packet.toPort) {
                    // Is message/ack meant for current drone
                    std::cout << packet.serialize() << std::endl;
                    if (isMsg) {
                        // If it is a meesage
                        listenerConfig->recvSeq = packet.seqNumber;
                        for (const auto &entry: configEntries) {
                            if (entry.port != listenPort) {
                                auto ack = Acknowledgement(
                                        getCurrentUTCTime(),
                                        "ACK",
                                        packet.fromPort,
                                        packet.toPort,
                                        PROTOCOL_TTL,
                                        PROTOCOL_VERSION, 0, location,
                                        listenerConfig->recvSeq, std::vector<ushort>{listenPort});
                                int backward_distance = find_distance(rows, cols, entry.location, ack);
                                if (backward_distance <= 2) {
                                    // Send ACK if within range
                                    sendMessageToEntry(entry, ack);
                                }
                            }
                        }
                    }
                    continue; // Do not do forwarding step
                }
                // Not a message or acknowledgement meant for this port then forward
                if (isMsg) {
                    auto p = packet.fromPort;
                    auto senderConfig = std::find_if(configEntries.begin(), configEntries.end(),
                                                     [p](const ConfigEntry &e) {
                                                         return e.port == p;
                                                     });
                    if (senderConfig->sendSeq > packet.seqNumber) {
                        isDuplicate = true;
                        std::cout << "Duplicate Message during forwarding: " << senderConfig->sendSeq << ", "
                                  << packet.serialize() << std::endl;
                        // std::cout << "Duplicate Message during forwarding" << std::endl;
                    } else senderConfig->sendSeq = packet.seqNumber;
                } else {
                    auto p = packet.fromPort;
                    auto acknowledgerConfig = std::find_if(configEntries.begin(), configEntries.end(),
                                                           [p](const ConfigEntry &e) {
                                                               return e.port == p;
                                                           });
                    if (acknowledgerConfig->recvSeq >= packet.seqNumber) {
                        isDuplicate = true;
                        std::cout << "Duplicate Acknowledgement during forwarding: " << acknowledgerConfig->recvSeq
                                  << ", " << packet.serialize() << std::endl;
                        // std::cout << "Duplicate Acknowledgement during forwarding" << std::endl;
                    } else acknowledgerConfig->recvSeq = packet.seqNumber;
                }
                if (isDuplicate) continue;
                forward_packet(configEntries, listenPort, location, packet);
            }
        }
        if (FD_ISSET(STDIN_FILENO, &readfds)) {
            while (true) {
                std::cout << "Enter the port number to send to: ";
                std::cin >> sendtoPort;
                auto sendToConfig = std::find_if(configEntries.begin(), configEntries.end(),
                                                 [sendtoPort](const ConfigEntry &e) {
                                                     return e.port == sendtoPort;
                                                 });
                if (sendToConfig != configEntries.end()) break;
                std::cout << "Port not in config" << std::endl;
            }
            std::cin.ignore(std::numeric_limits<std::streamsize>::max(), '\n'); // Clear the input buffer

            std::cout << "Enter message: ";
            std::getline(std::cin, msg); // Read the entire message
            if (msg.empty()) {
                std::cout << "Moving drone..." << std::endl;
                std::cout << "Enter New Location:" << std::endl;
                int newloc;
                std::cin >> newloc;
                std::cin.ignore(std::numeric_limits<std::streamsize>::max(), '\n'); // Clear the input buffer

                for (auto &entry: configEntries) {
                    if (entry.port == sendtoPort) {
                        entry.sendSeq += 1;
                    }
                    auto moveCmd = MoveCommand(getCurrentUTCTime(), sendtoPort, listenPort, PROTOCOL_TTL, PROTOCOL_VERSION,
                                               0, location, entry.sendSeq, newloc);
                    if (entry.port != listenPort) {
                        sendMessageToEntry(entry, moveCmd);
                    }

                    if (entry.port == sendtoPort) {
                        entry.location = newloc;
                        std::cout << "Moving: " << moveCmd.serialize() << std::endl;
                    }
                }
                print_config(configEntries, listenPort);
                continue;
            }
            listenerConfig->sendSeq += 1;
            for (const auto &entry: configEntries) {
                if (entry.port != listenPort) {
                    Message message = Message(getCurrentUTCTime(), msg, sendtoPort, listenPort, PROTOCOL_TTL,
                                              PROTOCOL_VERSION, 0, location, listenerConfig->sendSeq,
                                              std::vector<ushort>{listenPort});
                    sendMessageToEntry(entry, message);
                }
            }
        }
    }

    close(sockfd); // Cleanup
    return 0;
}
