#include <sys/socket.h>
#include <netinet/in.h>
#include <string>
#include <cerrno>
#include <unistd.h>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <stdlib.h>
#include <poll.h>
#include "conn.hpp"
#include "xslog.hpp"

static void handle_read(conn* c) {
    return;
}

static void handle_write(conn* c) {
    return;
}

void populate_poll_args(vector<conn*>& fd_to_conn, vector<struct pollfd>& poll_args) {
    for(conn c: fd_to_conn) { 
        if(!c) {
            continue;
        }
        struct pollfd p = {c->fd, POLLERR, 0};
        if(c->want_read) {
            p.events |= POLLIN;
        }
        if(c->want_write) {
            p.events |= POLLOUT;
        }
        poll_args.push_back(p);
    }
}

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

static void set_nonblocking(int fd) {
    int errno = 0;
    int ops = fcntl(fd, F_GETFL, 0) | O_NONBLOCK;
    if(errno) {
        xslog::error("fcntl() couldn't get flags");
        return;
    }
    fcntl(fd, F_SETFL, ops);
    if(errorno) {
        xslog::error("fcntl() couldn't set flags");
        return;
    }
    return;
}

static conn* accept_connection(int fd) {
    struct sockaddr_in addr;
    struct socklen_t addr_len = sizeof(client_addr);
    int conn_fd = accept(fd, &addr, &addr_len);
    if(conn_fd < 0) {
        return nullptr;
    }
    set_nonblocking(conn_fd);
    conn* c = new conn();
    c->fd = conn_fd;
    c->want_read = true;
    return c;
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

    vector<conn*> fd_to_conn;
    // Input for poll()
    vector<struct pollfd> poll_args;

    // Event loop
    while(true) {
        // Preparing input for poll()
        poll_args.clear()
        struct pollfd p = {server_sock_fd, POLLIN, 0};
        poll_args.push_back(p);

        // From the fd_to_conn vector get whatever wants to read and write, intially it will be empty
        // so the only argument that goes into poll() is the the server_socket_fd that wants to read
        populate_poll_args(fd_to_conn, poll_args);

        // Call poll()
        int rv = poll(poll_args.data(), (nfds_t)sizeof(poll_args), -1);

        if(rv < 0 && errno == EINTR) {
            continue;
        }
        if(rv < 0) {
            xslog::error('poll()');
            abort();
        }

        // Handle the listening socket
        if(poll_args[0].revents) {
            if(conn* c = accept_connection(server_sock_fd)) {
                if(fd_to_conn.size() <= (size_t)c->fd) {
                    fd_to_conn.resize(c->fd + 1);
                }
            }
            assert(!fd_to_conn[c->fd]);
            fd_to_conn[c->fd] = c;
        }

        // Handle connections that want to read/write
        for(int i = 1; i < poll_args.size(); i++) {
            uint32_t revents = poll_args[i].revents;
            if(revents & POLLIN) {
                handle_read(conn);
            }   
            if(revents & POLLOUT) {
                handle_write(conn);
            }
        }

        for(int i = 1; i < poll_args.size(); i++) {
            uint32_t revents = poll_args[i].revents;
            conn* c = fd_to_conn(poll_args[i].fd);
            if(revents & POLLERR || c->fd) {
                close(c->fd);
                fd_to_conn[c->fd] = nullptr;
                delete conn;
            }
        }

    }

    return 0;
}
