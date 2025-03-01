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
    char buffer[1] = {};
    uint8_t type = 0x00;
    memcpy(buffer, &type, sizeof(uint8_t));
    std::span<char, 1> message(buffer);

    SendTo(ip, port, message);
    //SendTo(ip, port, "CONNECT|");
    std::cout << "Send Connect" << std::endl;

    StartListening(port);

    // TODO : if no CONNECTACK has been received for a certain amount of time, stop listening to prevent infinite loop ?
}

void Falcon::OnConnectionEvent(uint64_t newClientID)    // client API
{
    ClientID = newClientID;

    // add server to "clients" map
    if (!clients.empty())
    {
        clients.clear();// empty clients map so it only has one server at a time
    }
    std::shared_ptr<ClientInfo> newServer = std::make_shared<ClientInfo>();;
    newServer->ip = "127.0.0.1";
    newServer->port = 5555;
    newServer->lastHeartbeat = std::chrono::steady_clock::now();
    uint64_t serverID = 0;
    clients.insert({serverID, newServer});
}


void Falcon::DisconnectToServer()
{
    const uint8_t type = 0x05;
    const uint16_t serverID = 0;
    const size_t bufferSize = sizeof(type) + sizeof(serverID);
    std::vector<char> message(bufferSize);

    memcpy(message.data(), &type, sizeof(type));
    memcpy(message.data() + sizeof(type), &serverID, sizeof(serverID));
    //std::string message = "DISCONNECT|";
    //message.append(std::to_string(ClientID));

    //std::vector<char> data(message.begin(), message.end());
    AddMessageToSendBuffer(serverID, std::move(message));
}

void Falcon::OnDisconnect()
{
    StopListening();
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
        StartCleanUp();
    }
    std::shared_ptr<ClientInfo> newClient = std::make_shared<ClientInfo>();;
    //newClient->clientID = newClientID;
    newClient->ip = from;
    newClient->port = clientPort;
    newClient->lastHeartbeat = std::chrono::steady_clock::now();
    //clients.push_back(newClient);
    clients.insert({newClientID, newClient});

    //std::string message = "CONNECTACK|";
    //message.append(std::to_string(newClientID));

    uint8_t type = 0x01;
    const size_t bufferSize = sizeof(type) + sizeof(newClientID);
    std::vector<char> message(bufferSize);
    //char buffer[sizeof(type) + sizeof(newClientID)] = {};
    memcpy(message.data(), &type, sizeof(type));
    memcpy(message.data() + sizeof(type), &newClientID, sizeof(newClientID));

    //Send an Acknowledgement to the Client
    //SendToInternal(from, clientPort, message);
    //std::vector<char> data(buffer, sizeof(buffer));
    AddMessageToSendBuffer(newClientID, std::move(message));
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

        it->second = nullptr;;
        clients.erase(it);

        // client is no longer part of clients list so we need to manually send the DC ACK message
        char buffer[1] = {};
        uint8_t type = 0x06;    // DISCONNECTACK
        memcpy(buffer, &type, sizeof(uint8_t));
        std::span<char, 1> message(buffer);
        SendTo(clientIP, clientPort,message);
    }
    std::cout << "Client Disconnected" << std::endl;
    //if (clients.empty())
        //StopCleanUp();
}

void Falcon::UpdateLastHeartbeat(const uint64_t clientID)
{
    auto matchedClient = clients.find(clientID);
    if (matchedClient != clients.end()) // failsafe
    {
        matchedClient->second->lastHeartbeat = std::chrono::steady_clock::now();
    }
}

void Falcon::StartCleanUp()
{
    if (ClientID ==0) {
        runningCleanUp.store(true);
        CleanConnectionsThread = std::thread(&Falcon::CleanUpLoop, this);
    }
}

//Executed on Thread 2
void Falcon::CleanUpLoop()
{
    while (runningCleanUp) {
        CleanConnections();
        std::this_thread::sleep_for(std::chrono::milliseconds(1000));
    }
    std::cout << "CleanupLoop DONE" << std::endl;
}

//Executed on Thread 2
void Falcon::CleanConnections()
{
    if (!clients.empty())
    {
        for (auto it = clients.begin(); it != clients.end(); )
        {
            if (it->first < 100000)
            {
                if (ElapsedTime(it->second) > 5)
                {
                    OnClientDisconnected(it->first); // Call after erase
                }
                else
                {
                    ++it;
                }
            }
            else
            {
                break;
            }
        }
    }
}

//Executed on Thread 1
void Falcon::StopCleanUp()
{
    runningCleanUp.store(false);
    if (ClientID != 0 || !CleanConnectionsThread.joinable())
    {
        return;
    }
    if (CleanConnectionsThread.joinable() && CleanConnectionsThread.get_id() != std::this_thread::get_id()) {
        CleanConnectionsThread.join();
        std::cout << "Thread Succesfully joined" << std::endl;
    }
}

long long Falcon::ElapsedTime(std::shared_ptr<ClientInfo> c) {
    if (c == nullptr || !c)
        return 0;
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

    //std::string message = "NEWSTREAM|";
    //message.append(std::to_string(ClientID));
    //message.append("|");
    //message.append(std::to_string((int) isStreamReliable));

    uint8_t type = 0x02;
    uint8_t streamInfo = 0x00;
    streamInfo = stream.IsReliable()?
                 (streamInfo | 0x80):   // set first bit to 1
                 (streamInfo & ~0x80);  // set first bit to 0
    const size_t bufferSize = sizeof(type) + sizeof(ClientID) + sizeof(streamInfo);
    std::vector<char> data(bufferSize);

    memcpy(data.data(), &type, sizeof(type));
    memcpy(data.data() + sizeof(type), &ClientID, sizeof(ClientID));
    memcpy(data.data() + sizeof(type) + sizeof(ClientID), &streamInfo, sizeof(streamInfo));
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
        if (stream != nullptr)
        {
            if (stream->HasDataToBeSent())
            {
                stream->WriteDataToBuffer(messagesToBeSent);
            }
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
    std::cout << "Start Update" << std::endl;
    while (running)
    {
        Update();
    }
    std::cout << "End Update" << std::endl;
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
    std::cout << "Listening Threads joined" << std::endl;
}

void Falcon::Listen(uint16_t port)
{
    std::string from_ip;
    from_ip.resize(255);
    std::array<char, 65535> buffer{};

    std::cout << "Start Listening" << std::endl;

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
    //std::span<char, 65535> msg(message.data(), message.size());
    //auto splitMessage = ParseMessage(msg, "|");
    //const std::string& messageType = splitMessage[0];

    uint8_t messageType = *(message.data()); // pointer to begining of data

    //Test if it's a new client trying to connect
    if (messageType == (uint8_t) 0x00) // CONNECT
    {
        size_t pos = from.find(':'); // Find the position of the character
        const std::string fromIP = from.substr(0, pos); // substring up to the delimiter
        const std::string fromPort = from.substr(pos + 1);

        OnClientConnected(fromIP, stoi(fromPort));

        //UpdateLastHeartbeat(GetSenderID(from));
    }

    if (messageType == (uint8_t) 0x01)    // splitMessage : CONNECTACK|clientID
    {
        uint64_t clientID;
        memcpy(&clientID, message.data() + 1, sizeof(uint64_t));
        OnConnectionEvent(clientID);
    }
    if (messageType == (uint8_t) 0x02)  // splitMessage : NEWSTREAM|senderID|isReliable
    {
        uint64_t senderID;
        uint8_t reliability;
        memcpy(&senderID, message.data() + 1, sizeof(senderID));
        memcpy(&reliability, message.data() + sizeof(senderID), sizeof(reliability));

        const bool isReliable = (reliability & 0x80) == 0x80;   // 0x80 : 1000 0000

        OnNewStreamNotificationReceived(senderID, isReliable);
    }
    if (messageType == (uint8_t) 0x03)  // splitMessage : STREAMACK|senderID|streamID|lastMessageReceivedID
    {
        //const uint64_t senderID = std::stoi(splitMessage[1]);
        uint64_t senderID;
        uint32_t streamID;
        uint32_t lastMessageReceivedID;
        memcpy(&senderID, message.data() + 1, sizeof(senderID));
        memcpy(&streamID, message.data() + 1 + sizeof(senderID), sizeof(streamID));
        memcpy(&lastMessageReceivedID, message.data() + 1 + sizeof(senderID) + sizeof(streamID), sizeof(lastMessageReceivedID));

        //const uint32_t streamID = std::stoi(splitMessage[2]);
        //const uint32_t lastMessageReceivedID = std::stoi(splitMessage[3]);

        auto mappedStream = activeStreams.find(streamID);
        if (mappedStream != activeStreams.end())    // failsafe
        {
            mappedStream->second->OnAckReceived(lastMessageReceivedID);
        }
        // else no stream with matching streamID found
    }
    if (messageType == (uint8_t) 0x04) // splitMessage : STREAMDATA|senderID|streamID|messageID|data
    {
        uint64_t senderID;
        uint32_t streamID;
        uint32_t messageID;
        memcpy(&senderID, message.data() + 1, sizeof(senderID));
        memcpy(&streamID, message.data() + 1 + sizeof(senderID), sizeof(streamID));
        memcpy(&messageID, message.data() + 1 + sizeof(senderID) + sizeof(streamID), sizeof(messageID));

        const size_t datasize = sizeof(message) - sizeof(messageType) - sizeof(senderID) - sizeof(streamID) - sizeof(messageID);
        char databuffer[datasize] = {};
        memcpy(databuffer, message.data() + sizeof(messageType) + sizeof(senderID) + sizeof(streamID) + sizeof(messageID), datasize);
        std::span<char, datasize> data(databuffer);

        //const uint64_t senderID = std::stoi(splitMessage[1]);
        //const uint32_t streamID = std::stoi(splitMessage[2]);
        //const uint32_t messageID = std::stoi(splitMessage[3]);
        //const std::string& data = splitMessage[4];

        auto mappedStream = activeStreams.find(streamID);
        if (mappedStream != activeStreams.end())    // failsafe
        {
            mappedStream->second->OnDataReceived(messageID, data);
        }
    }
    if (messageType == (uint8_t) 0x05)  // DISCONNECT
    {
        uint64_t senderID;
        memcpy(&senderID, message.data() + 1, sizeof(uint64_t));

        OnClientDisconnected(senderID);
    }
    if (messageType == (uint8_t) 0x06)  // DISCONNECTACK
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
