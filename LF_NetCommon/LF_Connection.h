#pragma once
#include <asio.hpp>

#include "LF_Message.h"
#include "LF_TSQueue.h"

namespace net
{
	template<typename T>
	class Connection : public std::enable_shared_from_this<connection<T>>
	{
	public:
		enum class Owner
		{
			server,
			client
		};

		Connection(Owner parent, asio::io_context& asioContext, asio::ip::tcp::socket socket, tsqueue<owned_message<T>>& qIn)
			: m_asioContext(asioContext), m_socket(std::move(socket)), m_qMessagesIn(qIn), m_OwnerType(parent) {}

		virtual ~Connection() {}

		// This ID is used system wide - its how clients will understand other clients
		// exist across the whole system.
		uint32_t GetID() const
		{
			return id;
		}

		void ConnectToClient(uint32_t uid = 0)
		{
			if (m_OwnerType == Owner::server)
			{
				if (m_socket.is_open())
				{
					id = uid;
					ReadHeader();
				}
			}
		}

		void ConnectToServer(const asio::ip::tcp::resolver::results_type& endpoints)
		{
			// Only clients can connect to servers
			if (m_OwnerType == Owner::client)
			{
				// Request asio attempts to connect to an endpoint
				asio::async_connect(m_socket, endpoints,
					[this](std::error_code ec, asio::ip::tcp::endpoint endpoint)
					{
						if (!ec)
						{
							ReadHeader();
						}
					});
			}
		}

		void Disconnect()
		{
			if (IsConnected())
				asio::post(m_asioContext, [this]() { m_socket.close(); });
		}

		bool IsConnected() const
		{
			return m_socket.is_open();
		}

	private:
		// ASYNC - Prime context ready to read a message header
		void ReadHeader()
		{
			// If this function is called, we are expecting asio to wait until it receives
			// enough bytes to form a header of a message. We know the headers are a fixed
			// size, so allocate a transmission buffer large enough to store it. In fact, 
			// we will construct the message in a "temporary" message object as it's 
			// convenient to work with.
			asio::async_read(m_socket, asio::buffer(&m_msgTemporaryIn.header, sizeof(message_header<T>)),
				[this](std::error_code ec, std::size_t length)
				{
					if (!ec)
					{
						// A complete message header has been read, check if this message
						// has a body to follow...
						if (m_msgTemporaryIn.header.size > 0)
						{
							// ...it does, so allocate enough space in the messages' body
							// vector, and issue asio with the task to read the body.
							m_msgTemporaryIn.body.resize(m_msgTemporaryIn.header.size);
							ReadBody();
						}
						else
						{
							// it doesn't, so add this bodyless message to the connections
							// incoming message queue
							AddToIncomingMessageQueue();
						}
					}
					else
					{
						// Reading form the client went wrong, most likely a disconnect
						// has occurred. Close the socket and let the system tidy it up later.
						std::cout << "[" << id << "] Read Header Fail.\n";
						m_socket.close();
					}
				});
		}

		// ASYNC - Prime context ready to read a message body
		void ReadBody()
		{
			// If this function is called, a header has already been read, and that header
			// request we read a body, The space for that body has already been allocated
			// in the temporary message object, so just wait for the bytes to arrive...
			asio::async_read(m_socket, asio::buffer(m_msgTemporaryIn.body.data(), m_msgTemporaryIn.body.size()),
				[this](std::error_code ec, std::size_t length)
				{
					if (!ec)
					{
						// ...and they have! The message is now complete, so add
						// the whole message to incoming queue
						AddToIncomingMessageQueue();
					}
					else
					{
						// As above!
						std::cout << "[" << id << "] Read Body Fail.\n";
						m_socket.close();
					}
				});
		}

		// Once a full message is received, add it to the incoming queue
		void AddToIncomingMessageQueue()
		{
			// Shove it in queue, converting it to an "owned message", by initialising
			// with the a shared pointer from this connection object
			if (m_OwnerType == Owner::server)
				m_qMessagesIn.push_back({ this->shared_from_this(), m_msgTemporaryIn });
			else
				m_qMessagesIn.push_back({ nullptr, m_msgTemporaryIn });

			// We must now prime the asio context to receive the next message. It 
			// wil just sit and wait for bytes to arrive, and the message construction
			// process repeats itself. Clever huh?
			ReadHeader();
		}

	protected:
		// Each connection has a unique socket to a remote 
		asio::ip::tcp::socket m_socket;

		// This context is shared with the whole asio instance
		asio::io_context& m_asioContext;

		// The "owner" decides how some of the connection behaves
		Owner m_OwnerType = Owner::server;

		// This references the incoming queue of the parent object
		tsqueue<owned_message<T>>& m_qMessagesIn;

		// Incoming messages are constructed asynchronously, so we will
		// store the part assembled message here, until it is ready
		message<T> m_msgTemporaryIn;

		uint32_t id = 0;

	};
}// namespace net
