
#ifndef SERVER_H_
#define SERVER_H_

#include <sys/socket.h>
#include <sys/un.h>

#include <list>

#define TEST_SERVER_UID       0

// Currently, LOG_DEX_PC_MOVED is not used
#define LOG_METHOD_ENTER      0x00000001
#define LOG_METHOD_EXIT       0x00000002
#define LOG_METHOD_UNWIND     0x00000004
#define LOG_DEX_PC_MOVED      0x00000008
#define LOG_FIELD_READ        0x00000010
#define LOG_FIELD_WRITE       0x00000020
#define LOG_EXCEPTION_CAUGHT  0x00000040
#define LOG_COVERAGE          0x00000080
#define LOG_FILTER            0x00000100
#define LOG_ALL_FLAGS         0x000001F7

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
    Server (uid_t uid, char *package_name, uint32_t log_type);
    virtual ~Server();

    int run();
    int get_available_prefix(char *prefix_buf);
    bool is_test_server() {return (uid == TEST_SERVER_UID);}

    static const char *SOCKET_NAME;
    const uint32_t log_type;

private:
    uid_t uid;
    char package_name[64];
    int socket_fd;
    std::list<Connection *> connections;
    int available_index;
};


#endif
