#include "transport.h"
#include <asm-generic/errno-base.h>
#include <asm-generic/socket.h>
#include <errno.h>
#include <stddef.h>
#include <stdio.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <sys/time.h>
#include <unistd.h>
#include <string.h>
#include <stdint.h>

#define SOCK_PATH "/run/sentryward/events.sock"

static int send_all (int fd, const void *buf, size_t len) {
    const uint8_t *p  = buf;
    size_t sent = 0;
    while (sent < len) {
        ssize_t n = send(fd, p + sent, len - sent, MSG_NOSIGNAL);
        if (n < 0) {
            if (errno == EINTR) { continue; }
            return -errno;
        }
        if (n == 0) { return -EIO; }
        sent += (size_t)n;
    }
    return 0;
}

int send_json_message(int fd, const char *json_msg) {
    if (!json_msg) { return -EINVAL; }
    size_t len = strlen(json_msg);

    char *end_str = "\n";
    size_t end_len = strlen(end_str);

    int ret_msg = send_all(fd, json_msg, len);
    if (ret_msg != 0) { return ret_msg; }
    return send_all(fd, end_str, end_len);
}

void socket_close(int fd, const char *client_path) {
    close(fd);
    unlink(client_path);
}

int socket_open(const char *client_path) {
    struct sockaddr_un client_addr;
    if (!client_path || !client_path[0]) { return -EINVAL; }
    size_t client_len = strlen(client_path);
    if (client_len >= sizeof(client_addr.sun_path)) { return -ENAMETOOLONG; }

    int fd = socket(AF_UNIX, SOCK_STREAM|SOCK_CLOEXEC, 0);
    if (fd == -1) { return -errno; }
    
    struct timeval timeout;
    timeout.tv_sec = 1; timeout.tv_usec = 0;
    if (setsockopt(fd, SOL_SOCKET, SO_SNDTIMEO, &timeout, sizeof(timeout)) == -1) {
        int err = errno;
        close(fd);
        return -err;
    }

    memset(&client_addr, 0, sizeof(client_addr));
    client_addr.sun_family = AF_UNIX;
    memcpy(client_addr.sun_path, client_path, client_len + 1);
    if (bind(fd, (struct sockaddr *)&client_addr, sizeof(client_addr)) == -1) {
        int err = errno;
        close(fd);
        return -err;
    }

    struct sockaddr_un addr;
    memset(&addr, 0, sizeof(addr));
    addr.sun_family = AF_UNIX;
    strncpy(addr.sun_path, SOCK_PATH, sizeof(addr.sun_path) - 1);

    if (connect(fd, (struct sockaddr *)&addr, sizeof(addr)) == -1) {
        int err = errno;
        socket_close(fd, client_path);
        return -err;
    }

    return fd;
}
