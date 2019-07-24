
#ifndef SERVER_H_
#define SERVER_H_

#include <list>

#include <sys/types.h>
#include <sys/socket.h>
#include <sys/un.h>

enum connectionStatus {
    sStart, // wait for first response (matching uid)
    sRunning // logs
};

class Connection {
public:
    Connection(int socket, sockaddr_un client_addr);
    virtual ~Connection();

    int handle();

    int get_socket_fd(){return socket_fd;}

private:
    // size_t read(void *buffer, int len);

    int socket_fd;
    sockaddr_un addr;

    int shm_fd;

    connectionStatus status;
    static const int SPECIAL_VALUE = 0x7415963; // handshake value
};


class Server {

public:
    Server (int16_t uid);
    virtual ~Server();

    int run();

    static const char *SOCKET_NAME;

private:
    int16_t uid_;
    int socket_fd;
    std::list<Connection *> connections;
};


#endif
