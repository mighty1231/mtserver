#include "Server.h"
#include "Client.h"
#include <dirent.h>
#include <pwd.h>
#include <sys/stat.h>
#include <string>


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
    TEST(parseflag_16("faf") == 4016);
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

        uint32_t log_type;
        if (argc == 3) {
            log_type = LOG_METHOD_ENTER
                       | LOG_METHOD_EXIT
                       | LOG_METHOD_UNWIND
                       | LOG_FIELD_READ
                       | LOG_FIELD_WRITE
                       | LOG_EXCEPTION_CAUGHT
                       | LOG_COVERAGE;
        } else {
            log_type = parseflag_16(argv[3]);
            if (log_type & ~LOG_ALL_FLAGS) {
                return -1;
            }
        }

        Server server(uid, argv[2], log_type);
        return server.run();
    } else if (strcmp(argv[1], "client") == 0) {
        if (argc <= 2) {
            return -1;
        }
        int16_t uid = (int16_t) (atoi(argv[2]) & 0xFFFF);

        Client client(uid);
        return client.run();
    } else if (strcmp(argv[1], "test") == 0) {
        test_parseflag_16();
    }
    return -1;
}
