//
// Created by grave on 2025-02-11.
//
#include "Stream.h"

#include <winsock2.h>

#include "falcon.h"
#include "fmt/format.h"

/*
struct StreamData {
    std::string messageType;
    uint64_t senderID;
    uint32_t streamID;
    uint32_t messageID;
    std::string data;   // TODO : do better

    static StreamData* ParseData(std::span<const char> Data, const std::string& delimiter) {
        std::vector<std::string> tokens;
        std::string_view s(Data.data(), Data.size()); // Avoids extra allocation

        size_t pos = 0;
        while ((pos = s.find(delimiter)) != std::string::npos) {
            tokens.emplace_back(s.substr(0, pos)); // Store token
            s.remove_prefix(pos + delimiter.length()); // Move past delimiter
        }

        tokens.emplace_back(s); // Add remaining part

        auto parsedData = new StreamData;
        parsedData->messageType = tokens[0];

        return parsedData;
    }
};
*/

Stream::Stream(Falcon& from, uint64_t client, uint32_t id, bool reliable) : streamFrom(from), clientID(client), streamID(id), isReliable(reliable) {
    streamTo = streamFrom.clients.find(clientID)->second;
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
    streamData.append(std::to_string(streamFrom.ClientID));
    streamData.append("|");
    streamData.append(std::to_string(streamID));
    streamData.append("|");
    streamData.append(std::to_string(lastMessageSentID));
    streamData.append("|");
    // Convert to string
    std::string dataSTR(Data.data(), Data.size());
    streamData.append(dataSTR);

    streamFrom.SendTo(streamTo->ip, streamTo->port, streamData);
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
    ack.append(std::to_string(streamFrom.ClientID));
    ack.append("|");
    ack.append(std::to_string(streamID));
    ack.append("|");

    ack.append(std::to_string(messageID));
    ack.append("|");
    streamFrom.SendTo(streamTo->ip, streamTo->port, ack);
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

uint32_t Stream::GetStreamID() const { return streamID; }
bool Stream::IsReliable() const { return isReliable; }

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
        streamFrom.SendTo(streamTo->ip, streamTo->port, msg->second);
    }
}

void Stream::NotYetAcknowledgedLoop() {
    while (running)
    {
        CheckNotYetAcknowledged();
        std::this_thread::sleep_for(std::chrono::milliseconds(1000));
    }
}

Falcon &Stream::GetStreamFrom() const {
    return streamFrom;
}

ClientInfo *Stream::GetStreamTo() const {
    return streamTo;
}
