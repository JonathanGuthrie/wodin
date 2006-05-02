#if !defined(_SOCKET_HPP_INCLUDED_)
#define _SOCKET_HPP_INCLUDED_

#include <stdint.h>
#include <netinet/in.h>

#include <string>

class SocketSocketErrorException
{
};

class SocketBindErrorException
{
};


class Socket
{
private:
    int sock;
    struct sockaddr_in address; 
    // SYZYGY -- I need a copy constructor because the "connect" function should return a pointer to a Socket instance
    Socket(int socket, struct sockaddr_in address);

public:
    Socket(uint32_t bind_address, short bind_port, int backlog = 16384) throw(SocketSocketErrorException, SocketBindErrorException);
    Socket *Accept(void);
    ~Socket();
    ssize_t Receive(uint8_t *buffer, size_t size);
    ssize_t Send(const uint8_t *data, size_t length);
    int SockNum() { return sock; }
};

#endif // _SOCKET_HPP_INCLUDED_
