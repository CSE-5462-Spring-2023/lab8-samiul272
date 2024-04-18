//
// Created by alam.140 on 2/11/24.
//

#ifndef CONFIG_ENTRY_H
#define CONFIG_ENTRY_H

#include <string>
#include <utility>

struct ConfigEntry {
    std::string ip;
    ushort port;
    int location;
    mutable int send_sequence;
    mutable int receive_sequence;


    ConfigEntry(std::string ip, ushort port, int location, int send_seq = 0, int receive_seq = 0)
            : ip(std::move(ip)), port(port), location(location), send_sequence(send_seq),
              receive_sequence(receive_seq) {
    }
};

#endif // CONFIG_ENTRY_H
