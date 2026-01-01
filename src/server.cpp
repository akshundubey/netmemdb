#include <sys/socket.h>
#include <netinet/in.h>
#include <string>
#include <cerrno>
#include <unistd.h>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <vector>
#include <stdlib.h>
#include <poll.h>
#include <fcntl.h>
#include <errno.h>
#include "conn.hpp"
#include "xslog.hpp"

int max_message_len = 1024;

static void append_buffer(uint8_t* from, uint8_t* to, int n) {
    memcpy(to, from, (size_t)n);
}

static bool try_one_request(conn* c) {
    if(c->incoming.size() < 4) {
        return false;
    }
    uint32_t len = 0;
    memcpy(&len, c->incoming.data(), sizeof(len));
    if(len > max_message_len) {
        c->want_close = true;
        xslog::error("message length too long");
        return false;
    }
    const uint8_t* request = &c->incoming[4];
    xslog::info((char*)request);
}

static void handle_read(conn* c) {
    uint8_t buffer[64*1024];
    int n = read(c->fd, buffer, sizeof(buffer));
    if(n < 0) {
        xslog::error("read error");
        return;
    }
    if(n == 0) {
        c->want_close = true;
        return;
    }
    append_buffer(c->incoming.data(), buffer, n);

    while(true) {
        if(try_one_request(c)){
            break;
        };
    }


}

static void handle_write(conn* c) {
}   



static void populate_poll_args(std::vector<conn*> fd_to_conn, std::vector<struct pollfd> poll_args) {
    for(conn* c: fd_to_conn) { 
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

static void set_nonblocking(int fd) {
    errno = 0;
    int ops = fcntl(fd, F_GETFL, 0) | O_NONBLOCK;
    if(errno) {
        xslog::error("fcntl() couldn't get flags");
        return;
    }
    fcntl(fd, F_SETFL, ops);
    if(errno) {
        xslog::error("fcntl() couldn't set flags");
        return;
    }
    return;
}

static conn* accept_connection(int fd) {
    struct sockaddr_in addr;
    socklen_t addr_len = sizeof(addr);
    int conn_fd = accept(fd, (sockaddr*)&addr, &addr_len);
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

    // We use conn because pollfd doesn't have field to store data
    std::vector<conn*> fd_to_conn;
    // Input for poll()
    std::vector<struct pollfd> poll_args;

    // Event loop
    
    while(true) {
        // Preparing input for poll()
        poll_args.clear();
        struct pollfd p = {server_sock_fd, POLLIN, 0};
        poll_args.push_back(p);

        // From the fd_to_conn vector get whatever wants to read and write, intially it will be empty
        // so the only argument that goes into poll() is the the server_sock_fd that wants to read
        populate_poll_args(fd_to_conn, poll_args);

        // Call poll()
        int rv = poll(poll_args.data(), (nfds_t)sizeof(poll_args), -1);

        if(rv < 0 && errno == EINTR) {
            continue;
        }
        if(rv < 0) {
            xslog::error("poll()");
            abort();
        }

        // Accept connetions on the listening socket
        if(poll_args[0].revents) {
            if(conn* c = accept_connection(server_sock_fd)) {
                if(fd_to_conn.size() <= (size_t)c->fd) {
                    fd_to_conn.resize(c->fd + 1);
                }
                fd_to_conn[c->fd] = c;
            }            
        }

        // Handle connection sockets that want to read/write/cloes
        for(size_t i = 1; i < poll_args.size(); i++) {
            if(poll_args[i].revents == 0) {
                continue;
            }
            conn* c = fd_to_conn[poll_args[i].fd];
            if(poll_args[i].revents & POLLIN) {
                handle_read(c);
            }   
            if(poll_args[i].revents & POLLOUT) {
                handle_write(c);
            }
            if(poll_args[i].revents & POLLERR || c->want_close) {
                close(c->fd);
                fd_to_conn[c->fd] = nullptr;
                delete c;
            }
        }
    }
    return 0;
}
