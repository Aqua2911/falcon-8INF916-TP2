//
// Created by grave on 2025-02-11.
//
#include "Stream.h"

Stream::Stream(uint64_t client, uint32_t id, bool reliable) : clientID(client), streamID(id), isReliable(reliable) {}

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

uint32_t Stream::GetID() const { return streamID; }
bool Stream::IsReliable() const { return isReliable; }