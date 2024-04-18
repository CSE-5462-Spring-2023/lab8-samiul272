#ifndef UTILITY_H
#define UTILITY_H

#include <memory>

#include "ConfigEntry.h"
#include <vector>
#include <string>
#include <unordered_map>

#include "Message.h"

std::unordered_map<std::string, std::string> parseMessage(const std::string &message);

std::vector<ConfigEntry> readConfig(const std::string &filePath);

unsigned long getCurrentUTCTime();

void sendMessage(const std::string &ip, short port, const std::string &message);

int setupListenSocket(int listenPort);

void sendMessageToEntry(const ConfigEntry &entry, const BasePacket &packet);

std::pair<std::unique_ptr<Packet>, bool> processIncomingMessage(std::unordered_map<std::basic_string<char>, std::basic_string<char>> map);
#endif
