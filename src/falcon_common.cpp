#include <iostream>

#include "falcon.h"

int Falcon::SendTo(const std::string &to, uint16_t port, const std::span<const char> message)
{
    return SendToInternal(to, port, message);
}

int Falcon::ReceiveFrom(std::string& from, const std::span<char, 65535> message)
{
    return ReceiveFromInternal(from, message);
}

void Falcon::ConnectTo(const std::string &ip, uint16_t port)
{
    // clients vector doesn't have data yet so we can't add message to message buffer
    SendTo(ip, port, "CONNECT|");

    StartListening(port);

    // TODO : if no CONNECTACK has been received for a certain amount of time, stop listening to prevent infinite loop ?
}

void Falcon::OnConnectionEvent(uint64_t newClientID)    // client API
{
    ClientID = newClientID;

    // add server to "clients" map
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

//void Falcon::OnConnectionEvent(uint64_t clientID, std::function<void(bool, uint64_t)> handler) {
//    this->handler = [handler, clientID]() {handler(true, clientID);};
//}

void Falcon::DisconnectToServer()
{
    const uint16_t serverID = 0;
    std::string message = "DISCONNECT|";
    message.append(std::to_string(ClientID));

    std::vector<char> data(message.begin(), message.end());
    AddMessageToSendBuffer(serverID, std::move(data));
}

void Falcon::OnDisconnect()
{
    //StopListening();
}

void Falcon::OnClientConnected(const std::string &from, uint16_t clientPort)    // server API
{
    uint64_t newClientID;
    if (!clients.empty())
    {
        uint64_t lastClientID = std::prev(clients.end())->first;
        newClientID = ++lastClientID;
    }
    else
    {
        newClientID = 1;
        //StartCleanUp();
    }
    auto* newClient = new ClientInfo;
    //newClient->clientID = newClientID;
    newClient->ip = from;
    newClient->port = clientPort;
    newClient->lastHeartbeat = std::chrono::steady_clock::now();
    //clients.push_back(newClient);
    clients.insert({newClientID, newClient});

    std::string message = "CONNECTACK|";
    message.append(std::to_string(newClientID));

    //Send an Acknowledgement to the Client
    //SendToInternal(from, clientPort, message);
    std::vector<char> data(message.begin(), message.end());
    AddMessageToSendBuffer(newClientID, std::move(data));
}


//Executed on Thread 2

void Falcon::OnClientDisconnected(uint64_t clientID)    // not sure if this works i just took the previous version and changed some stuff
{
    std::string clientIP;
    uint16_t clientPort;
    auto it = clients.find(clientID);
    if (it != clients.end()) {
        clientIP = it->second->ip;
        clientPort = it->second->port;
        clients.erase(it);

        // client is no longer part of clients list so we need to manually send the DC ACK message
        SendTo(clientIP, clientPort,"DISCONNECTACK|");
    }
    //delete it->second;
    std::cout << "Client Disconnected" << std::endl;
    //if (clients.empty())
    //    StopCleanUp();

}

void Falcon::UpdateLastHeartbeat(const uint64_t clientID)
{
    auto matchedClient = clients.find(clientID);
    if (matchedClient != clients.end()) // failsafe
    {
        matchedClient->second->lastHeartbeat = std::chrono::steady_clock::now();
    }
}
/*
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
*/
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
            return c.first; // c.first is the client id
        }
    }
    return 0;
}

MessageType Falcon::GetMessageType(const std::string& messageType) {    // updated version
    // parsing of the message should be done beforehand using Falcon::ParseMessage

    if (messageType == "CONNECT")
    {
        return MessageType::CONNECT;
    }
    if (messageType == "CONNECTACK")
    {
        return MessageType::CONNECTACK;
    }
    if (messageType == "HEARTBEAT")
    {
        return MessageType::HEARTBEAT;
    }
    if (messageType == "NEWSTREAM")
    {
        return MessageType::NEWSTREAM;
    }
    if (messageType == "STREAMACK")
    {
        return MessageType::STREAMACK;
    }
    if (messageType == "STREAMDATA")
    {
        return MessageType::STREAMDATA;
    }

    return MessageType::MESSAGE;
}

std::shared_ptr<Stream> Falcon::CreateStream(uint64_t client, bool reliable) {  // Server API
    // generate unique stream and id

    activeStreams[nextStreamID] = std::make_shared<Stream>(ClientID, client, nextStreamID, reliable);
    nextStreamID++;
    return activeStreams[nextStreamID - 1];
}

std::shared_ptr<Stream> Falcon::CreateStream(bool reliable) {   // Client API
    return CreateStream(0, reliable);   // 0 is server id
}

void Falcon::CloseStream(const Stream &stream) {
    // TODO:    send message to client that stream is closed
    //          maybe wait for confirmation by client that stream has been deleted on their side before deleting it here (so that it can be resent ?)

    // remove from map and call destructor of stream
    activeStreams.erase(stream.GetStreamID());
}

void Falcon::NotifyNewStream(const Stream& stream)
{
    // NB: uin64_t clientID could be passed as argument,
    // but we're passing a stream to make sure it actually
    // exists on the sender-side before the notification is sent

    // get notification recipient info
    uint64_t notificationRecipient = stream.GetReceiverID();
    bool isStreamReliable = stream.IsReliable();

    std::string message = "NEWSTREAM|";
    message.append(std::to_string(ClientID));
    message.append("|");
    message.append(std::to_string((int) isStreamReliable));

    std::vector<char> data(message.begin(), message.end());
    AddMessageToSendBuffer(notificationRecipient, std::move(data));
}

void Falcon::OnNewStreamNotificationReceived(uint64_t senderID, bool isReliable)
{
    auto stream = CreateStream(senderID, isReliable); // CreateStream already adds new stream to activeStreams map

    stream->SendAck(0);
}

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

void Falcon::FindStreamMessage(uint32_t streamID, uint32_t messageID, std::vector<char> &messageBuffer)
{
    auto it = activeStreams.find(streamID);
    if (it == activeStreams.end()) {
        return;
    }
    auto msgIT = it->second->messageMap.find(messageID);
    if (msgIT == it->second->messageMap.end()){
        return;
    }

    messageBuffer = std::move(msgIT->second);
}

void Falcon::AddMessageToSendBuffer(uint64_t receiverID, std::vector<char> message)
{
    messagesToBeSent.emplace_back(receiverID, std::move(message) );
}

void Falcon::Update()
{
    // decode messages in receive queue
    for (const auto &[from, message] : messagesReceived)
    {
        DecodeMessage(from, message);
    }
    messagesReceived.clear();   // clear buffer once all messages have been decoded

    for (const auto &[streamID, stream] : activeStreams)
    {
        if (stream->HasDataToBeSent())
        {
            stream->WriteDataToBuffer(messagesToBeSent);
        }
    }

    // i realize this if statement isn't necessary and the loop would be enough but it makes it easier to understand the code i think
    if (!messagesToBeSent.empty())
    {
        // send every message
        for (const auto &[receiverID, message] : messagesToBeSent)
        {
            // map receiverID with ip and port from clients vector
            auto matchedReceiver = clients.find(receiverID);
            if (matchedReceiver != clients.end()) // failsafe
            {
                SendTo(matchedReceiver->second->ip, matchedReceiver->second->port, message);
            }
        }
        // delete messages from queue
        messagesToBeSent.clear();
    }
}

void Falcon::UpdateLoop()
{
    while (running)
    {
        Update();
    }
}

void Falcon::StartListening(uint16_t port)
{
    running = true;

    updateThread = std::thread(&Falcon::UpdateLoop, this);
    listenThread = std::thread([this, port]() { Listen(port); });
}

void Falcon::StopListening() {
    running = false;

    if (updateThread.joinable() && updateThread.get_id() != std::this_thread::get_id()) {
        updateThread.join();
    }
    if (listenThread.joinable() && listenThread.get_id() != std::this_thread::get_id()) {
        listenThread.join();
    }
}

void Falcon::Listen(uint16_t port)
{
    std::string from_ip;
    from_ip.resize(255);
    std::array<char, 65535> buffer;

    while (running)
    {
        //std::memset(buffer.data(), 0, buffer.size());

        int read_bytes = ReceiveFrom(from_ip, buffer);

        if (read_bytes > 0)
        {
            // a message has been received
            size_t pos = from_ip.find(':'); // Find the position of the character
            const std::string fromIP = from_ip.substr(0, pos); // substring up to the delimiter
            const std::string fromPort = from_ip.substr(pos + 1);
            uint16_t fromPortInt = atoi(fromPort.c_str());

            if (fromPortInt == port)
            {
                //DecodeMessage(from_ip, buffer);

                // store message to be decoded later
                std::vector<char> message(buffer.begin(), buffer.end());
                messagesReceived.push_back( {from_ip, std::move(message)} );
            }
            // message is ignored if not on designated port
        }
        else if (read_bytes == -1)
        {
            // an error occured
            //break;
        }

    }
}

void Falcon::DecodeMessage(const std::string& from, std::vector<char> message)
{
    std::span<char, 65535> msg(message.data(), message.size());
    auto splitMessage = ParseMessage(msg, "|");
    const std::string& messageType = splitMessage[0];

    //Test if it's a new client trying to connect
    if (GetMessageType(messageType) == MessageType::CONNECT) // splitMessage : CONNECT|
    {
        size_t pos = from.find(':'); // Find the position of the character
        const std::string fromIP = from.substr(0, pos); // substring up to the delimiter
        const std::string fromPort = from.substr(pos + 1);

        OnClientConnected(fromIP, stoi(fromPort));
        //OnClientConnected([](uint64_t clientID){
        //    // handler lambda ...
        //
        //});

        //UpdateLastHeartbeat(GetSenderID(from));
    }

    if (GetMessageType(messageType) == MessageType::CONNECTACK)    // splitMessage : CONNECTACK|clientID
    {
        const std::string& clientID = splitMessage[1];
        OnConnectionEvent(stoi(clientID));
    }
    if (GetMessageType(messageType) == MessageType::NEWSTREAM)  // splitMessage : NEWSTREAM|senderID|isReliable
    {
        const uint64_t senderID = std::stoi(splitMessage[1]);
        const bool isReliable = stoi(splitMessage[2]);
        OnNewStreamNotificationReceived(senderID, isReliable);
    }
    if (GetMessageType(messageType) == MessageType::STREAMACK)  // splitMessage : STREAMACK|senderID|streamID|lastMessageReceivedID
    {
        //const uint64_t senderID = std::stoi(splitMessage[1]);
        const uint32_t streamID = std::stoi(splitMessage[2]);
        const uint32_t lastMessageReceivedID = std::stoi(splitMessage[3]);

        auto mappedStream = activeStreams.find(streamID);
        if (mappedStream != activeStreams.end())    // failsafe
        {
            mappedStream->second->OnAckReceived(lastMessageReceivedID);
        }
        // else no stream with matching streamID found
    }
    if (GetMessageType(messageType) == MessageType::STREAMDATA) // splitMessage : STREAMDATA|senderID|streamID|messageID|data
    {
        const uint64_t senderID = std::stoi(splitMessage[1]);
        const uint32_t streamID = std::stoi(splitMessage[2]);
        const uint32_t messageID = std::stoi(splitMessage[3]);
        const std::string data = splitMessage[4];

        auto mappedStream = activeStreams.find(streamID);
        if (mappedStream != activeStreams.end())    // failsafe
        {
            mappedStream->second->OnDataReceived(messageID, data);
        }
    }
    if (messageType == "DISCONNECT")
    {
        const uint64_t senderID = std::stoi(splitMessage[1]);
        OnClientDisconnected(senderID);
    }
    if (messageType == "DISCONNECTACK")
    {
        OnDisconnect();
    }
    // TODO: if (CLOSESTREAM) : delete stream locally and send CLOSESTREAMACK
}

// TODO:
//          - send notification that stream had been closed
// DONE     - create while loop that calls Falcon::Update()
// ??       - figure out how to handle connection (not sure what i meant when i wrote this todo...)
// DONE-ish - use handlers for event management
// DONE     - create while loop for reception of message
// DONE     - figure how when to start loop
// DONE     - make it so stream adds message to queue instead of calling sendto
