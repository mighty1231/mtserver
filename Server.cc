#include <cstring>
#include <cstdio>

#include <unistd.h>
#include <dirent.h>

#include "Server.h"

const char *Server::SOCKET_NAME = "/dev/mt/server";


Connection::Connection(int socket_fd_, char *package_name_)
    :socket_fd(socket_fd_),
    status(sStart), available_index(0) {

    strcpy(package_name, package_name_);
}

Connection::~Connection() {
    // save to file
    close(socket_fd);
}

int Connection::handle() {
    // several phases
    // send uid, send package name
    switch (status) {
        case sStart:
            int32_t shakeval;
            size_t written;
            written = read(socket_fd, &shakeval, 4);
            if (written == 4) {
                if (shakeval == SPECIAL_VALUE) {
                    printf("[Socket %d] Connection Success!\n", socket_fd);
                    if (send_available_prefix()) {
                        status = sRunning;
                        return 1;
                    }
                } else {
                    fprintf(stderr, "[Socket %d] Given special value not expected %X\n", socket_fd, shakeval);
                }
            } 
            return 0;

        case sRunning:

            // just check running
            char t;
            written = read(socket_fd, &t, 1);
            if (written == 0) {
                fprintf(stderr, "[Socket %d] Connection closed\n", socket_fd);
                return 0;
            }
    }
    return 1;
}

int Connection::send_available_prefix() {
    char path[256], prefix[256];
    sprintf(path, "/data/data/%s/", package_name);

    DIR *dir = opendir(path);
    dirent *entry;
    int32_t prefix_length;

    while (true) { // loop for available_index
        sprintf(prefix, "mt_%d_", available_index);
        prefix_length = strlen(prefix);
        while ((entry = readdir(dir)) != NULL) {
            if (memcmp(entry->d_name, prefix, prefix_length) == 0) {
                // If there is some file with given prefix,
                //    prefix should be changed.
                goto continue_point;
            }
        }
        // found
        break;

        continue_point:
        available_index++;
    }
    closedir(dir);

    // found and send to socket
    sprintf(path, "/data/data/%s/mt_%d_", package_name, available_index);
    prefix_length = strlen(path);
    if (write(socket_fd, &prefix_length, 4) != 4) {
        fprintf(stderr, "[Socket %d] Write prefix errno %d\n", socket_fd, errno);
        return 0;
    }
    if (write(socket_fd, path, prefix_length + 1) != prefix_length + 1) {
        fprintf(stderr, "[Socket %d] Write prefix errno %d\n", socket_fd, errno);
        return 0;
    }
    printf("[Socket %d] Selected prefix: %s\n", socket_fd, path);
    available_index += 1;
    return 1;
}

Server::Server(int16_t uid_, char *package_name): uid(uid_) {
    int yes;
    if ((socket_fd = socket(AF_UNIX, SOCK_STREAM, 0)) == -1)  {
        fprintf(stderr, "socket\n");
        socket_fd = -1;
        return;
    }

    if (setsockopt(socket_fd, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(int)) == -1) {
        fprintf(stderr, "setsockopt\n");
        socket_fd = -1;
    }

    printf("Server with uid %hu package name %s is constructed\n", uid_, package_name);
    strcpy(this->package_name, package_name);
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
    signal(SIGPIPE, SIG_IGN);
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

    int package_length = strlen(package_name);

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
            fprintf(stderr, "select\n");
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
                fprintf(stderr, "accept\n");
                goto handle_traffic;
            }

            printf("[Socket %d] Connection attempt!\n", client_sock);

            // send uid
            int ret;
            ret = write(client_sock, &uid, 2);
            if (ret != 2) {
                fprintf(stderr, "write uid\n");
                goto handle_traffic;
            }

            // Add valid connection to list
            Connection *connection = new Connection(client_sock, package_name);
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
