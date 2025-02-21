//
// Created by grave on 2025-02-11.
//
#pragma once

#include <span>
#include <cstdint>
#include <vector>
#include <map>
#include <thread>

class Stream {
public:
    Stream(uint64_t senderID, uint64_t receiverID, uint32_t id, bool reliable);

    void SendData(std::span<const char> Data);
    void OnDataReceived(uint32_t messageID, std::span<const char> Data);

    void SendAck(uint32_t messageID);
    void OnAckReceived(uint32_t lastMessageReceivedID);

    uint32_t GetStreamID() const;
    bool IsReliable() const;
    bool HasDataToBeSent() const;
    uint64_t GetReceiverID() const;

    std::map<uint32_t, std::string> messageMap;    // <messageID, message>
    //std::map<uint32_t, std::span<const char>> receiveBuffer;
    std::vector<uint32_t> notYetAcknowledged;

    void WriteDataToBuffer(std::vector<std::pair<uint64_t, std::span<const char>>> &buffer);

private:
    const uint64_t senderID;
    const uint64_t receiverID;

    const uint32_t streamID;
    uint32_t lastMessageSentID;
    bool isReliable;

    void StartNotYetAcknowledgedLoop();
    void StopNotYetAcknowledgedLoop();
    void CheckNotYetAcknowledged();

    void NotYetAcknowledgedLoop();

    std::thread NotYetAcknowledgedThread;
    std::atomic<bool> running{true};


    std::vector<std::span<const char>> dataToBeSent;
    bool hasDataToBeSent;
};
