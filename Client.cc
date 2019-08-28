#include <cstdio>
#include <unistd.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <pthread.h>

#include "Client.h"

Client::Client() {
    sockaddr_un server_addr;
    if ((socket_fd = socket(AF_UNIX, SOCK_STREAM, 0)) < 0) {
        fprintf(stderr, "socket\n");
        socket_fd = -1;
        return;
    }

    else {
        memset(&server_addr, 0, sizeof server_addr);
        server_addr.sun_family = AF_UNIX;
        strcpy(&server_addr.sun_path[1], "/dev/mt/server");
        int addrlen = sizeof server_addr.sun_family + strlen(&server_addr.sun_path[1]) + 1;

        if (connect(socket_fd, (sockaddr *)&server_addr, addrlen) < 0) {
            fprintf(stderr, "connect\n");
            close(socket_fd);
            socket_fd = -1;
        }
    }
}

// Test whether new thread can use unix socket created by other one.
// The answer is 'can'
static void *tempfunc(void *temparg) {
    // Check fd
    int32_t MAGIC_NUMBER = 7777;
    int socket_fd = *(int *) temparg;
    usleep(1000000);
    if (write(socket_fd, &MAGIC_NUMBER, 4) != 4) {
        // NOT REACHABLE
        fprintf(stderr, "write on new thread, errno %d\n", errno);
    } else {
        printf("Write on new thread success!\n");
        close(socket_fd);
    }
    return NULL;
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

        pthread_t thread;
        pthread_create(&thread, NULL, &tempfunc, &socket_fd);
        int32_t MESSAGE = 1234;
        write(socket_fd, &MESSAGE, 4);
        pthread_join(thread, NULL);
        MESSAGE = 5555;
        write(socket_fd, &MESSAGE, 4); // This write should fails due to close on thread
        close(socket_fd);
        return 0;
    } else {
        // NOT REACHABLE
        printf("UID does not matches!\n");
        return -1;

    }
    close(socket_fd);
    return -1;
}
