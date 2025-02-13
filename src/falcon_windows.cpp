#ifndef _WIN32_WINNT
    #define _WIN32_WINNT 0x0600
#elif _WIN32_WINNT < 0x0600
    #undef _WIN32_WINNT
    #define _WIN32_WINNT 0x0600
#endif

#include <winsock2.h>
#include <ws2tcpip.h>

#include <fmt/core.h>

#pragma comment(lib, "Ws2_32.lib")

#include "falcon.h"

struct WinSockInitializer
{
    WinSockInitializer()
    {
        WSADATA wsaData;
        if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0)
        {
            printf("WSAStartup failed with error: %d\n", WSAGetLastError());
        }
    }

    ~WinSockInitializer()
    {
        WSACleanup();
    }
};

std::string IpToString(const sockaddr* sa)
{
    switch(sa->sa_family)
    {
        case AF_INET: {
            char ip[INET_ADDRSTRLEN + 6];
            const char* ret = inet_ntop(AF_INET,
                &reinterpret_cast<const sockaddr_in*>(sa)->sin_addr,
                ip,
                INET_ADDRSTRLEN);
            return fmt::format("{}:{}", ret, ntohs(reinterpret_cast<const sockaddr_in*>(sa)->sin_port));
        }
        case AF_INET6: {
            char ip[INET6_ADDRSTRLEN + 8];
            const char* ret = inet_ntop(AF_INET6,
                &reinterpret_cast<const sockaddr_in6*>(sa)->sin6_addr,
                ip+ 1,
                INET6_ADDRSTRLEN);
            return fmt::format("[{}]:{}", ret, ntohs(reinterpret_cast<const sockaddr_in6*>(sa)->sin6_port));
        }
    }

    return "";
}

sockaddr StringToIp(const std::string& ip, uint16_t port)
{
    sockaddr result {};
    int error = inet_pton(AF_INET, ip.c_str(), &reinterpret_cast<sockaddr_in*>(&result)->sin_addr);
    if (error == 1) {
        result.sa_family = AF_INET;
        reinterpret_cast<sockaddr_in*>(&result)->sin_port = htons(port);
        return result;
    }

    memset(&result, 0, sizeof(result));
    error = inet_pton(AF_INET6, ip.c_str(), &result);
    if (error == 1) {
        result.sa_family = AF_INET6;
        reinterpret_cast<sockaddr_in6*>(&result)->sin6_port = htons(port);
        return result;
    }
    memset(&result, 0, sizeof(result));
    return result;
}


Falcon::Falcon()
{
    static WinSockInitializer winsockInitializer{};
}

Falcon::~Falcon() {
    if(m_socket != INVALID_SOCKET)
    {
        closesocket(m_socket);
    }
}

std::unique_ptr<Falcon> Falcon::Listen(const std::string& endpoint, uint16_t port)
{
    sockaddr local_endpoint = StringToIp(endpoint, port);
    auto falcon = std::make_unique<Falcon>();
    falcon->m_socket = socket(local_endpoint.sa_family,
        SOCK_DGRAM,
        IPPROTO_UDP);
    if (int error = bind(falcon->m_socket, &local_endpoint, sizeof(local_endpoint)); error != 0)
    {
        closesocket(falcon->m_socket);
        return nullptr;
    }

    return falcon;
}

std::unique_ptr<Falcon> Falcon::Connect(const std::string& serverIp, uint16_t port)
{
    sockaddr local_endpoint = StringToIp(serverIp, port);
    auto falcon = std::make_unique<Falcon>();
    falcon->m_socket = socket(local_endpoint.sa_family,
        SOCK_DGRAM,
        IPPROTO_UDP);
    if (int error = bind(falcon->m_socket, &local_endpoint, sizeof(local_endpoint)); error != 0)
    {
        closesocket(falcon->m_socket);
        return nullptr;
    }

    return falcon;
}

void Falcon::ConnectTo(const std::string &ip, uint16_t port)
{
    SendToInternal(ip, port, "CONNECT|");
}

void Falcon::OnConnectionEvent(uint64_t newClientID)
{
    ClientID = newClientID;
}

void Falcon::OnClientConnected(const std::string &from, uint16_t clientPort)
{
    uint64_t newClientID;
    if (!clients.empty())
    {
        newClientID = ++clients.back()->clientID;
    }
    else
    {
        newClientID = 1;
    }
    auto* newClient = new ClientInfo;
    newClient->clientID = newClientID;
    newClient->port = clientPort;
    clients.push_back(newClient);

    std::string message = "ACKNOWLEDGE|";
    message.append(std::to_string(newClientID));

    //Send an Acknowledgement to the Client
    SendToInternal(from, clientPort, message);
}

int Falcon::SendToInternal(const std::string &to, uint16_t port, std::span<const char> message)
{
    const sockaddr destination = StringToIp(to, port);
    int error = sendto(m_socket,
        message.data(),
        message.size(),
        0,
        &destination,
        sizeof(destination));
    return error;
}

int Falcon::ReceiveFromInternal(std::string &from, std::span<char, 65535> message)
{
    struct sockaddr_storage peer_addr;
    socklen_t peer_addr_len = sizeof(struct sockaddr_storage);
    const int read_bytes = recvfrom(m_socket,
        message.data(),
        message.size_bytes(),
        0,
        reinterpret_cast<sockaddr*>(&peer_addr),
        &peer_addr_len);

    from = IpToString(reinterpret_cast<const sockaddr*>(&peer_addr));

    //Server test if it's a new client trying to connect
    if (GetMessageType(message) == MessageType::CONNECT)
    {
        size_t pos = from.find(':'); // Find the position of the character
        if (pos != std::string::npos) {
            const std::string fromIP = from.substr(0, pos); // substring up to the delimiter
            const std::string fromPort = from.substr(pos + 1);

            //Get The Informations of the new Client
            OnClientConnected(fromIP, stoi(fromPort));
        }
    }

    if (GetMessageType(message) == MessageType::ACKNOWLEDGE)
    {
        std::string str(message.data(), message.size());
        size_t pos = str.find('|'); // Find the position of the character
        if (pos != std::string::npos) {
            const std::string ClientID = str.substr(pos + 1);
            OnConnectionEvent(stoi(ClientID));
        }
    }

    return read_bytes;
}

MessageType Falcon::GetMessageType(const std::span<char, 65535> message) {
    // Convert to string
    std::string str(message.data(), message.size());

    size_t pos = str.find('|'); // Find the position of the character
    if (pos != std::string::npos) {
        str = str.substr(0, pos); // substring up to the delimiter
    }

    if (str == "CONNECT")
    {
        return MessageType::CONNECT;
    }
    else if (str == "ACKNOWLEDGE")
    {
        return MessageType::ACKNOWLEDGE;
    }
    else {
        return MessageType::MESSAGE;
    }
}
