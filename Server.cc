#include <cstring>
#include <cstdio>

#include <unistd.h>

#include "Server.h"

const char *Server::SOCKET_NAME = "/dev/mt/server";


Connection::Connection(int socket_fd_, sockaddr_un client_addr_)
    :socket_fd(socket_fd_), addr(client_addr_),
    shm_fd(-1), status(sStart) {
}

Connection::~Connection() {
    // save to file
    close(socket_fd);
}

int Connection::handle() {
    // several phases
    // send uid
    // get package name, shm size
    // 
    // several callbacks
    //  - DexPcMoved
    //  - FieldRead
    //  - FieldWritten
    //  - MethodEntered
    //  - MethodExited
    //  - MethodUnwind
    //  - ExceptionCaught

    switch (status) {
        case sStart:
        int32_t shakeval;
        size_t written;
        written = read(socket_fd, &shakeval, 4);
        if (written == 4) {
            if (shakeval == SPECIAL_VALUE){
                status = sRunning;
                printf("connection success!\n");
                return 1;
            }
            fprintf(stderr, "special value %X\n", shakeval);
        }
        return 0;

        case sRunning:
        printf("running...\n");
        char t;
        written = read(socket_fd, &t, 1);
        if (written == 0) {
            fprintf(stderr, "End on the running\n");
            return 0;
        }
    }
    return 1;
}

// Connection::read(void *buffer, int size) {
//     // fd_set read_fds;

//     // FD_ZERO(&read_fds);
//     // FD_SET(socket_fd, )
// }

Server::Server(int16_t uid): uid_(uid) {
    int yes;
    printf("Creating Socket...\n");
    if ((socket_fd = socket(AF_UNIX, SOCK_STREAM, 0)) == -1)  {
        fprintf(stderr, "socket\n");
        socket_fd = -1;
        return;
    }

    if (setsockopt(socket_fd, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(int)) == -1) {
        fprintf(stderr, "setsockopt\n");
        socket_fd = -1;
    }
}

Server::~Server() {
    std::list<Connection *>::iterator iterator = connections.begin();
    while (iterator != connections.end()) {
        Connection *connection = *iterator;
        delete connection;
        iterator++;
    }
    close(socket_fd);
}

int Server::run() {
    if (socket_fd == -1) return -1;
    sockaddr_un server_addr;
    int yes;

    memset(&server_addr, 0, sizeof (struct sockaddr_un));
    server_addr.sun_family = AF_UNIX;
    strcpy(&server_addr.sun_path[1], SOCKET_NAME);
    int addrlen = sizeof (server_addr.sun_family) + strlen(&server_addr.sun_path[1]) + 1;

    if (bind(socket_fd, (struct sockaddr *)&server_addr, addrlen) < 0) {
        fprintf(stderr, "bind\n");
        return -errno;
    }

    if (listen(socket_fd, 5) < 0) {
        fprintf(stderr, "listen\n");
        return -errno;
    }

    std::list<Connection *>::iterator iterator;
    for (;;) {
        fd_set read_fds;

        FD_ZERO(&read_fds);
        FD_SET(socket_fd, &read_fds);

        int max_fd = socket_fd;
        for (iterator = connections.begin();
                iterator != connections.end();
                iterator++) {
            Connection *connection = *iterator;
            int _fd = connection->get_socket_fd();
            FD_SET(_fd, &read_fds);
            if (_fd > max_fd) {
                max_fd = _fd;
            }
        }

        int num_sockets = select(max_fd + 1, &read_fds, NULL, NULL, NULL);

        if (num_sockets < 0) {
            fprintf(stderr, "select");
            return -errno;
        }

        if (num_sockets == 0) {
            continue;
        }

        if (FD_ISSET(socket_fd, &read_fds)) {
            num_sockets--;

            sockaddr_un client_addr;
            socklen_t client_sock_len = sizeof (sockaddr_un);
            int client_sock = accept(
                socket_fd,
                (struct sockaddr *) &client_addr,
                &client_sock_len
            );

            if (client_sock <= 0) {
                fprintf(stderr, "accept");
                goto handle_traffic;
            }

            // send uid
            size_t ret = write(client_sock, &uid_, 2);
            if (ret != 2) {
                fprintf(stderr, "write uid");
                goto handle_traffic;
            }

            Connection *connection = new Connection(client_sock, client_addr);
            connections.push_back(connection);
        }

        // manage all connections
        handle_traffic:
        iterator = connections.begin();
        while (iterator != connections.end() && num_sockets > 0) {
            Connection *connection = *iterator;

            if (!FD_ISSET(connection->get_socket_fd(), &read_fds)) {
                iterator++;
                continue;
            }

            num_sockets--;

            // end of the connection
            if (!connection->handle()) {
                delete connection;
                iterator = connections.erase(iterator);
                continue;
            }

            iterator++;
        }
    }

    return 0;
}
