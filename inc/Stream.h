//
// Created by grave on 2025-02-11.
//
#pragma once

#include <span>
#include <cstdint>
#include <vector>
#include <map>

// forward declaration
class Falcon;

class ClientInfo;

class Stream {
public:
    Stream(Falcon& from, uint64_t clientID, uint32_t id, bool reliable);

    void SendData(std::span<const char> Data);
    void OnDataReceived(std::span<const char> Data);

    void SendAck(uint32_t messageID);
    void OnAckReceived(uint32_t lastMessageReceivedID);

    uint32_t GetStreamID() const;
    Falcon& GetStreamFrom() const;
    ClientInfo* GetStreamTo() const;
    bool IsReliable() const;

    std::map<uint32_t, std::span<const char>> messageMap;    // <messageID, message>
    //std::map<uint32_t, std::span<const char>> receiveBuffer;
    std::vector<uint32_t> notYetAcknowledged;
private:
    Falcon& streamFrom;
    ClientInfo* streamTo;

    uint64_t clientID;  // id of streamTo client
    uint32_t streamID;
    uint32_t lastMessageSentID; // TODO: maybe map msgIDs with pendingResends or receiveBuffer or something
    bool isReliable;
};
