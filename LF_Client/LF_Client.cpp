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
    c.Connect("192.168.0.102", 52000);
    c.IsConnected();

    std::cout << "Hello World!\n";
}