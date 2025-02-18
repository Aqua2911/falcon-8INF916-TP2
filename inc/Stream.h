//
// Created by grave on 2025-02-11.
//
#pragma once

#include <span>
#include <cstdint>
#include <vector>

// forward declaration
class Falcon;

class ClientInfo;

class Stream {
public:
    Stream(Falcon& from, uint64_t clientID, uint32_t id, bool reliable);

    void SendData(std::span<const char> Data);
    void OnDataReceived(std::span<const char> Data);

    void SendAck();
    void OnAckReceived(uint64_t senderID, std::span<const char> lastPacketReceived);

    uint32_t GetID() const;
    Falcon& GetStreamFrom() const;
    ClientInfo* GetStreamTo() const;
    bool IsReliable() const;

    std::vector<std::span<const char>> pendingResends;
    std::vector<std::span<const char>> receiveBuffer;
private:
    Falcon& streamFrom;
    ClientInfo* streamTo;

    uint64_t clientID;  // id of streamTo client
    uint32_t msgID; // streamID
    bool isReliable;

};
