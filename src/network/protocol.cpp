#include "forgedb/protocol.hpp"

#include <cstdint>
#include<string>
#include "forgedb/serialization.hpp"

namespace forgedb {

std::string create_frame(
    const std::string& payload
) {
    if (payload.size() > MAX_MESSAGE_SIZE) {
        return {};
    }

    std::string frame;

    frame.reserve(
        sizeof(uint32_t) + payload.size()
    );

    append_uint32(
        frame,
        static_cast<uint32_t>(payload.size())
    );

    frame.append(payload);

    return frame;
}

bool try_extract_frame(
    std::string& input_buffer,
    std::string& payload
) {
    payload.clear();

    if (input_buffer.size() < sizeof(uint32_t)) {
        return false;
    }

    std::size_t offset = 0;
    uint32_t message_size = 0;

    if (!read_uint32(
            input_buffer,
            offset,
            message_size
        )) {
        return false;
    }

    if (message_size > MAX_MESSAGE_SIZE) {
        input_buffer.clear();
        return false;
    }

    const std::size_t frame_size =
        sizeof(uint32_t) +
        static_cast<std::size_t>(message_size);

    if (input_buffer.size() < frame_size) {
        return false;
    }

    payload.assign(
        input_buffer.data() + sizeof(uint32_t),
        message_size
    );

    input_buffer.erase(
        0,
        frame_size
    );

    return true;
}

} // namespace forgedb
