#include "Server.h"
#include "Client.h"
#include <dirent.h>
#include <pwd.h>
#include <sys/stat.h>

uid_t getuid(const char *package_name) {
    char buffer[256];
    sprintf(buffer, "/data/data/%s", package_name);

    struct stat info;
    if (stat(buffer, &info) == 0) {
        return info.st_uid;
    } else {
        return 0;
    }
}

int main(int argc, char** argv) {
    if (argc <= 1) {
        return -1;
    }

    if (strcmp(argv[1], "server") == 0) {
        if (argc <= 2) {
            return -1;
        }
        uid_t uid = getuid(argv[2]);
        if (uid == 0) {
            fprintf(stderr, "Package %s is not found\n", argv[2]);
            return -1;
        }

        Server server(uid, argv[2]);
        return server.run();
    } else if (strcmp(argv[1], "client") == 0) {
        if (argc <= 2) {
            return -1;
        }
        int16_t uid = (int16_t) (atoi(argv[2]) & 0xFFFF);

        Client client(uid);
        return client.run();
    }
    return -1;
}
