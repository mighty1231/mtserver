
#ifndef SERVER_H_
#define SERVER_H_

#include <sys/socket.h>
#include <sys/un.h>

#include <list>

// Currently, LOG_DEX_PC_MOVED is not used
#define LOG_METHOD_ENTER      0x00000001
#define LOG_METHOD_EXIT       0x00000002
#define LOG_METHOD_UNWIND     0x00000004
#define LOG_DEX_PC_MOVED      0x00000008
#define LOG_FIELD_READ        0x00000010
#define LOG_FIELD_WRITE       0x00000020
#define LOG_EXCEPTION_CAUGHT  0x00000040
#define LOG_COVERAGE          0x00000080
#define LOG_MESSAGE           0x00000100
#define CONNECT_APE           0x00010000
#define LOG_ONE_SEC_PING      0x00020000
#define LOG_FIELD_TYPE_FLAGS  0x0F000000
#define LOG_FIELD_TYPE0       0x01000000
#define LOG_FIELD_TYPE1       0x02000000
#define LOG_FIELD_TYPE2       0x04000000
#define LOG_FIELD_TYPE3       0x08000000
#define LOG_METHODTYPE_FLAGS  0xF0000000
#define LOG_METHOD_TYPE0      0x10000000
#define LOG_METHOD_TYPE1      0x20000000
#define LOG_METHOD_TYPE2      0x40000000
#define LOG_METHOD_TYPE3      0x80000000
#define LOG_ALL_FLAGS         0xFF0301FF
#define LOG_FLAG_DEFAULT      0xA8020077

struct s_log_type {
    uint32_t value;
    const char *desc;
};

extern const struct s_log_type log_types[];
#define APE_PACKAGE "ape"

enum connectionStatus {
    sStart, // wait for first response (matching uid)
    sPidRead, // pid is read
    sRunning
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

    char prefix_buf[256];
    char fname_buf[256];
    int fname_buf_offset;
};


class Server {

public:
    Server (uid_t uid, char *package_name, uint32_t log_type);
    virtual ~Server();

    int run();
    int get_available_prefix(char *prefix_buf);
    bool is_test_server() {return package_name[0] == 0;}
    int get_log_type() {return log_type;}

    static const char SOCKET_NAME[];
    static const char MTDATA_DIRNAME[];

private:
    uid_t uid;
    char package_name[64];
    const uint32_t log_type;
    int socket_fd;
    std::list<Connection *> connections;
    int available_index;
    bool _target_ape;
};


#endif
