#include "Server.h"
#include "Client.h"
#include <dirent.h>
#include <pwd.h>
#include <sys/stat.h>
#include <string>
#include <time.h>


#define TEST(t)                               \
    {                                         \
        if (!(t)) {                           \
            printf("TEST " #t " failed\n");   \
            exit(-1);                         \
        }                                     \
    }

uint32_t parseflag_16(const char *c_arr) {
    uint32_t ret = 0;
    size_t idx = 0;
    char ch = c_arr[idx];
    while (ch != 0) {
        ret <<= 4;
        if ('0' <= ch && ch <= '9')
            ret += ch - '0';
        else if ('a' <= ch && ch <= 'f')
            ret += ch - 'a' + 10;
        else if ('A' <= ch && ch <= 'F')
            ret += ch - 'A' + 10;
        else
            return ~LOG_ALL_FLAGS;
        ch = c_arr[++idx];
    }
    return ret;
}

void test_parseflag_16() {
    TEST(parseflag_16("a") == 10);
    TEST(parseflag_16("9") == 9);
    TEST(parseflag_16("20") == 32);
    TEST(parseflag_16("faf") == 4015);
    TEST(parseflag_16("AAa") == 2730);

    timeval tv;
    gettimeofday(&tv, NULL);
    printf("sec %ld usec %ld\n", tv.tv_sec, tv.tv_usec);
}

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
        fprintf(stderr, "Usage: %s server package_name [logging_flag]\n", argv[0]);
        return -1;
    }

    if (strcmp(argv[1], "server") == 0) {
        if (argc <= 2) {
            fprintf(stderr, "Usage: %s server package_name [logging_flag]\n", argv[0]);
            return -1;
        }
        if (strcmp(argv[2], "theclient") == 0) {
            // To communicate with custom client
            Server server(0, NULL, 0);
            return server.run();
        }

        // parse uid
        uid_t uid;
        if (strcmp(argv[2], APE_PACKAGE) == 0) {
            uid = 0;
        } else {
            uid = getuid(argv[2]);
            if (uid == 0) {
                fprintf(stderr, "Package %s is not found\n", argv[2]);
                return -1;
            }
        }

        // parse log type
        uint32_t log_type;
        if (argc == 3) {
            /* Default log type */
            log_type = LOG_FLAG_DEFAULT;
        } else {
            log_type = parseflag_16(argv[3]);
            if (log_type & ~LOG_ALL_FLAGS) {
                fprintf(stderr, "Logging flag should belong in %08X, default value is %08X\n",
                    LOG_ALL_FLAGS, LOG_FLAG_DEFAULT);
                return -1;
            }
        }

        Server server(uid, argv[2], log_type);
        return server.run();
    } else if (strcmp(argv[1], "client") == 0) {
        Client client;
        return client.run();
    } else if (strcmp(argv[1], "test") == 0) {
        test_parseflag_16();
    } else if (strcmp(argv[1], "list") == 0) {
        printf("List log types\n");
        printf(" FLAG      DESCRIPTION\n");
        int i = 0;
        while (log_types[i].value != 0) {
            printf(" %08X  %s\n", log_types[i].value, log_types[i].desc);
            i++;
        }
    }
    return -1;
}
