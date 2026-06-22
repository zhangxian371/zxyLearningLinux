// Day 3 of the 前 10 天计划 — file copy using only open/read/write/close.
//
// Lessons demonstrated:
//   * fd is a small per-process integer index, not the file itself.
//   * Every open() must be paired with a close(), on every code path.
//   * read() may return fewer bytes than requested (short read); loop until done.
//   * write() may write fewer bytes than given (short write); loop until done.
//   * Both calls may fail with EINTR when a signal arrives; the standard answer
//     is to retry, not to treat it as a real error.
//   * errno is only meaningful after a syscall reports failure; read it, then
//     turn it into a human message via strerror.
//
// Day 8 will revisit this code and replace the manual close()/error-path
// cleanup with an RAII UniqueFd wrapper. For Day 3 we keep it explicit so the
// fd lifecycle is visible.

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

constexpr std::size_t kBufSize = 64 * 1024;  // 64 KiB — common sweet spot

// Build a human-readable "what: reason (errno=N)" string for the *current* errno.
std::string sysmsg(const char* what) {
    return std::string(what) + ": " + std::strerror(errno) +
           " (errno=" + std::to_string(errno) + ")";
}

// Write all `n` bytes from `buf` to `fd`. Loops on short write and EINTR.
// Returns true on success, false on any real error.
bool write_all(int fd, const void* buf, std::size_t n) {
    const char* p = static_cast<const char*>(buf);
    std::size_t left = n;
    while (left > 0) {
        ssize_t w = ::write(fd, p, left);
        if (w < 0) {
            if (errno == EINTR) continue;            // signal arrived — retry
            std::fprintf(stderr, "write: %s\n", sysmsg("write").c_str());
            return false;
        }
        if (w == 0) {
            // write() returning 0 with n>0 should not happen for regular files,
            // but be defensive anyway.
            std::fprintf(stderr, "write: returned 0 with %zu bytes remaining\n", left);
            return false;
        }
        p += w;
        left -= static_cast<std::size_t>(w);
    }
    return true;
}

// Copy src_fd to dst_fd until EOF. Returns true on success.
bool copy_stream(int src_fd, int dst_fd) {
    char buf[kBufSize];
    for (;;) {
        ssize_t n = ::read(src_fd, buf, sizeof(buf));
        if (n < 0) {
            if (errno == EINTR) continue;            // signal arrived — retry
            std::fprintf(stderr, "read: %s\n", sysmsg("read").c_str());
            return false;
        }
        if (n == 0) return true;                     // clean EOF
        if (!write_all(dst_fd, buf, static_cast<std::size_t>(n))) return false;
    }
}

}  // namespace

int main(int argc, char* argv[]) {
    if (argc != 3) {
        std::fprintf(stderr, "usage: %s <src> <dst>\n", argv[0]);
        return 2;
    }
    const char* src_path = argv[1];
    const char* dst_path = argv[2];

    int src_fd = ::open(src_path, O_RDONLY);
    if (src_fd < 0) {
        std::fprintf(stderr, "open(%s): %s\n", src_path,
                     sysmsg("open src").c_str());
        return 1;
    }

    int dst_fd = ::open(dst_path, O_WRONLY | O_CREAT | O_TRUNC, 0644);
    if (dst_fd < 0) {
        std::fprintf(stderr, "open(%s): %s\n", dst_path,
                     sysmsg("open dst").c_str());
        ::close(src_fd);                              // (3) every error path closes what is open
        return 1;
    }

    bool ok = copy_stream(src_fd, dst_fd);

    // Always close, even on success. A failing close here is reported but does
    // not change the overall verdict beyond what the stream loop saw.
    if (::close(src_fd) < 0) {
        std::fprintf(stderr, "close src: %s\n", sysmsg("close src").c_str());
        ok = false;
    }
    if (::close(dst_fd) < 0) {
        std::fprintf(stderr, "close dst: %s\n", sysmsg("close dst").c_str());
        ok = false;
    }

    if (!ok) return 1;
    std::printf("copied %s -> %s\n", src_path, dst_path);
    return 0;
}
