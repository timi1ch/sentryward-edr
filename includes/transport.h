#ifndef TRANSPORT_H
#define TRANSPORT_H

struct transport {
    int fd;
};

int socket_open(const char *client_path);
void socket_close(int fd, const char *client_path);

int send_json_message(int fd, const char *msg);

#endif
