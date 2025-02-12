//
// Created by grave on 2025-02-11.
//
#pragma once

#include <span>

class Stream {
  public:
  void SendData(std::span<const char> Data);
  void OnDataReceived(std::span<const char> Data);
};
