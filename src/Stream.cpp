//
// Created by grave on 2025-02-11.
//
#include "Stream.h"
#include "falcon.h"


Stream::Stream(Falcon& from, uint64_t client, uint32_t id, bool reliable) : streamFrom(from), clientID(client), msgID(id), isReliable(reliable) {
    streamTo = streamFrom.clients.find(clientID)->second;
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

uint32_t Stream::GetID() const { return msgID; }
bool Stream::IsReliable() const { return isReliable; }

Falcon &Stream::GetStreamFrom() const {
    return streamFrom;
}

ClientInfo *Stream::GetStreamTo() const {
    return streamTo;
}
