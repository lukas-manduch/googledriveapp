#pragma once
#include <vector>
#include <Windows.h>

class Tcp_communicator final
{
public:
	// Constructors
	explicit Tcp_communicator();
	//explicit Tcp_communicator(unsigned int port);

	Tcp_communicator(const Tcp_communicator&) = delete;
	Tcp_communicator(Tcp_communicator&&) = delete;

	Tcp_communicator& operator=(const Tcp_communicator&) = delete;
	Tcp_communicator& operator=(Tcp_communicator&&) = delete;

	~Tcp_communicator();
	
	// Functions
	unsigned int get_port_number() const;
	void set_timeout( _In_ int seconds );
	void unset_timeout();
	bool accept_connection();
	std::vector<byte> get_data();
	
	void send_data( _In_ const std::string& data);
	bool close_connection();

	//std::vector<byte> get_data_and_respond(unsigned int max_bytes);
	bool is_connected() const { return socket_connected; };
private:
	SOCKET listen_socket = INVALID_SOCKET;
	SOCKET client_socket = INVALID_SOCKET; // Socket for connection
	WSADATA wsaData;
	struct timeval timeout;

	bool listening { false };
	bool is_nonblocking{ false };
	bool socket_connected { false };
	//unsigned int const buff_len{ 512 };
	//unsigned int const reasonable_timeout{ 50000 };
};

