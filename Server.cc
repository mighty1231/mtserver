#include <cstring>
#include <cstdio>

#include <unistd.h>
#include <dirent.h>

#include "Server.h"

const char *Server::SOCKET_NAME = "/dev/mt/server";


Connection::Connection(int socket_fd_, Server *server_)
    :socket_fd(socket_fd_), pid(-1), server(server_),
    status(sStart), fname_buf_offset(0) {
}

Connection::~Connection() {
    // save to file
    close(socket_fd);
}

int Connection::handle() {
    // several phases
    // send uid, send package name
    size_t written;
    switch (status) {
        case sStart:
            written = read(socket_fd, &pid, 4);
            if (written == 4) {
                status = sPidRead;
                return 1;
            }
            return 0;
        case sPidRead:
            int32_t shakeval;
            written = read(socket_fd, &shakeval, 4);
            if (written == 4) {
                if (shakeval == 0x7415963) {
                    printf("[Socket %d] Connection with pid %d from Start()\n", socket_fd, pid);
                    int log_type = server->get_log_type();
                    if (write(socket_fd, &log_type, 4) == 4 && send_available_prefix()) {
                        status = sRunning;
                        return 1;
                    }
                } else {
                    fprintf(stderr, "[Socket %d] Given special value not expected %X\n", socket_fd, shakeval);
                }
            } 
            return 0;

        case sRunning:
            int bytes_to_read = (sizeof fname_buf) - fname_buf_offset;
            if (bytes_to_read == 0) {
                fprintf(stderr, "[Socket %d] File path longer than %d byte\n",
                    socket_fd, sizeof fname_buf);
                return 0;
            }
            written = read(socket_fd, fname_buf + fname_buf_offset, bytes_to_read);
            if (written == 0) {
                if (fname_buf_offset != 0) {
                    fname_buf[fname_buf_offset] = 0;
                    fprintf(stderr, "[Socket %d] Incomplete file released: release %s\n", socket_fd, fname_buf);
                }
                printf("[Socket %d] Connection closed, prefix=%s\n", socket_fd, prefix_buf);
                return 0;
            } else {
                int left_offset, right_offset;
                left_offset = fname_buf_offset;
                right_offset = fname_buf_offset + written;
                fname_buf_offset = right_offset;

                void *null_loc;

            read_file_loop:
                null_loc = memchr(fname_buf + left_offset, 0, right_offset - left_offset);
                if (null_loc == NULL) {
                    // more bytes should be read.
                    return 1;
                } else {
                    int bytes_to_slide = strlen(fname_buf) + 1;
                    printf("[Socket %d] File released: %s\n", socket_fd, fname_buf);
                    memmove(fname_buf, (char *) null_loc + 1, right_offset - bytes_to_slide);
                    left_offset = 0;
                    right_offset -= bytes_to_slide;
                    fname_buf_offset -= bytes_to_slide;
                    if (left_offset == right_offset) 
                        return 1;
                    goto read_file_loop;
                }
            }
    }
    return 1;
}

int Connection::send_available_prefix() {
    int prefix_length = server->get_available_prefix(prefix_buf);

    if (write(socket_fd, &prefix_length, 4) != 4) {
        fprintf(stderr, "[Socket %d] Write prefix errno %d\n", socket_fd, errno);
        return 0;
    }
    if (write(socket_fd, prefix_buf, prefix_length + 1) != prefix_length + 1) {
        fprintf(stderr, "[Socket %d] Write prefix errno %d\n", socket_fd, errno);
        return 0;
    }
    printf("[Socket %d] Selected prefix: %s\n", socket_fd, prefix_buf);
    return 1;
}

Server::Server(uid_t uid_, char *package_name, uint32_t log_type_)
        : uid(uid_), log_type(log_type_), available_index(0) {
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

    if (is_test_server()) {
        printf("Test server is constructed\n");
    } else {
        printf("Server with uid %u package name %s is constructed on Socket %s\n", uid_, package_name, SOCKET_NAME);
        strcpy(this->package_name, package_name);
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

int Server::get_available_prefix(char *prefix_buf) {
    if (is_test_server()) {
        int length = sprintf(prefix_buf, "Hello_my_friend_%d", available_index);
        available_index++;
        return length;
    }
    char tmpbuf[256];
    sprintf(tmpbuf, "/data/data/%s/", package_name);

    DIR *dir = opendir(tmpbuf);
    dirent *entry;
    int32_t flen;

    while (true) { // loop for available_index
        sprintf(tmpbuf, "mt_%d_", available_index);
        flen = strlen(tmpbuf);
        while ((entry = readdir(dir)) != NULL) {
            if (memcmp(entry->d_name, tmpbuf, flen) == 0) {
                // If there is some file with given tmpbuf,
                //    tmpbuf should be changed.
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
    int length = sprintf(prefix_buf, "/data/data/%s/mt_%d_", package_name, available_index);
    available_index++;
    return length;
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
            ret = write(client_sock, &uid, sizeof (uid_t));
            if (ret != sizeof (uid_t)) {
                fprintf(stderr, "write uid\n");
                goto handle_traffic;
            }

            // Add valid connection to list
            Connection *connection = new Connection(client_sock, this);
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
