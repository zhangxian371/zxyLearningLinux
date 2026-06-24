#pragma once

#include <cstddef>
#include <string>
#include <vector>

namespace chat {

class Buffer {
public:
    std::size_t readable() const { return data_.size() - read_pos_; }
    bool empty() const { return readable() == 0; }
    const char* peek() const { return data_.data() + read_pos_; }

    void append(const char* data, std::size_t n);
    void append(const std::string& s) { append(s.data(), s.size()); }
    void consume(std::size_t n);
    std::string take(std::size_t n);
    std::size_t capacity_bytes() const { return data_.size(); }

private:
    void compact_if_needed();

    std::vector<char> data_;
    std::size_t read_pos_{0};
};

}  // namespace chat

