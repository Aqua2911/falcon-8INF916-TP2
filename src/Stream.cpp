﻿//
// Created by grave on 2025-02-11.
//
#include "Stream.h"

#include <winsock2.h>

#include "falcon.h"
#include "fmt/format.h"


Stream::Stream(uint64_t senderID, uint64_t receiverID, uint32_t id, bool reliable) : senderID(senderID),receiverID(receiverID), streamID(id), isReliable(reliable) {
    lastMessageSentID = 0;
}

void Stream::SendData(std::span<const char> Data)
{
    lastMessageSentID++;
    if(isReliable) {
        std::string dataSTR(Data.data(), Data.size());
        //pendingResends.push_back(std::span<const char>(Data.begin(), Data.end()));
        messageMap.insert({lastMessageSentID, dataSTR});
        notYetAcknowledged.push_back(lastMessageSentID);
        StartNotYetAcknowledgedLoop();
    }

    std::string streamData = "STREAMDATA";
    streamData.append("|");
    streamData.append(std::to_string(senderID));
    streamData.append("|");
    streamData.append(std::to_string(streamID));
    streamData.append("|");
    streamData.append(std::to_string(lastMessageSentID));
    streamData.append("|");
    // Convert to string
    std::string dataSTR(Data.data(), Data.size());
    streamData.append(dataSTR);

    // add data to buffer to be sent by falcon
    dataToBeSent.push_back(streamData);
}

void Stream::OnDataReceived(uint32_t messageID, std::span<const char> Data)
{
    std::string dataSTR(Data.data(), Data.size());
    messageMap.insert({messageID, dataSTR});   // receiver stores data for future processing

    if (isReliable) {
        SendAck(messageID);
    }
}

void Stream::SendAck(uint32_t messageID)
{
    std::string ack = "STREAMACK";
    ack.append("|");
    ack.append(std::to_string(senderID));
    ack.append("|");
    ack.append(std::to_string(streamID));
    ack.append("|");

    ack.append(std::to_string(messageID));
    ack.append("|");

    // add message to data buffer
    dataToBeSent.push_back(ack);
}

void Stream::OnAckReceived(uint32_t lastMessageReceivedID)
{
    if (lastMessageReceivedID == 0) // TODO find better solution
    {
        return;
    }

    auto it = std::ranges::find(notYetAcknowledged, lastMessageReceivedID);
    if (it != notYetAcknowledged.end()) {
        notYetAcknowledged.erase(it);
        if (notYetAcknowledged.empty())
        {
            StopNotYetAcknowledgedLoop();
        }
    }
    // else do nothing
}

void Stream::StartNotYetAcknowledgedLoop()
{
    NotYetAcknowledgedThread = std::thread(&Stream::NotYetAcknowledgedLoop, this);
}

void Stream::StopNotYetAcknowledgedLoop() {
    running.store(false);
    if (NotYetAcknowledgedThread.joinable() && NotYetAcknowledgedThread.get_id() != std::this_thread::get_id()) {
        NotYetAcknowledgedThread.join();
    }
}

void Stream::CheckNotYetAcknowledged()
{
    for (auto msgID: notYetAcknowledged)
    {
        auto msg = messageMap.find(msgID);
        dataToBeSent.push_back(msg->second);
    }
}

void Stream::NotYetAcknowledgedLoop() {
    while (running)
    {
        CheckNotYetAcknowledged();
        std::this_thread::sleep_for(std::chrono::milliseconds(1000));
    }
}

void Stream::WriteDataToBuffer(std::vector<std::pair<uint64_t, std::span<const char>>> &buffer)
{
    for (const auto &data : dataToBeSent)
    {
        buffer.push_back({receiverID, data});
    }
    // clear data buffer once data is copied
    dataToBeSent.clear();
    hasDataToBeSent = !dataToBeSent.empty();
}


uint32_t Stream::GetStreamID() const { return streamID; }
bool Stream::IsReliable() const { return isReliable; }

bool Stream::HasDataToBeSent() const {
    return hasDataToBeSent;
}

uint64_t Stream::GetReceiverID() const {
    return receiverID;
}
