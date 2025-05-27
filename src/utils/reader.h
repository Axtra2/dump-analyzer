#pragma once

#include <bit>
#include <cstddef>
#include <limits>
#include <span>
#include <stdexcept>

class R final {

public:
    R(const std::byte* begin, size_t size = std::numeric_limits<size_t>::max())
      : begin_(begin)
      , size_(size)
      , read_(0) {}

public:
    R(const R&)            = default;
    R& operator=(const R&) = default;
    R(R&&)                 = default;
    R& operator=(R&&)      = default;

public:
    template <typename T, std::endian E = std::endian::big> void read(T& v, size_t n) {
        if (n > sizeof(T)) {
            throw std::runtime_error("n must be <= sizeof(v)");
        }
        ensure(n);
        std::byte* dst = reinterpret_cast<std::byte*>(&v);
        if constexpr (std::endian::native == std::endian::big) {
            std::fill_n(dst, sizeof(T) - n, 0);
            dst += sizeof(T) - n;
        } else {
            std::fill_n(dst + n, sizeof(T) - n, std::byte{0});
        }
        if constexpr (std::endian::native == E) {
            std::copy_n(it(), n, dst);
        } else {
            std::reverse_copy(it(), it() + n, dst);
        }
        read_ += n;
    }

    template <std::endian E = std::endian::big> void read(auto& v) {
        const size_t n = sizeof(v);
        ensure(n);
        std::byte* dst = reinterpret_cast<std::byte*>(&v);
        if constexpr (std::endian::native == E) {
            std::copy_n(it(), n, dst);
        } else {
            std::reverse_copy(it(), it() + n, dst);
        }
        read_ += n;
    }

    template <typename T, std::endian E = std::endian::big> T read() {
        T v{0};
        read<E>(v);
        return v;
    }

    template <typename T, std::endian E = std::endian::big> T read(size_t n) {
        T v{0};
        read<T, E>(v, n);
        return v;
    }

    std::span<const std::byte> skip(size_t nbytes) {
        ensure(nbytes);
        std::span<const std::byte> skippedBytes(it(), nbytes);
        read_ += nbytes;
        return skippedBytes;
    }

    void reset() {
        read_ = 0;
    }

    bool eof() const {
        return read_ == size_;
    }

    const std::byte* it() const {
        return begin_ + read_;
    }

private:
    void ensure(size_t nbytes) {
        if (read_ > size_ || size_ - read_ < nbytes) {
            throw std::runtime_error("out of bounds read");
        }
    }

private:
    const std::byte* begin_;
    size_t           size_;
    size_t           read_;
};
