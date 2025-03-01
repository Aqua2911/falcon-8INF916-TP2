#include <string>
#include <array>
#include <iostream>
#include <span>
//#include <unistd.h>


#include <catch2/catch_test_macros.hpp>

#include "falcon.h"
#include "Stream.h"

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
    auto client = Falcon::Connect("127.0.0.1", 5556);
    auto server = Falcon::Listen("127.0.0.1", 5555);

    REQUIRE(client != nullptr);
    REQUIRE(server != nullptr);

    server->StartListening(5556);


    client->ConnectTo("127.0.0.1", 5555);

    std::this_thread::sleep_for(std::chrono::seconds(3));
    REQUIRE(!client->clients.empty());
    REQUIRE(!server->clients.empty());
    REQUIRE(client->ClientID == 1);
}

TEST_CASE("Can Notify Disconnection", "[falcon]") {
    auto client = Falcon::Connect("127.0.0.1", 5556);
    auto server = Falcon::Listen("127.0.0.1", 5555);

    REQUIRE(client != nullptr);
    REQUIRE(server != nullptr);

    server->StartListening(5556);


    client->ConnectTo("127.0.0.1", 5555);

    std::this_thread::sleep_for(std::chrono::seconds(2));
    REQUIRE(!client->clients.empty());
    REQUIRE(!server->clients.empty());
    REQUIRE(client->ClientID == 1);

    std::this_thread::sleep_for(std::chrono::seconds(6));

    REQUIRE(server->clients.empty());
}

TEST_CASE("Stream can send and receive Ack", "[stream]") {
    auto client = Falcon::Connect("127.0.0.1", 5556);   //client
    auto server = Falcon::Listen("127.0.0.1", 5555);  //server

    //Connect client to server
    client->ConnectTo("127.0.0.1", 5555);

    std::string from_ip;
    from_ip.resize(255);
    std::array<char, 65535> buffer{};

    //Receive the client connection
    server->ReceiveFrom(from_ip, buffer);
    client->ReceiveFrom(from_ip, buffer);


    // Server-side creation of reliable stream
    auto stream = server->CreateStream(client->ClientID, true);
    server->NotifyNewStream(*stream);

    //Receive new Stream Info
    client->ReceiveFrom(from_ip, buffer);

    //Receive Ack
    server->ReceiveFrom(from_ip, buffer);
}


TEST_CASE("Stream can send and receive data", "[stream]") {
    auto client = Falcon::Connect("127.0.0.1", 5556);
    auto server = Falcon::Listen("127.0.0.1", 5555);

    REQUIRE(client != nullptr);
    REQUIRE(server != nullptr);

    server->StartListening(5556);


    client->ConnectTo("127.0.0.1", 5555);

    std::this_thread::sleep_for(std::chrono::seconds(2));


    // Server-side creation of reliable stream
    auto stream = server->CreateStream(client->ClientID, true);
    server->NotifyNewStream(*stream);

    //Send Message Through stream
    stream->SendData("Hello World");



    std::this_thread::sleep_for(std::chrono::seconds(5));
    auto serverStream = server->activeStreams.find(1);
    auto clientStream = client->activeStreams.find(1);
    REQUIRE(serverStream != nullptr);
    REQUIRE(clientStream != nullptr);

    REQUIRE(!serverStream->second->messageMap.empty());
    REQUIRE(!clientStream->second->messageMap.empty());
}

TEST_CASE("Can Listen for messages on other thread", "[falcon]") {
    auto client = Falcon::Connect("127.0.0.1", 5556);
    auto server = Falcon::Listen("127.0.0.1", 5555);

    REQUIRE(client != nullptr);
    REQUIRE(server != nullptr);

    server->StartListening(5556);


    client->ConnectTo("127.0.0.1", 5555);


    std::string message = "Hello World!";
    //std::span data(message.data(), message.size());

    // wait a bit for everything to be processed in other threads before ending test
    //std::this_thread::sleep_for(std::chrono::seconds(10));


    std::cout << "Test finished.\n";

    //client->DisconnectToServer();
    //while (client->ClientID != 1)
    //{
    //    // stall this thread while others process messages
    //}
    std::this_thread::sleep_for(std::chrono::seconds(5));

    //REQUIRE(client->ClientID == 1);
    client->StopListening();
    server->StopListening();
    REQUIRE(client->ClientID == 1);
}

TEST_CASE("Can Create stream and send data multi-threaded", "[falcon]") {
    auto client = Falcon::Connect("127.0.0.1", 5556);
    auto server = Falcon::Listen("127.0.0.1", 5555);

    REQUIRE(client != nullptr);
    REQUIRE(server != nullptr);

    server->StartListening(5556);

    client->ConnectTo("127.0.0.1", 5555);

    //client->DisconnectToServer();
    while (client->ClientID != 1)
    {
        // stall this thread while others process messages
    }

    auto stream = server->CreateStream(client->ClientID, false);
    server->NotifyNewStream(*stream);

    std::this_thread::sleep_for(std::chrono::milliseconds(20));

    char rawData[1] = { 0x42 };
    std::span<const char> data(rawData);
    stream->SendData(data);

    std::this_thread::sleep_for(std::chrono::milliseconds(20));

    //REQUIRE(client->ClientID == 1);
    client->StopListening();
    server->StopListening();
    REQUIRE(client->ClientID == 1);
    std::vector<char> messageBuffer;
    client->FindStreamMessage(1, 1, messageBuffer);
    REQUIRE(messageBuffer[0] == rawData[0]);    // message has succefully been received and stored for processing by client
}