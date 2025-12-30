#ifndef CONN_H
#define CONN_H

#include <vector>

struct conn {
    int fd = -1;
    bool want_read = false;
    bool want_write = false;
    bool want_close = false;
    std::vector<uint8_t> incoming;
    std::vector<uint8_t> outgoing;
}

#endif // CONN_H