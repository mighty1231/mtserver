
#ifndef SERVER_H_
#define SERVER_H_

#include <sys/socket.h>
#include <sys/un.h>

#include <list>

enum connectionStatus {
    sStart, // wait for first response (matching uid)
    sRunning // logs
};

class Connection {
public:
    Connection(int socket, char *package_name);
    virtual ~Connection();

    int handle();

    int get_socket_fd(){return socket_fd;}
    int send_available_prefix();

private:
    static const int SPECIAL_VALUE = 0x7415963; // handshake value

    int socket_fd;
    char package_name[64];

    connectionStatus status;

    int available_index;
};


class Server {

public:
    Server (int16_t uid, char *package_name);
    virtual ~Server();

    int run();

    static const char *SOCKET_NAME;

private:
    int16_t uid;
    char package_name[64];
    int socket_fd;
    std::list<Connection *> connections;
};


#endif
