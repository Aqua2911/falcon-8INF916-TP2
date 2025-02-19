//
// Created by grave on 2025-02-11.
//
#include "Stream.h"
#include "falcon.h"

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


Stream::Stream(Falcon& from, uint64_t client, uint32_t id, bool reliable) : streamFrom(from), clientID(client), streamID(id), isReliable(reliable) {
    streamTo = streamFrom.clients.find(clientID)->second;
    lastMessageSentID = 0;
}

void Stream::SendData(std::span<const char> Data)
{
    if(isReliable) {
        //pendingResends.push_back(std::span<const char>(Data.begin(), Data.end()));
        messageMap.insert({lastMessageSentID, std::span<const char>(Data.begin(), Data.end())});
        notYetAcknowledged.push_back(lastMessageSentID);
    }

    streamFrom.SendTo(streamTo->ip, streamTo->port, Data);
}

void Stream::OnDataReceived(std::span<const char> Data)
{
    StreamData* parsedData = StreamData::ParseData(Data, "|");
    messageMap.insert({parsedData->messageID, Data});   // receiver stores data for future processing

    if (isReliable) {
        SendAck(parsedData->messageID);
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
    streamFrom.SendTo(streamTo->ip, streamTo->port, ack);
}

void Stream::OnAckReceived(uint32_t lastMessageReceivedID)
{
    if (lastMessageReceivedID == 0) // TODO find better solution
    {
        // this is a NEWSTREAM ack
    }


    auto it = std::ranges::find(notYetAcknowledged, lastMessageReceivedID);
    if (it != notYetAcknowledged.end()) {
        notYetAcknowledged.erase(it);
    }
    // else do nothing
}

uint32_t Stream::GetStreamID() const { return streamID; }
bool Stream::IsReliable() const { return isReliable; }

Falcon &Stream::GetStreamFrom() const {
    return streamFrom;
}

ClientInfo *Stream::GetStreamTo() const {
    return streamTo;
}
