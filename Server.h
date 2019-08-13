
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

class Server;

class Connection {
public:
    Connection(int socket, Server *server);
    virtual ~Connection();

    int handle();

    int get_socket_fd(){return socket_fd;}

private:
    int send_available_prefix();
    int socket_fd;
    pid_t pid;
    Server *server;

    connectionStatus status;

    char fname_buf[256];
    int fname_buf_offset;
};


class Server {

public:
    Server (uid_t uid, char *package_name);
    virtual ~Server();

    int run();
    int get_available_prefix(char *prefix_buf);

    static const char *SOCKET_NAME;

private:
    uid_t uid;
    char package_name[64];
    int socket_fd;
    std::list<Connection *> connections;
    int available_index;
};


#endif
