#include <sys/socket.h>
#include <netinet/in.h>
#include <string>
#include <cerrno>
#include <unistd.h>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <stdlib.h>
#include "xslog.hpp"

static void read_message(int conn_fd) {
    char buffer[64];
    ssize_t n = read(conn_fd, buffer, sizeof(buffer) - 1);
    if (n < 0) {
        xslog::error("read()");
        return;
    }
    xslog::info(buffer);
}

static void write_message(int conn_fd, const char* message) {
    ssize_t n = write(conn_fd, message, strlen(message));
    if(n < 0) {
        xslog::error("write()");
        return;
    }
}

int main() {
    int server_sock_fd = socket(AF_INET, SOCK_STREAM, 0);
    if(server_sock_fd < 0) {
        xslog::error("socket()");
        abort();
    }

    int val = 1;
    if(setsockopt(server_sock_fd, SOL_SOCKET, SO_REUSEADDR, &val, sizeof(val)) < 0) {
        xslog::error("setsockopt()");
        abort();
    }

    sockaddr_in server_addr;
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(9090);
    server_addr.sin_addr.s_addr = INADDR_ANY;

    if(bind(server_sock_fd, (struct sockaddr*)(&server_addr), sizeof(server_addr)) < 0) {
        xslog::error("bind()");
        abort();
    };

    if(listen(server_sock_fd, SOMAXCONN) < 0) {
        xslog::error("listen()");
        abort();
    }

    char message[] = "server is in progress";
    while(true) {
        struct sockaddr_in client_addr = {};
        socklen_t client_addr_len = sizeof(client_addr);
        int conn_fd  =  accept(server_sock_fd, (struct sockaddr *)&client_addr, &client_addr_len);
        if(conn_fd < 0) {
            continue;
        }
        read_message(conn_fd);
        write_message(conn_fd, message);
        close(conn_fd);
    }

    return 0;
}
