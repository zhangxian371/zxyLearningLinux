#include "common/nonblock.h"

#include <fcntl.h>

namespace chat {

bool set_nonblock(int fd) {
    int flags = ::fcntl(fd, F_GETFL, 0);
    if (flags < 0) return false;
    return ::fcntl(fd, F_SETFL, flags | O_NONBLOCK) == 0;
}

bool set_cloexec(int fd) {
    int flags = ::fcntl(fd, F_GETFD, 0);
    if (flags < 0) return false;
    return ::fcntl(fd, F_SETFD, flags | FD_CLOEXEC) == 0;
}

}  // namespace chat

