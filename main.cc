#include "Server.h"
#include "Client.h"

int main(int argc, char** argv) {
    if (argc <= 1) {
        return -1;
    }

    if (strcmp(argv[1], "server") == 0) {
        if (argc <= 3) {
            return -1;
        }
        int16_t uid = (int16_t) (atoi(argv[2]) & 0xFFFF);
        Server server(uid, argv[3]);

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
