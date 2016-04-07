#include <winsock2.h>
#include <exception>
#include <sstream>
#include <iostream> // ONLY FOR DEBUG
#include <ws2tcpip.h>
#pragma comment(lib,"ws2_32.lib") 

#include "scope_exit.h"
#include "tcp_communicator.h"

Tcp_communicator::Tcp_communicator()
{
	SOCKET ClientSocket = INVALID_SOCKET;

	// Initialize Winsock
	if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0)
		throw std::exception("Error WSAStartup() failed\n");

    ScopeExit wsa_cleanup = MakeGuard(WSACleanup);

	struct addrinfo hints;
	ZeroMemory(&hints, sizeof(hints));
	hints.ai_family = AF_INET;
	hints.ai_socktype = SOCK_STREAM;
	hints.ai_protocol = IPPROTO_TCP;
	hints.ai_flags = AI_PASSIVE;

	// Resolve the server address and port
	struct addrinfo *result = NULL;
	if (getaddrinfo(NULL, std::to_string(0).c_str(), &hints, &result) != 0) // 2nd parameter 0 = pick any number
	{
		throw std::exception("Error getaddrinfo failed\n");
	}

	// Create a SOCKET for connecting to server
	listen_socket = socket(result->ai_family, result->ai_socktype, result->ai_protocol);
	if (listen_socket == INVALID_SOCKET)
	{
		freeaddrinfo(result);
		throw std::exception("Error creating socket failed\n");
	}

	// Setup the TCP listening socket
	if (bind(listen_socket, result->ai_addr, (int)result->ai_addrlen) == SOCKET_ERROR)
	{
		freeaddrinfo(result);
		closesocket(listen_socket);
		throw std::exception("Error binding socket failed\n");
	}
	freeaddrinfo(result);

    wsa_cleanup.Dismiss();
}

//	-------------------------------------------------------------------------------------------


void Tcp_communicator::set_timeout(int seconds)
{
	is_nonblocking = true;
	timeout.tv_sec = seconds;
	timeout.tv_usec = 1; // What could possibly happen ?
}

//	-------------------------------------------------------------------------------------------

void Tcp_communicator::unset_timeout()
{
	is_nonblocking = false;
}

//	-------------------------------------------------------------------------------------------



//	-------------------------------------------------------------------------------------------

Tcp_communicator::~Tcp_communicator()
{
	closesocket(listen_socket);
	close_connection();
	WSACleanup();
}

//	-------------------------------------------------------------------------------------------

unsigned int Tcp_communicator::get_port_number() const 
// Determine port number from listen socket
{
	if (listen_socket == INVALID_SOCKET)
		return 0;
	struct sockaddr_in sin;
	int addrlen = sizeof(sin);
	if (getsockname(listen_socket, (struct sockaddr *)&sin, &addrlen) == 0 && sin.sin_family == AF_INET && addrlen == sizeof(sin))
		return ntohs(sin.sin_port);
	throw std::exception{ "Couldn't determine port number for listening socket." };
}

//	-------------------------------------------------------------------------------------------

bool Tcp_communicator::accept_connection()
{
	if (!listening)
		if (listen(listen_socket, SOMAXCONN) == SOCKET_ERROR)
			throw std::exception{ "Error listen() failed\n" };
		else
			listening = true;
	if (is_nonblocking)
	{
		fd_set socket_fds;
		FD_ZERO(&socket_fds);
		FD_SET(listen_socket, &socket_fds);

		int ret = select(0, &socket_fds, NULL, NULL, &timeout);

		if (ret == -1)
			throw std::exception("Multiplexed io - select returns error ");
		if (ret == 0)
			return false;
	}

	if ((client_socket = accept(listen_socket, NULL, NULL)) == INVALID_SOCKET)
		throw std::exception{ "Error accept() failed\n" };
	socket_connected = true;
	return true;
}

//	-------------------------------------------------------------------------------------------

std::vector<byte> Tcp_communicator::get_data()
{
	if (!is_connected())
		throw std::exception{ "Trying to get_data from not connected socket" };
	
	int result;
	std::vector<byte> data;
	constexpr int buffer_size = 1000;
	byte raw_data[buffer_size+1]; // +1 just to be sure

	while (true) 
	{
		if (is_nonblocking)
		{
			fd_set socket_fds;
			FD_ZERO(&socket_fds);
			FD_SET(client_socket, &socket_fds);

			int ret = select(0, &socket_fds, NULL, NULL, &timeout);

			if (ret == -1)
				throw std::exception("Multiplexed io - select returns error ");
			if (ret == 0)
				return data; // Client is probably waiting, so return
		}
		
		result = recv(client_socket, reinterpret_cast<char *>(raw_data), buffer_size, 0);
		
		if (result == 0) // Connection closed
		{
			socket_connected = false;
			close_connection();
			return data;
		}
		if (result == SOCKET_ERROR )
			throw std::exception("Error while receiving data!\n");

		data.insert(data.end(), &raw_data[0], &raw_data[result]); // Concatenate data
		
		if (result != buffer_size) // Accepted less than buffer size = that's probably all :) 
			return data;
	}
}

//	-------------------------------------------------------------------------------------------

void Tcp_communicator::send_data(const std::string & data)
{
	if (!is_connected())
		throw std::exception{ "Trying to send_data via not connected socket" };
	if (send(client_socket, &data[0], data.length(), 0) == SOCKET_ERROR)
		throw std::exception("Error send()\n");
}

//	-------------------------------------------------------------------------------------------

bool Tcp_communicator::close_connection()
{
	if (client_socket == INVALID_SOCKET)
		return true;
	if (!socket_connected)
	{
		closesocket(client_socket);
		client_socket = INVALID_SOCKET;
		return true;
	}
	
	socket_connected = false;
	if  (shutdown(client_socket, SD_SEND) == SOCKET_ERROR)
	{
		closesocket(client_socket);
		client_socket = INVALID_SOCKET;
		return false;
	}
	closesocket(client_socket);
	client_socket = INVALID_SOCKET;
	return true;
}

//	-------------------------------------------------------------------------------------------
