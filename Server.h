
#ifndef SERVER_H_
#define SERVER_H_

#include <sys/socket.h>
#include <sys/un.h>

#include <list>

enum connectionStatus {
    sStart, // wait for first response (matching uid)
    sPidRead, // pid is read
    sRunning, // logs
    sEnding
};

class Connection {
public:
    Connection(int socket, char *package_name);
    virtual ~Connection();

    int handle();

    int get_socket_fd(){return socket_fd;}
    int send_available_prefix();

private:
    int socket_fd;
    pid_t pid;
    char package_name[64];

    connectionStatus status;

    char fname_buf[256];
    int fname_buf_offset;

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
