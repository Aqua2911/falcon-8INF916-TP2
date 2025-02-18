//
// Created by grave on 2025-02-12.
//
#include <chrono>
#include <memory>

class ClientInfo{
public:
  //uint64_t clientID;
  std::string ip;
  uint16_t port;
  std::chrono::steady_clock::time_point lastHeartbeat;
};