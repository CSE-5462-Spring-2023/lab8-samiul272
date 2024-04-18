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
    ushort toPort;
    ushort fromPort;
    short TTL;
    int location;
    int seqNumber;

    BasePacket(unsigned long time, ushort toPort, ushort fromPort, short TTL,
               short version, short flags, int location, int seqNumber);
    virtual ~BasePacket();

    virtual std::unordered_map<std::string, std::string> toMap() const;
    virtual void fromMap(const std::unordered_map<std::string, std::string>& packetMap);
    virtual std::string serialize() const;
    static BasePacket* deserialize(const std::string& packetStr);
};

class Packet : public BasePacket {

public:
    std::vector<ushort> sendPath;

    Packet(unsigned long time, ushort toPort, ushort fromPort, short TTL,
           short version, short flags, int location, int seqNumber, std::vector<ushort> sendPath);
    ~Packet() override;

    static void parseSendPath(const std::string& sendPathStr, std::vector<ushort>& path);
    std::unordered_map<std::string, std::string> toMap() const override;
    void fromMap(const std::unordered_map<std::string, std::string>& packetMap) override;
    std::string serialize() const override;
    static Packet* deserialize(const std::string& packetStr); // Adjust implementation accordingly
    static Packet deserialize(std::unordered_map<std::basic_string<char>, std::basic_string<char>> &map);

};

class Message : public Packet {
public:
    std::string msg;

    Message(unsigned long time, std::string msg, ushort toPort, ushort fromPort, short TTL, short version, short flags,
            int location, int seqNumber, std::vector<ushort> sendPath);

    std::unordered_map<std::string, std::string> toMap() const override;

    void fromMap(const std::unordered_map<std::string, std::string> &packetMap) override;

    std::string serialize() const override;

    static Message deserialize(const std::string &messageStr);

    static Message deserialize(std::unordered_map<std::basic_string<char>, std::basic_string<char>> &map);
};

class Acknowledgement : public Packet {
public:
    std::string type;

    Acknowledgement(unsigned long time, std::string type, ushort toPort, ushort fromPort, short TTL,
                    short version, short flags, int location, int seqNumber, std::vector<ushort> sendPath);

    std::unordered_map<std::string, std::string> toMap() const override;

    void fromMap(const std::unordered_map<std::string, std::string> &packetMap) override;

    std::string serialize() const override;

    static Acknowledgement deserialize(const std::string &ackStr);

    static Acknowledgement deserialize(std::unordered_map<std::basic_string<char>, std::basic_string<char>> &map);

};

class MoveCommand : public BasePacket {
protected:
    int move;

public:
    MoveCommand(unsigned long time, ushort toPort, ushort fromPort, short TTL,
                short version, short flags, int location,
                int seqNumber, int move);

    ~MoveCommand() override;

    std::unordered_map<std::string, std::string> toMap() const override;
    void fromMap(const std::unordered_map<std::string, std::string>& packetMap) override;
    std::string serialize() const override;

    // Assuming deserialize is a static member function, it might need to be
    // defined outside the class if it creates a new instance of MoveCommand.
    static MoveCommand deserialize(const std::string& packetStr);
    static MoveCommand deserialize(std::unordered_map<std::basic_string<char>, std::basic_string<char>> &map);

};


#endif // MESSAGE_H
