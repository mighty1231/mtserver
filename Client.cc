#include <cstdio>
#include <unistd.h>
#include <sys/socket.h>
#include <sys/un.h>

#include "Client.h"

Client::Client() {
    sockaddr_un server_addr;
    if ((socket_fd = socket(AF_UNIX, SOCK_STREAM, 0)) < 0) {
        fprintf(stderr, "socket");
        socket_fd = -1;
        return;
    }

    else {
        memset(&server_addr, 0, sizeof server_addr);
        server_addr.sun_family = AF_UNIX;
        strcpy(&server_addr.sun_path[1], "/dev/mt/server");
        int addrlen = sizeof server_addr.sun_family + strlen(&server_addr.sun_path[1]) + 1;

        if (connect(socket_fd, (sockaddr *)&server_addr, addrlen) < 0) {
            fprintf(stderr, "connect");
            close(socket_fd);
            socket_fd = -1;
        }
    }
}

int Client::run() {
    if (socket_fd == -1) {
        return -1;
    }

    uid_t targetuid;
    int written;
    char buf[100];
    written = read(socket_fd, &targetuid, sizeof (uid_t));
    if (written == sizeof (uid_t) && targetuid == 0) {
        printf("UID matches!\n");
        int32_t SPECIAL_VALUE = 0x7415963;
        int32_t pid = getpid();
        write(socket_fd, &pid, 4);
        write(socket_fd, &SPECIAL_VALUE, 4);

        int32_t logflag, prefix_length;
        char buf[256];
        read(socket_fd, &logflag, 4);
        read(socket_fd, &prefix_length, 4);
        read(socket_fd, buf, prefix_length + 1);
        printf("Given logflag: %d\n", logflag);
        printf("Given message: %s\n", buf);
        close(socket_fd);
        return 0;
    } else {
        printf("UID NOT MATCH\n");
        return -1;

    }
    close(socket_fd);
    return -1;
}
