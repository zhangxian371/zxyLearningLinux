#pragma once

namespace chat {

bool set_nonblock(int fd);
bool set_cloexec(int fd);

}  // namespace chat

