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

    if (!clients.empty())
    {
        clients.clear(); // empty clients map so it only has one server at a time
    }
    auto* newServer = new ClientInfo;
    newServer->ip = "127.0.0.1";
    newServer->port = 5555;
    newServer->lastHeartbeat = std::chrono::steady_clock::now();
    uint64_t serverID = 0;
    clients.insert({serverID, newServer});
}

void Falcon::OnClientConnected(const std::string &from, uint16_t clientPort)
{
    uint64_t newClientID;
    if (!clients.empty())
    {
        //newClientID = ++clients.back()->clientID;
        uint64_t lastClientID = std::prev(clients.end())->first;
        newClientID = ++lastClientID;
    }
    else
    {
        newClientID = 1;
        StartCleanUp();
    }
    auto* newClient = new ClientInfo;
    //newClient->clientID = newClientID;
    newClient->ip = from;
    newClient->port = clientPort;
    newClient->lastHeartbeat = std::chrono::steady_clock::now();
    //clients.push_back(newClient);
    clients.insert({newClientID, newClient});

    std::string message = "ACKNOWLEDGE|";
    message.append(std::to_string(newClientID));

    //Send an Acknowledgement to the Client
    SendToInternal(from, clientPort, message);

}

/*
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
*/

void Falcon::OnClientDisconnected(uint64_t clientID) { // not sure if this works i just took the previous version and changed some stuff
    auto it = clients.find(clientID);
    if (it != clients.end()) {
        clients.erase(it);
    }
    delete it->second;
    std::cout << "Client Disconnected" << std::endl;
    if (clients.empty())
        StopCleanUp();
}

void Falcon::UpdateLastHeartbeat(const uint64_t clientID)
{
    //for(auto c : clients) {
    //    if (c->clientID == ClientID)
    //    {
    //        c->lastHeartbeat = std::chrono::steady_clock::now();
    //        return;
    //    }
    //}
    ClientInfo* matchedClient = clients.find(clientID)->second;
    matchedClient->lastHeartbeat = std::chrono::steady_clock::now();
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
        if (ElapsedTime(c.second) > 1)
            OnClientDisconnected(c.first);
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
        if (c.second->port == stoi(fromPort))   // c.second is the clientInfo
        {
            //return c.second->clientID;
            return c.first; // c.first is the client id
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
    auto stream = std::make_unique<Stream>(*this, client, nextStreamID++, reliable);

    activeStreams[stream->GetID()] = std::move(stream);
    return std::move(activeStreams[stream->GetID()]);
}

std::unique_ptr<Stream> Falcon::CreateStream(bool reliable) {   // Client API
    return CreateStream(0, reliable);   // 0 is server id
}

void Falcon::CloseStream(const Stream &stream) {
    // remove from map and call destructor of stream
    activeStreams.erase(stream.GetID());
}

void Falcon::NotifyNewStream(const Stream& stream)
{
    // NB: uin64_t clientID could be passed as argument,
    // but we're passing a stream to make sure it actually
    // exists on the sender-side before the notification is sent

    // get notification recipient info
    ClientInfo* notificationRecipient = stream.GetStreamTo();
    bool isStreamReliable = stream.IsReliable();

    std::string message = "NEWSTREAM|";
    message.append(std::to_string(ClientID));
    message.append("|");
    message.append(std::to_string(isStreamReliable));
    SendTo(notificationRecipient->ip, notificationRecipient->port, message);
}

void Falcon::OnNewStreamNotificationReceived(uint64_t senderID, bool isReliable)
{
    // TODO:
    // get client id -> done (passed as argument)
    // if client : create stream locally
    // if server : create stream locally
    // send ack

    auto stream = CreateStream(senderID, isReliable); // CreateStreams already adds new stream to activeStreams map

    stream.SendAck();


}

// TODO : use this method where needed (ReceiveFrom(), ...)
std::vector<std::string> Falcon::ParseMessage(const std::span<char, 65535> message, const std::string& delimiter)
{
    // source : stackoverflow & chatgpt

    std::vector<std::string> tokens;
    std::string_view s(message.data(), message.size()); // Avoids extra allocation

    size_t pos = 0;
    while ((pos = s.find(delimiter)) != std::string::npos) {
        tokens.emplace_back(s.substr(0, pos)); // Store token
        s.remove_prefix(pos + delimiter.length()); // Move past delimiter
    }

    tokens.emplace_back(s); // Add remaining part
    return tokens;
}

