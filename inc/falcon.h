#pragma once

#include <condition_variable>
#include <functional>
#include <memory>
#include <string>
#include <span>
#include <thread>
//#include <_bsd_types.h>
#include <map>

#include "ClientInfo.h"
#include "Stream.h"

void hello();

#ifdef WIN32
    using SocketType = unsigned int;
#else
    using SocketType = int;
#endif


enum MessageType
{
    CONNECT,
    CONNECTACK,
    MESSAGE,
    HEARTBEAT,
    NEWSTREAM,
    STREAMACK,
    STREAMDATA,
};

class Falcon {
public:
    //Server
    static std::unique_ptr<Falcon> Listen(const std::string& endpoint, uint16_t port);
    void OnClientConnected(const std::string &from, uint16_t clientPort);

    void Listen(uint16_t port);

    //Client
    static std::unique_ptr<Falcon> Connect(const std::string& serverIp, uint16_t port);
    void ConnectTo(const std::string& ip, uint16_t port);
    void OnConnectionEvent(uint64_t newClientID); //Here the bool represent the success of the connection

    void StartListening(uint16_t port);
    void StopListening();


    //void StartHeartbeat();
    void StartCleanUp();
    void StopCleanUp();

    void DisconnectToServer();  // client API
    void OnClientDisconnected(uint64_t clientID); //Server API
    void OnDisconnect();    // client API

    std::shared_ptr<Stream> CreateStream(uint64_t client, bool reliable); //Server API
    std::shared_ptr<Stream> CreateStream(bool reliable); //Client API
    void CloseStream(const Stream& stream); //Server API
    void NotifyNewStream(const Stream& stream);
    void OnNewStreamNotificationReceived(uint64_t senderID, bool isReliable);

    Falcon();
    ~Falcon();
    Falcon(const Falcon&) = default;
    Falcon& operator=(const Falcon&) = default;
    Falcon(Falcon&&) = default;
    Falcon& operator=(Falcon&&) = default;

    int SendTo(const std::string& to, uint16_t port, std::span<const char> message);
    int ReceiveFrom(std::string& from, std::span<char, 65535> message);

    static MessageType GetMessageType(const std::string& messageType);
    static std::vector<std::string> ParseMessage(const std::span<char, 65535> message, const std::string& delimiter);
    void FindStreamMessage(uint32_t streamID, uint32_t messageID, std::vector<char> &messageBuffer);    // used for tests

    // <clientID, clientInfo>
    std::map<uint64_t, ClientInfo*> clients;

    uint64_t ClientID;

private:
    int SendToInternal(const std::string& to, uint16_t port, std::span<const char> message);
    int ReceiveFromInternal(std::string& from, std::span<char, 65535> message);

    void UpdateLastHeartbeat(const uint64_t ClientID);
    void CleanConnections();


    //void HeartbeatLoop();
    //void HeartBeat();
    void CleanUpLoop();

    long long ElapsedTime(ClientInfo* c);
    uint64_t GetSenderID(std::string &from);


    //std::thread heartbeatThread;
    std::thread CleanConnectionsThread;
    //std::atomic<bool> running{true};
    SocketType m_socket;

    // threads
    std::thread updateThread;
    std::thread listenThread;
    std::atomic<bool> running = false;

    // streams
    std::unordered_map<uint32_t, std::shared_ptr<Stream>> activeStreams;    // <streamID, Stream>
    uint32_t nextStreamID = 1;

    std::vector<std::pair<uint64_t, std::vector<char>>> messagesToBeSent;    // <receiverID, message> pairs
    std::vector<std::pair<std::string, std::vector<char>>> messagesReceived;   // <from, message> pairs

    void Update();
    void UpdateLoop();
    void DecodeMessage(const std::string& from, std::vector<char> message);
    void AddMessageToSendBuffer(uint64_t receiverID, std::vector<char> message);

};

/*
inline void Falcon::HeartbeatLoop()
{
    while (true) {
        HeartBeat();
        std::this_thread::sleep_for(std::chrono::milliseconds(500));
    }
}
*/