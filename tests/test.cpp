#include <string>
#include <array>
#include <iostream>
#include <span>
#include <unistd.h>
#include <winsock2.h>

#include <catch2/catch_test_macros.hpp>

#include "falcon.h"
#include "spdlog/spdlog.h"

TEST_CASE("Can Listen", "[falcon]") {
    auto receiver = Falcon::Listen("127.0.0.1", 5555);
    REQUIRE(receiver != nullptr);
}

TEST_CASE( "Can Connect", "[falcon]" ) {
    auto sender = Falcon::Connect("127.0.0.1", 5556);
    REQUIRE(sender != nullptr);
}

TEST_CASE( "Can Send To", "[falcon]" ) {
    auto sender = Falcon::Connect("127.0.0.1", 5556);
    auto receiver = Falcon::Listen("127.0.0.1", 5555);
    std::string message = "Hello World!";
    std::span data(message.data(), message.size());
    int bytes_sent = sender->SendTo("127.0.0.1", 5555, data);
    REQUIRE(bytes_sent == message.size());
}

TEST_CASE("Can Receive From", "[falcon]") {
    auto sender = Falcon::Connect("127.0.0.1", 5556);
    auto receiver = Falcon::Listen("127.0.0.1", 5555);
    std::string message = "Hello World!";
    std::span data(message.data(), message.size());
    int bytes_sent = sender->SendTo("127.0.0.1", 5555, data);

    REQUIRE(bytes_sent == message.size());
    std::string from_ip;
    from_ip.resize(255);
    std::array<char, 65535> buffer;
    int byte_received = receiver->ReceiveFrom(from_ip, buffer);

    REQUIRE(byte_received == message.size());
    REQUIRE(std::equal(buffer.begin(),
        buffer.begin() + byte_received,
        message.begin(),
        message.end()));
    REQUIRE(from_ip == "127.0.0.1:5556");
}

TEST_CASE("Can Connect To", "[falcon]") {
    auto sender = Falcon::Connect("127.0.0.1", 5556);
    auto receiver = Falcon::Listen("127.0.0.1", 5555);
    sender->ConnectTo("127.0.0.1", 5555);

    std::string from_ip;
    from_ip.resize(255);
    std::array<char, 65535> buffer;
    int byte_received_server = receiver->ReceiveFrom(from_ip, buffer);

    int byte_received_client = sender->ReceiveFrom(from_ip, buffer);

    std::cout << "Here" << std::endl;
}

TEST_CASE("Can Notify Disconnection", "[falcon]") {
    auto sender = Falcon::Connect("127.0.0.1", 5556);
    auto receiver = Falcon::Listen("127.0.0.1", 5555);
    sender->ConnectTo("127.0.0.1", 5555);

    std::string from_ip;
    from_ip.resize(255);
    std::array<char, 65535> buffer;
    int byte_received_server = receiver->ReceiveFrom(from_ip, buffer);

    int byte_received_client = sender->ReceiveFrom(from_ip, buffer);

    sender->SendTo("127.0.0.1", 5555, "Hello World!");

    // Wait for 2 seconds
    std::this_thread::sleep_for(std::chrono::seconds(2));

    std::chrono::steady_clock::time_point now = std::chrono::steady_clock::now();
    auto duration = now - receiver->clients.at(0)->lastHeartbeat;
    auto seconds = std::chrono::duration_cast<std::chrono::seconds>(duration).count();
    if (seconds > 1) {
        std::cout << "Here" << std::endl;
    }
}