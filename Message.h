#ifndef MESSAGE_H
#define MESSAGE_H

#include <string>
#include <unordered_map>
#include <vector>
#include <bits/stl_vector.h>

#include "ConfigEntry.h"

typedef unsigned short ushort;

class BasePacket {
protected:
    unsigned long time;
    short version, flags;

public:
    ushort to_port;
    ushort from_port;
    short ttl;
    int location;
    int sequence_number;

    BasePacket(unsigned long time, ushort to_port, ushort from_port, short ttl,
               short version, short flags, int location, int sequence_number);

    virtual ~BasePacket();

    virtual std::unordered_map<std::string, std::string> to_map() const;

    virtual void from_map(const std::unordered_map<std::string, std::string> &packet_map);

    virtual std::string serialize() const;

    __attribute__((unused)) static BasePacket *deserialize(__attribute__((unused)) const std::string &packet_string);
};

class Packet : public BasePacket {

public:
    std::vector<ushort> send_path;

    Packet(unsigned long time, ushort to_port, ushort from_port, short ttl,
           short version, short flags, int location, int seqNumber, std::vector<ushort> send_path);

    ~Packet() override;

    static void parse_send_path(const std::string &send_path_string, std::vector<ushort> &path);

    std::unordered_map<std::string, std::string> to_map() const override;

    void from_map(const std::unordered_map<std::string, std::string> &packet_map) override;

    std::string serialize() const override;

    __attribute__((unused)) static Packet *deserialize(const std::string &packet_string); // Adjust implementation accordingly
    __attribute__((unused)) static Packet deserialize(std::unordered_map<std::basic_string<char>, std::basic_string<char>> &map);

};

class Message : public Packet {
public:
    std::string msg;

    Message(unsigned long time, std::string msg, ushort toPort, ushort from_port, short ttl, short version, short flags,
            int location, int sequence_number, std::vector<ushort> sendPath);

    std::unordered_map<std::string, std::string> to_map() const override;

    void from_map(const std::unordered_map<std::string, std::string> &packet_map) override;

    std::string serialize() const override;

    __attribute__((unused)) static Message deserialize(const std::string &message_content);

    static Message deserialize(std::unordered_map<std::basic_string<char>, std::basic_string<char>> &map);
};

class Acknowledgement : public Packet {
public:
    std::string type;

    Acknowledgement(unsigned long time, std::string type, ushort to_port, ushort from_port, short ttl,
                    short version, short flags, int location, int sequence_number, std::vector<ushort> send_path);

    std::unordered_map<std::string, std::string> to_map() const override;

    void from_map(const std::unordered_map<std::string, std::string> &packet_map) override;

    std::string serialize() const override;

    __attribute__((unused)) static Acknowledgement deserialize(const std::string &acknowledgement_string);

    static Acknowledgement deserialize(std::unordered_map<std::basic_string<char>, std::basic_string<char>> &map);

};

class MoveCommand : public BasePacket {
protected:
    int move;

public:
    MoveCommand(unsigned long time, ushort to_port, ushort from_port, short ttl,
                short version, short flags, int location,
                int sequence_number, int move);

    ~MoveCommand() override;

    std::unordered_map<std::string, std::string> to_map() const override;

    void from_map(const std::unordered_map<std::string, std::string> &packet_map) override;

    std::string serialize() const override;

    // Assuming deserialize is a static member function, it might need to be
    // defined outside the class if it creates a new instance of MoveCommand.
    __attribute__((unused)) static MoveCommand deserialize(const std::string &packetStr);

    __attribute__((unused)) static MoveCommand deserialize(std::unordered_map<std::basic_string<char>, std::basic_string<char>> &map);

};


#endif // MESSAGE_H
