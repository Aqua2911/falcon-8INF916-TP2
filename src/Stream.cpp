//
// Created by grave on 2025-02-11.
//
#include "Stream.h"
#include "falcon.h"


Stream::Stream(Falcon& from, uint64_t client, uint32_t id, bool reliable) : streamFrom(from), clientID(client), streamID(id), isReliable(reliable) {
    streamTo = streamFrom.clients.find(clientID)->second;
    msgID = 0;
}

void Stream::SendData(std::span<const char> Data)
{
    if(isReliable) {
        pendingResends.push_back(std::span<const char>(Data.begin(), Data.end()));
    }



}

void Stream::OnDataReceived(std::span<const char> Data)
{
    if (isReliable) {

    }
}

void Stream::SendAck()
{
    std::string ack = "STREAMACK";
    ack.append("|");
    ack.append(std::to_string(streamFrom.ClientID));
    ack.append("|");
    ack.append(std::to_string(streamID));
    ack.append("|");
    if (!receiveBuffer.empty())
    {
        auto last = receiveBuffer.back();
        std::string lastString(last.data(), last.size());
        ack.append(lastString);
    }
    else
    {
        ack.append("0"); // TODO: find better solution
    }
    streamFrom.SendTo(streamTo->ip, streamTo->port, ack);
}

void Stream::OnAckReceived(uint64_t senderID, std::span<const char> lastPacketReceived)
{
    // TODO:  if satisfactory ... ?
}

uint32_t Stream::GetID() const { return msgID; }
bool Stream::IsReliable() const { return isReliable; }

Falcon &Stream::GetStreamFrom() const {
    return streamFrom;
}

ClientInfo *Stream::GetStreamTo() const {
    return streamTo;
}
