//
// Created by alam.140 on 2/11/24.
//

#ifndef CONFIG_ENTRY_H
#define CONFIG_ENTRY_H

#include <string>

struct ConfigEntry {
    std::string ip;
    ushort port;
    int location;
    mutable int sendSeq;
    mutable int recvSeq;


    ConfigEntry(const std::string &ip, ushort port, int location, int sendSeq = 0, int recvSeq = 0)
        : ip(ip), port(port), location(location), sendSeq(sendSeq), recvSeq(recvSeq) {
    }
};

#endif // CONFIG_ENTRY_H
