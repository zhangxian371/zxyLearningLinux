// Day 3 of the 前 10 天计划 — demonstrate dup2 to redirect stdout.
//
// Lesson: fd 1 is just an index into the process's fd table. Calling
// dup2(fd, 1) makes the fd-table entry for index 1 refer to the same open
// file description as `fd`. Anything subsequently written to fd 1 — including
// libc printf, which uses fd 1 — goes to the file, not the terminal.
//
// This program does *not* try to keep the terminal line working afterwards;
// it just shows the before/after transition.

#include <cerrno>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fcntl.h>
#include <string>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

namespace {

std::string sysmsg(const char* what) {
    return std::string(what) + ": " + std::strerror(errno) +
           " (errno=" + std::to_string(errno) + ")";
}

}  // namespace

int main(int argc, char* argv[]) {
    if (argc != 2) {
        std::fprintf(stderr, "usage: %s <output-file>\n", argv[0]);
        return 2;
    }
    const char* path = argv[1];

    int fd = ::open(path, O_WRONLY | O_CREAT | O_TRUNC, 0644);
    if (fd < 0) {
        std::fprintf(stderr, "open(%s): %s\n", path,
                     sysmsg("open").c_str());
        return 1;
    }

    // Line 1 goes to the *real* stdout (the terminal). Flush so it actually
    // reaches the terminal before we rewire fd 1.
    std::printf("[before dup2] stdout is fd 1; this line goes to the terminal\n");
    std::fflush(stdout);

    if (::dup2(fd, STDOUT_FILENO) < 0) {
        std::fprintf(stderr, "dup2: %s\n", sysmsg("dup2").c_str());
        ::close(fd);
        return 1;
    }

    // fd is now redundant — fd 1 already covers the same open file
    // description. Closing it avoids a leak.
    ::close(fd);

    // Line 2 still uses printf (which writes to fd 1), but fd 1 now points
    // to the file. Flush again so the data is on disk before we exit.
    std::printf("[after  dup2] this line goes to %s, not the terminal\n", path);
    std::fflush(stdout);

    return 0;
}
