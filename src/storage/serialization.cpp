#include "forgedb/serialization.hpp"

namespace forgedb {

void append_uint8(
    std::string& buffer,
    uint8_t value
) {
    buffer.push_back(
        static_cast<char>(value)
    );
}

void append_uint32(
    std::string& buffer,
    uint32_t value
) {
    for (int i = 0; i < 4; ++i) {
        buffer.push_back(
            static_cast<char>(
                (value >> (i * 8)) & 0xFFu
            )
        );
    }
}

void append_uint64(
    std::string& buffer,
    uint64_t value
) {
    for (int i = 0; i < 8; ++i) {
        buffer.push_back(
            static_cast<char>(
                (value >> (i * 8)) & 0xFFu
            )
        );
    }
}

bool read_uint8(
    const std::string& buffer,
    std::size_t& offset,
    uint8_t& value
) {
    if (offset + 1 > buffer.size()) {
        return false;
    }

    value = static_cast<uint8_t>(
        static_cast<unsigned char>(
            buffer[offset]
        )
    );

    ++offset;

    return true;
}

bool read_uint32(
    const std::string& buffer,
    std::size_t& offset,
    uint32_t& value
) {
    if (offset + 4 > buffer.size()) {
        return false;
    }

    value = 0;

    for (int i = 0; i < 4; ++i) {
        value |=
            static_cast<uint32_t>(
                static_cast<unsigned char>(
                    buffer[offset + i]
                )
            )
            << (i * 8);
    }

    offset += 4;

    return true;
}

bool read_uint64(
    const std::string& buffer,
    std::size_t& offset,
    uint64_t& value
) {
    if (offset + 8 > buffer.size()) {
        return false;
    }

    value = 0;

    for (int i = 0; i < 8; ++i) {
        value |=
            static_cast<uint64_t>(
                static_cast<unsigned char>(
                    buffer[offset + i]
                )
            )
            << (i * 8);
    }

    offset += 8;

    return true;
}

} // namespace forgedb
