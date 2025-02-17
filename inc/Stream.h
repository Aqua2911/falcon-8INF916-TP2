//
// Created by grave on 2025-02-11.
//
#pragma once

#include <span>
#include <cstdint>

class Stream {
public:
    Stream(uint64_t clientID, uint32_t id, bool reliable);

    void SendData(std::span<const char> Data);
    void OnDataReceived(std::span<const char> Data);

    uint32_t GetID() const;
    bool IsReliable() const;

private:
    //auto server;
    //auto client;

    uint64_t clientID;
    uint32_t streamID;
    bool isReliable;
    std::vector<std::span<const char>> pendingResends;
};
