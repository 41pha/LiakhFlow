#include <iostream>
#include "LF_Connection.h"

using namespace net;

enum class CustomMsgTypes : uint32_t
{
    ServerAccept,
    ServerDeny,
    ServerPing,
    MessageAll,
    ServerMessage,
};

template <typename T>
class Client
{
public:
    Client() {}

    virtual  ~Client() {}

    bool Connect(const std::string& host, const uint16_t port)
    {
        try
        {
            asio::ip::tcp::resolver resolver(m_context);
            asio::ip::tcp::resolver::results_type endpoints = resolver.resolve(host, std::to_string(port));

            // Create connection
            m_connection = std::make_unique<Connection<T>>(Connection<T>::Owner::client, m_context, asio::ip::tcp::socket(m_context), m_qMessagesIn);

            // Tell the connection object to connect to server
            m_connection->ConnectToServer(endpoints);

            // Start Context Thread
            thrContext = std::thread([this]() { m_context.run(); });
        }
        catch (const std::exception& e)
        {
            std::cerr << "Client Exception: " << e.what() << "\n";
            return false;
        }
        return true;
    }

    // Check if client is actually connected to a server
    bool IsConnected()
    {
        if (m_connection)
            return m_connection->IsConnected();
        else
            return false;
    }

    // Send message to server
    void Send(const message<T>& msg)
    {
        if (IsConnected())
            m_connection->Send(msg);
    }

    // Retrieve queue of messages from server
    tsqueue<owned_message<T>>& Incoming()
    {
        return m_qMessagesIn;
    }

protected:
    // asio context handles the data transfer...
    asio::io_context m_context;
    // ...but needs a thread of its own to execute its work commands
    std::thread thrContext;

    // The client has a single instance of a "connection" object, which handles data transfer
    std::unique_ptr<Connection<T>> m_connection;
    
private:
    // This is the thread safe queue of incoming messages from server
    tsqueue<owned_message<T>> m_qMessagesIn;
};

int main()
{
    Client<CustomMsgTypes> c;
    c.Connect("ip", 52000);
    c.IsConnected();

    while (c.IsConnected())
    {
        std::string input;
        std::cin >> input;

        message<CustomMsgTypes> msg;
        msg << input;

        c.Send(msg);

        if (!c.Incoming().empty())
        {
            auto msg = c.Incoming().pop_front().msg;

            switch (msg.header.id)
            {
            case CustomMsgTypes::ServerAccept:
            {
                // Server has responded to a ping request				
                std::cout << "Server Accepted Connection\n";
            }
            break;

            case CustomMsgTypes::ServerPing:
            {
                // Server has responded to a ping request
                std::chrono::system_clock::time_point timeNow = std::chrono::system_clock::now();
                std::chrono::system_clock::time_point timeThen;
                msg >> timeThen;
                std::cout << "Ping: " << std::chrono::duration<double>(timeNow - timeThen).count() << "\n";
            }
            break;

            case CustomMsgTypes::ServerMessage:
            {
                // Server has responded to a ping request	
                uint32_t clientID;
                msg >> clientID;
                std::cout << "Hello from [" << clientID << "]\n";
            }
            break;
            }
        }
    }

    std::cout << "Hello World!\n";
}