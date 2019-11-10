#include <cstring>
#include <cstdio>

#include <unistd.h>
#include <dirent.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <vector>

#include "Server.h"

const char Server::SOCKET_NAME[] = "/dev/mt/server";
const char Server::MTDATA_DIRNAME[] = "mt_data";

const struct s_log_type log_types[] = {
    {LOG_METHOD_ENTER,        "log method enter"},
    {LOG_METHOD_EXIT,         "log method exit"},
    {LOG_METHOD_UNWIND,       "log method unwind"},
    {LOG_DEX_PC_MOVED,        "log dex pc moved (Unused)"},
    {LOG_FIELD_READ,          "log field read"},
    {LOG_FIELD_WRITE,         "log field write"},
    {LOG_EXCEPTION_CAUGHT,    "log exception caught"},
    {LOG_COVERAGE,            "log coverage"},
    {LOG_MESSAGE,             "log message"},
    {CONNECT_APE,             "connect ape"},
    {LOG_ONE_SEC_PING,        "log one second pinging"},
    {LOG_FIELD_TYPE0,         "log field type 0 (All fields except below types)"},
    {LOG_FIELD_TYPE1,         "log field type 1 (Unused)"},
    {LOG_FIELD_TYPE2,         "log field type 2 (Unused)"},
    {LOG_FIELD_TYPE3,         "log field type 3 (Fields defined on app)"},
    {LOG_METHOD_TYPE0,        "log method type 0 (Basic API methods)"},
    {LOG_METHOD_TYPE1,        "log method type 1 (Non-basic API methods)"},
    {LOG_METHOD_TYPE2,        "log method type 2 (Unused)"},
    {LOG_METHOD_TYPE3,        "log method type 3 (Methods defined on app)"},
    {LOG_FLAG_DEFAULT,        "default flag"},
    {0, 0}
};

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
                if (server->target_ape()) {
                    char buffer[256];
                    sprintf(buffer, "/proc/%d/cmdline", pid);
                    int fd = open(buffer, O_RDONLY);
                    if (fd == -1)
                        return 0;
                    int written = read(fd, buffer, 256);
                    int idx = 0;
                    int monkey_found = 0;
                    int ape_found = 0;
                    while (idx < written) {
                        if (strcmp(&buffer[idx], "com.android.commands.monkey.Monkey") == 0)
                            monkey_found = 1;
                        else if (strcmp(&buffer[idx], "--ape") == 0)
                            ape_found = 1;
                        while (++idx < written && buffer[idx] != 0) {}
                        ++idx;
                    }
                    if (!ape_found || !monkey_found) {
                        int32_t wrong_log_type = 0xFFFFFFFF;
                        write(socket_fd, &wrong_log_type, 4);
                        return 0;
                    }
                }
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
                    if (write(socket_fd, &log_type, 4) == 4 && send_available_directory()) {
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
                printf("[Socket %d] Connection closed, directory=%s\n", socket_fd, directory_buf);
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

int Connection::send_available_directory() {
    int directory_length = server->get_available_directory(directory_buf);

    if (write(socket_fd, &directory_length, 4) != 4) {
        fprintf(stderr, "[Socket %d] Write directory errno %d\n", socket_fd, errno);
        return 0;
    }
    if (write(socket_fd, directory_buf, directory_length + 1) != directory_length + 1) {
        fprintf(stderr, "[Socket %d] Write directory errno %d\n", socket_fd, errno);
        return 0;
    }
    printf("[Socket %d] Selected directory: %s\n", socket_fd, directory_buf);
    return 1;
}

Server::Server(uid_t uid_, char *package_name, uint32_t log_type_)
        : uid(uid_), log_type(log_type_), available_index(0), _target_ape(false) {
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

    kill_process();

    if (package_name[0] == 0) {
        this->package_name[0] = 0;
        printf("Test server is constructed\n");
    } else if (strcmp(package_name, APE_PACKAGE) == 0) {
        _target_ape = true;
        printf("Server for ape is constructed\n");
    } else {
        strcpy(this->package_name, package_name);
        printf("Server with uid %u package name %s is constructed on Socket %s\n", uid_, package_name, SOCKET_NAME);
    }
}

static bool is_num(char *buf) {
    char *c = buf;
    while ( *c != 0 ) {
        if ('0' <= *c && *c <= '9') {
            ++c;
            continue;
        }
        return false;
    }
    return true;
}

void Server::kill_process() {
    if (uid < 10000) // do not terminate system app
        return;
    std::vector<int> pids;
    char tmpbuf[256] = "/proc/";
    DIR *dir = opendir(tmpbuf);
    dirent *proc_entry;
    while ((proc_entry = readdir(dir)) != NULL) {
        if (is_num(proc_entry->d_name)) {
            char *line = NULL;
            size_t len = 0;
            ssize_t read;
            sprintf(tmpbuf, "/proc/%s/status", proc_entry->d_name);
            FILE *fp = fopen(tmpbuf, "r");
            if (fp == NULL) {
                continue;
            }

            uint32_t cur_uid = 65535;
            while ((read = getline(&line, &len, fp)) != -1) {
                // Uid: 10005   10005   10005   10005
                if (strncmp(line, "Uid:\t", 5) == 0) {
                    cur_uid = atoi(line + 5);
                    break;
                }
            }

            if (uid == cur_uid)
                pids.push_back(atoi(proc_entry->d_name));

            if (cur_uid == 65535) {
                printf("Server failed to fetch uid information of pid=%s\n", proc_entry->d_name);
            }

            fclose(fp);
            free(line);
        }
    }

    for (std::vector<int>::iterator it = pids.begin(); it != pids.end(); it++) {
        printf("Killing process, uid=%d pid=%d...\n", uid, *it);
        kill(*it, SIGTERM);
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

int Server::get_available_directory(char *directory_buf) {
    if (is_test_server()) {
        int length = sprintf(directory_buf, "Hello_my_friend_%d", available_index);
        available_index++;
        return length;
    }
    char tmpbuf[256];
    if (_target_ape) {
        strcpy(tmpbuf, "/data/ape/ape_mt_data/");
    } else {
        sprintf(tmpbuf, "/data/data/%s/%s/", package_name, MTDATA_DIRNAME);
    }

    DIR *dir = opendir(tmpbuf);
    if (dir == NULL) {
        if (errno == ENOENT) {
            if (mkdir(tmpbuf, S_IRWXU | S_IRWXG | S_IRWXO) != 0) {
                fprintf(stderr, "mkdir: errno %d %s\n", errno, strerror(errno));
                exit(1);
            }
            dir = opendir(tmpbuf);
        } else {
            fprintf(stderr, "opendir: errno %d %s\n", errno, strerror(errno));
            exit(1);
        }
    }
    dirent *entry;
    int32_t flen;

    while (true) { // loop for available_index
        sprintf(tmpbuf, "%d", available_index);
        flen = strlen(tmpbuf);
        while ((entry = readdir(dir)) != NULL) {
            if (strcmp(entry->d_name, tmpbuf) == 0) {
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

    // found, makedir and send to socket
    int length;
    if (_target_ape) {
        length = sprintf(directory_buf, "/data/ape/ape_mt_data/%d", available_index);
        if (mkdir(directory_buf, S_IRWXU | S_IRWXG | S_IRWXO) != 0) {
            fprintf(stderr, "mkdir %s errno %d %s\n", directory_buf, errno, strerror(errno));
            exit(1);
        }
        directory_buf[length++] = '/';
        directory_buf[length] = 0;
    } else {
        length = sprintf(directory_buf, "/data/data/%s/%s/%d", package_name, MTDATA_DIRNAME, available_index);
        if (mkdir(directory_buf, S_IRWXU | S_IRWXG | S_IRWXO) != 0) {
            fprintf(stderr, "mkdir %s errno %d %s\n", directory_buf, errno, strerror(errno));
            exit(1);
        }
        directory_buf[length++] = '/';
        directory_buf[length] = 0;
    }
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
