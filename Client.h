#ifndef CLIENT_H_
#define CLIENT_H_

class Client {
public:
    Client(int16_t uid);

    int run();

private:
    int socket_fd;

    int16_t uid;
};

#endif
