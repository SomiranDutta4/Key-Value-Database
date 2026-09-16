#include "forgedb/crc32.hpp"

namespace forgedb {

uint32_t calculate_crc32(
    const void* data,
    std::size_t size
) {
    static uint32_t table[256] = {};
    static bool initialized = false;

    if (!initialized) {
        for (uint32_t i = 0; i < 256; ++i) {
            uint32_t crc = i;

            for (int bit = 0; bit < 8; ++bit) {
                if (crc & 1) {
                    crc = (crc >> 1) ^ 0xEDB88320u;
                } else {
                    crc >>= 1;
                }
            }

            table[i] = crc;
        }

        initialized = true;
    }

    const auto* bytes =
        static_cast<const uint8_t*>(data);

    uint32_t crc = 0xFFFFFFFFu;

    for (std::size_t i = 0; i < size; ++i) {
        uint8_t index =
            static_cast<uint8_t>(
                (crc ^ bytes[i]) & 0xFFu
            );

        crc = (crc >> 8) ^ table[index];
    }

    return crc ^ 0xFFFFFFFFu;
}

} // namespace forgedb
