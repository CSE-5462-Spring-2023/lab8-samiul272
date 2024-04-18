#ifndef UTILITY_H
#define UTILITY_H

#include <memory>

#include "ConfigEntry.h"
#include <vector>
#include <string>
#include <unordered_map>

#include "Message.h"

std::unordered_map<std::string, std::string> parse_message(const std::string &message);

std::vector<ConfigEntry> read_config(const std::string &file_path);

unsigned long get_current_UTC_time();

int setup_listen_socket(int listen_port);

void send_message_to_entry(const ConfigEntry &entry, const BasePacket &packet);

std::pair<std::unique_ptr<Packet>, bool>
process_incoming_message(std::unordered_map<std::basic_string<char>, std::basic_string<char>> map);

#endif
