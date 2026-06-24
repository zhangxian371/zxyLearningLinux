#include "net/buffer.h"

#include <algorithm>
#include <stdexcept>

namespace chat {

void Buffer::append(const char* data, std::size_t n) {
    if (n == 0) return;
    compact_if_needed();
    data_.insert(data_.end(), data, data + n);
}

void Buffer::consume(std::size_t n) {
    if (n > readable()) throw std::out_of_range("buffer consume out of range");
    read_pos_ += n;
    compact_if_needed();
}

std::string Buffer::take(std::size_t n) {
    if (n > readable()) throw std::out_of_range("buffer take out of range");
    std::string out(peek(), n);
    consume(n);
    return out;
}

void Buffer::compact_if_needed() {
    if (read_pos_ == 0) return;
    if (read_pos_ == data_.size()) {
        data_.clear();
        read_pos_ = 0;
        return;
    }
    if (read_pos_ > 4096 && read_pos_ * 2 >= data_.size()) {
        std::move(data_.begin() + static_cast<std::ptrdiff_t>(read_pos_), data_.end(), data_.begin());
        data_.resize(data_.size() - read_pos_);
        read_pos_ = 0;
    }
}

}  // namespace chat

