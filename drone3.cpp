#include <algorithm>
#include <cmath>

#include "Utility.h"
#include "Message.h"
#include <iostream>
#include <limits>
#include <sys/select.h>
#include <unistd.h>
#include <netinet/in.h>
#include <functional>
#include <mutex>

#define BUFFER_SIZE 1024
#define PROTOCOL_VERSION 8
#define PROTOCOL_TTL 5
#define ROWS 4
#define COLS 4

std::vector<Message> message_store;
std::mutex message_store_mutex;



void resend_messages(std::vector<ConfigEntry> &config_entries) {
    std::lock_guard<std::mutex> lock(message_store_mutex);
    for (auto it = message_store.begin(); it != message_store.end();) {
        if (it->ttl <= 0) {
            it = message_store.erase(it);
            std::cout << "Out of time message erased from queue." << std::endl;
        } else {
            // Assume that send_message_to_entry is capable of checking the network status or other conditions
            --(it->ttl);
            std::cout << it->serialize() << std::endl;
            for (auto &entry: config_entries) {
                if (entry.port != it->from_port) {
                    send_message_to_entry(entry, *it);
                }
            }
            ++it;
        }
    }
}

void remove_message_if_acked(const Packet& acknowledgement_packet) {
    std::lock_guard<std::mutex> lock(message_store_mutex);
    auto it = std::remove_if(message_store.begin(), message_store.end(), [&acknowledgement_packet](const Packet& stored_packet) {
        return stored_packet.sequence_number == acknowledgement_packet.sequence_number &&
               stored_packet.from_port == acknowledgement_packet.to_port && // Note: 'to_port' of ack should match 'from_port' of the message
               stored_packet.to_port == acknowledgement_packet.from_port;   // and vice versa
    });
    if (it != message_store.end()) {
        message_store.erase(it, message_store.end());
        std::cout << "ACK received, message removed: Seq #" << acknowledgement_packet.sequence_number << std::endl;
    }
}

int load_and_validate_config(const std::string &config_file_path, std::vector<ConfigEntry> &config, ushort &listen_port,
                             ushort &sendtoPort, int &location) {
    config = read_config(config_file_path);

    sendtoPort = 0;
    location = -1;
    std::cout << "Config:" << std::endl;
    bool is_valid = false;
    for (const auto &entry: config) {
        if (entry.port == listen_port) {
            location = entry.location;
            is_valid = true;
            std::cout << entry.port << " " << entry.port << " " << entry.location << " *" << std::endl;
        } else std::cout << entry.port << " " << entry.port << " " << entry.location << std::endl;
    }
    if (is_valid) return 0;
    std::cerr << "Port not in config" << std::endl;
    return 1;

}

void print_config(std::vector<ConfigEntry> &config, ushort &listen_port) {
    std::cout << "Config:" << std::endl;
    for (const auto &entry: config) {
        if (entry.port == listen_port) {
            std::cout << entry.ip << " " << entry.port << " " << entry.location << " *" << std::endl;
        } else std::cout << entry.ip << " " << entry.port << " " << entry.location << std::endl;
    }
}

int find_distance(const int rows, const int cols, const int location, const Packet &packet) {
    const int r0 = (location - 1) / rows;
    const int c0 = (location - 1) % cols;

    const int r = (packet.location - 1) / rows;
    const int c = (packet.location - 1) % cols;

    return static_cast<int>(std::sqrt((r - r0) * (r - r0) + (c - c0) * (c - c0)));
}

void forward_packet(std::vector<ConfigEntry> &config_entries, ushort listen_port, int location, Packet &packet) {
    packet.location = location;
    packet.ttl -= 1;
    packet.send_path.push_back(listen_port);
    for (const auto &entry: config_entries) {
        bool should_forward = true;
        for (auto port: packet.send_path) {
            if (port == entry.port) {
                should_forward = false;
                break;
            }
        }
        if (entry.port != listen_port && should_forward) {
            std::cout << "Forwarding to " << entry.port << ": " << packet.serialize() << std::endl;
            send_message_to_entry(entry, packet);
        }
    }
}

int main(int argc, char *argv[]) {
    if (argc != 2) {
        std::cerr << "Usage: " << argv[0] << " <Listen Port>" << std::endl;
        return 1;
    }
    std::string config_file_path = "Samiul Alam-config.file";


    int rows = ROWS, cols = COLS;
    std::cout << "Grid is " << rows << " x " << cols << std::endl;

    std::vector<ConfigEntry> config_entries;
    ushort listen_port = static_cast<ushort>(std::stoi(argv[1]));
    ushort send_to_port;
    int location;
    if (load_and_validate_config(config_file_path, config_entries, listen_port, send_to_port, location) != 0) return 1;

    int socket_file_descriptor = setup_listen_socket(listen_port);
    fd_set read_file_descriptor;
    int max_sd = socket_file_descriptor;


    std::string message_content;
    auto listener_config = std::find_if(config_entries.begin(), config_entries.end(), [listen_port](const ConfigEntry &e) {
        return e.port == listen_port;
    });

    struct timeval timeout{};

    while (true) {
        FD_ZERO(&read_file_descriptor);
        FD_SET(STDIN_FILENO, &read_file_descriptor);
        FD_SET(socket_file_descriptor, &read_file_descriptor);
        timeout.tv_sec = 20;
        timeout.tv_usec = 0;
        int rc = select(max_sd + 1, &read_file_descriptor, nullptr, nullptr, &timeout);

        if (rc < 0) {
            std::cerr << "Select error." << std::endl;
            break; // Exit the loop in case of select error
        }

        if (rc == 0 && !message_store.empty()) {
            resend_messages(config_entries);
            std::cout << "Timeout! Resending messages.\n";
            continue;
        }

        if (FD_ISSET(socket_file_descriptor, &read_file_descriptor)) {
            char buffer[BUFFER_SIZE];
            sockaddr_in client_address{};
            socklen_t len = sizeof(client_address);


            long n = recvfrom(socket_file_descriptor, buffer, BUFFER_SIZE, 0, reinterpret_cast<struct sockaddr *>(&client_address), &len);
            if (n > 0) {
                buffer[n] = '\0'; // Null-terminate the received data
                std::string received_message(buffer);
                // Got a message
                std::pair<std::unique_ptr<Packet>, bool> result;
                std::unordered_map<std::basic_string<char>, std::basic_string<char>> map;
                try {
                    map = parse_message(received_message);

                } catch (const std::exception &e) {
                    std::cout << "Parsing Error" << std::endl;
                    continue;
                }
                if (map.find("move") != map.end()) {
                    if (listen_port == std::stoi(map["to_port"])) {
                        location = std::stoi(map["move"]);
                    }
                    for (auto &entry: config_entries) {
                        if (entry.port == std::stoi(map["to_port"])) {
                            entry.location = std::stoi(map["move"]);
                        }

                    }
                    std::cout << "Moving: " << received_message << std::endl;
                    print_config(config_entries, listen_port);
                    resend_messages(config_entries);
                    continue;
                }
                result = process_incoming_message(map);
                Packet &packet = *result.first;
                bool isMsg = result.second;
                if (result.first == nullptr) continue; // Check if message is valid
                bool is_duplicate = false;
                int forward_distance = find_distance(rows, cols, location, packet);
                if (forward_distance > 2 || packet.ttl < 0) continue; // Check if message is within range and ttl

                if (listener_config->port == packet.to_port) {
                    // Check if message meant for current location
                    if (isMsg) {
                        // Check if it is a duplicate message
                        auto p = packet.from_port;
                        auto sender_config = std::find_if(config_entries.begin(), config_entries.end(),
                                                          [p](const ConfigEntry &e) {
                                                             return e.port == p;
                                                         });
                        is_duplicate = packet.sequence_number <= sender_config->send_sequence;
                        if (is_duplicate)
                            std::cout << "Duplicate Message: " << sender_config->send_sequence << ", " << packet.serialize()
                                      << std::endl;
                        else sender_config->send_sequence = packet.sequence_number;
                    } else {
                        // Check if it is a duplicate acknowledgement
                        auto p = packet.from_port;
                        auto sender_config = std::find_if(config_entries.begin(), config_entries.end(),
                                                          [p](const ConfigEntry &e) {
                                                             return e.port == p;
                                                         });
                        is_duplicate = packet.sequence_number <= sender_config->receive_sequence;
                        if (is_duplicate)
                            std::cout << "Duplicate Acknowledgement: " << sender_config->receive_sequence << ", "
                                      << packet.serialize() << std::endl;
                        else sender_config->receive_sequence = packet.sequence_number;
                    }
                }
                if (is_duplicate) continue; // If duplicate then continue
                if (listen_port == packet.to_port) {
                    // Is message/ack meant for current drone
                    std::cout << packet.serialize() << std::endl;
                    if (isMsg) { // If it is a message
                        listener_config->receive_sequence = packet.sequence_number;
                        for (const auto &entry: config_entries) {
                            if (entry.port != listen_port) {
                                auto ack = Acknowledgement(
                                        get_current_UTC_time(),
                                        "ACK",
                                        packet.from_port,
                                        packet.to_port,
                                        PROTOCOL_TTL,
                                        PROTOCOL_VERSION, 0, location,
                                        listener_config->receive_sequence, std::vector<ushort>{listen_port});
                                int backward_distance = find_distance(rows, cols, entry.location, ack);
                                if (backward_distance <= 2) { // Send ACK if within range
                                    send_message_to_entry(entry, ack);
                                }
                            }
                        }
                    } else {
                        remove_message_if_acked(packet);
                        std::cout << "Acknowledged message removed from store." << std::endl;
                    }
                    continue; // Do not do forwarding step
                }
                forward_packet(config_entries, listen_port, location, packet);
            }
        }
        if (FD_ISSET(STDIN_FILENO, &read_file_descriptor)) {
            while (true) {
                std::cout << "Enter the port number to send to: ";
                std::cin >> send_to_port;
                auto send_to_config = std::find_if(config_entries.begin(), config_entries.end(),
                                                   [send_to_port](const ConfigEntry &e) {
                                                     return e.port == send_to_port;
                                                 });
                if (send_to_config != config_entries.end()) {
                    break;
                }
                std::cout << "Port not in config" << std::endl;
            }
            std::cin.ignore(std::numeric_limits<std::streamsize>::max(), '\n'); // Clear the input buffer

            std::cout << "Enter message: ";
            std::getline(std::cin, message_content); // Read the entire message
            if (message_content.empty()) {
                std::cout << "Moving drone..." << std::endl;
                std::cout << "Enter New Location:" << std::endl;
                int new_location;
                std::cin >> new_location;
                std::cin.ignore(std::numeric_limits<std::streamsize>::max(), '\n'); // Clear the input buffer

                for (auto &entry: config_entries) {
                    if (entry.port == send_to_port) {
                        entry.send_sequence += 1;
                    }
                    auto move_command = MoveCommand(get_current_UTC_time(), send_to_port, listen_port, PROTOCOL_TTL,
                                                    PROTOCOL_VERSION,
                                                    0, location, entry.send_sequence, new_location);
                    if (entry.port != listen_port) {
                        send_message_to_entry(entry, move_command);
                    }

                    if (entry.port == send_to_port) {
                        entry.location = new_location;
                        std::cout << "Moving: " << move_command.serialize() << std::endl;
                    }
                }
                print_config(config_entries, listen_port);
                continue;
            }
            listener_config->send_sequence += 1;
            for (const auto &entry: config_entries) {
                if (entry.port != listen_port) {
                    Message message = Message(get_current_UTC_time(), message_content, send_to_port, listen_port, PROTOCOL_TTL,
                                              PROTOCOL_VERSION, 0, location, listener_config->send_sequence,
                                              std::vector<ushort>{listen_port});
                    send_message_to_entry(entry, message);
                    message_store.push_back(message);

                }
            }
        }
    }

    close(socket_file_descriptor); // Cleanup
    return 0;
}
