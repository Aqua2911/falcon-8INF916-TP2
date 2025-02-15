#pragma once

#include <condition_variable>
#include <functional>
#include <memory>
#include <string>
#include <span>
#include <thread>
#include <_bsd_types.h>

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
    ACKNOWLEDGE,
    MESSAGE,
    HEARTBEAT,
};

class Falcon {
public:
    //Server
    static std::unique_ptr<Falcon> Listen(const std::string& endpoint, uint16_t port);
    void OnClientConnected(const std::string &from, uint16_t clientPort);

    //Client
    static std::unique_ptr<Falcon> Connect(const std::string& serverIp, uint16_t port);
    void ConnectTo(const std::string& ip, uint16_t port);
    void OnConnectionEvent(uint64_t newClientID); //Here the bool represent the success of the connection

    //void StartHeartbeat();
    void StartCleanUp();
    void StopCleanUp();

    void OnClientDisconnected(ClientInfo* c); //Server API
    void OnDisconnect(std::function<void> handler);  //Client API


    std::unique_ptr<Stream> CreateStream(uint64_t client, bool reliable); //Server API
    std::unique_ptr<Stream> CreateStream(bool reliable); //Client API
    void CloseStream(const Stream& stream); //Server API

    Falcon();
    ~Falcon();
    Falcon(const Falcon&) = default;
    Falcon& operator=(const Falcon&) = default;
    Falcon(Falcon&&) = default;
    Falcon& operator=(Falcon&&) = default;

    int SendTo(const std::string& to, uint16_t port, std::span<const char> message);
    int ReceiveFrom(std::string& from, std::span<char, 65535> message);

    static MessageType GetMessageType(const std::span<char, 65535> message);
    std::pmr::vector<ClientInfo*> clients;
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
    std::atomic<bool> running{true};
    uint64_t ClientID;
    SocketType m_socket;
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