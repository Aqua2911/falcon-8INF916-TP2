#include <iostream>

#include "falcon.h"

int Falcon::SendTo(const std::string &to, uint16_t port, const std::span<const char> message)
{
    return SendToInternal(to, port, message);
}

int Falcon::ReceiveFrom(std::string& from, const std::span<char, 65535> message)
{

    auto read_bytes = ReceiveFromInternal(from, message);

    //Server Only
    if(ClientID == 0)
    {
        size_t pos = from.find(':'); // Find the position of the character
        const std::string fromIP = from.substr(0, pos); // substring up to the delimiter
        const std::string fromPort = from.substr(pos + 1);

        //Test if it's a new client trying to connect
        if (GetMessageType(message) == MessageType::CONNECT)
        {
            //Get The Informations of the new Client
            OnClientConnected(fromIP, stoi(fromPort));
        }
        UpdateLastHeartbeat(GetSenderID(from));
    }
    else
    {
        //Client Only
        if(ClientID != 0)
        {
            if (GetMessageType(message) == MessageType::ACKNOWLEDGE)
            {
                std::string str(message.data(), message.size());
                size_t pos = str.find('|'); // Find the position of the character
                if (pos != std::string::npos)
                {
                    const std::string ClientID = str.substr(pos + 1);
                    OnConnectionEvent(stoi(ClientID));
                }
            }
        }
    }
    return read_bytes;
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
        StartCleanUp();
    }
    auto* newClient = new ClientInfo;
    newClient->clientID = newClientID;
    newClient->port = clientPort;
    newClient->lastHeartbeat = std::chrono::steady_clock::now();
    clients.push_back(newClient);

    std::string message = "ACKNOWLEDGE|";
    message.append(std::to_string(newClientID));

    //Send an Acknowledgement to the Client
    SendToInternal(from, clientPort, message);

}

//Executed on Thread 2
void Falcon::OnClientDisconnected(ClientInfo *c) {
    auto it = std::ranges::find(clients, c);
    if (it != clients.end()) {
        clients.erase(it);
    }
    delete c;
    std::cout << "Client Disconnected" << std::endl;
    if (clients.empty())
        StopCleanUp();
}


void Falcon::UpdateLastHeartbeat(const uint64_t ClientID)
{
    for(auto c : clients) {
        if (c->clientID == ClientID)
        {
            c->lastHeartbeat = std::chrono::steady_clock::now();
            return;
        }
    }
}

void Falcon::StartCleanUp()
{
    if (ClientID ==0) {
        CleanConnectionsThread = std::thread(&Falcon::CleanUpLoop, this);
    }
}

//Executed on Thread 2
void Falcon::CleanUpLoop()
{
    while (running) {
        CleanConnections();
        std::this_thread::sleep_for(std::chrono::milliseconds(1000));
    }
    std::cout << "CleanupLoop DONE" << std::endl;
}

//Executed on Thread 2
void Falcon::CleanConnections()
{
    for (auto c: clients)
    {
        if (ElapsedTime(c) > 1)
            OnClientDisconnected(c);
    }
}

//Executed on Thread 1
void Falcon::StopCleanUp()
{
    running.store(false);
    if (ClientID != 0)
    {
        return;
    }
    if (CleanConnectionsThread.joinable() && CleanConnectionsThread.get_id() != std::this_thread::get_id()) {
        CleanConnectionsThread.join();
        std::cout << "Thread Succesfully joined" << std::endl;
    }
}

long long Falcon::ElapsedTime(ClientInfo* c) {
    std::chrono::steady_clock::time_point now = std::chrono::steady_clock::now();
    auto duration = now - c->lastHeartbeat;
    auto seconds = std::chrono::duration_cast<std::chrono::seconds>(duration).count();
    return seconds;
}

uint64_t Falcon::GetSenderID(std::string &from)
{
    size_t pos = from.find(':'); // Find the position of the character
    const std::string fromPort = from.substr(pos + 1);

    for (auto c: clients) {
        if (c->port == stoi(fromPort))
        {
            return c->clientID;
        }
    }
    return 0;
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
    else if (str == "HEARTBEAT")
    {
        return MessageType::HEARTBEAT;
    }
    else {
        return MessageType::MESSAGE;
    }
}

std::unique_ptr<Stream> Falcon::CreateStream(uint64_t client, bool reliable) {  // Server API
    // generate unique stream and id
    auto stream = std::make_unique<Stream>(client, nextStreamID++, reliable);

    activeStreams[stream->GetID()] = std::move(stream);
    return std::move(activeStreams[stream->GetID()]);
}

std::unique_ptr<Stream> Falcon::CreateStream(bool reliable) {   // Client API
    return CreateStream(0, reliable);   // 0 as placeholder id
}

void Falcon::CloseStream(const Stream &stream) {
    // remove from map and call destructor of stream
    activeStreams.erase(stream.GetID());
}
