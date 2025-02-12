#pragma once

#include <functional>
#include <memory>
#include <string>
#include <span>

void hello();

#ifdef WIN32
    using SocketType = unsigned int;
#else
    using SocketType = int;
#endif

class Falcon {
public:
    static std::unique_ptr<Falcon> Listen(const std::string& endpoint, uint16_t port);
    static std::unique_ptr<Falcon> Connect(const std::string& serverIp, uint16_t port);
    void OnClientConnected(std::function<void, uint64_t> handler);
    void ConnectTo(const std::string& ip, uin16_t port);
    void OnConnectionEvent(std::function<void, bool, uint64_t> handler); //Here the bool represent the success of the connection
    void OnClientDisconnected(std::function<void, uint64_t> handler); //Server API
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

private:
    int SendToInternal(const std::string& to, uint16_t port, std::span<const char> message);
    int ReceiveFromInternal(std::string& from, std::span<char, 65535> message);

    SocketType m_socket;
};
