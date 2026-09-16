#include <cassert>
#include <cstdint>
#include <iostream>
#include <string>

#include "forgedb/crc32.hpp"

int main() {
    {
        const std::string data = "";

        uint32_t crc =
            forgedb::calculate_crc32(
                data.data(),
                data.size()
            );

        assert(crc == 0x00000000U);
    }

    {
        const std::string data = "123456789";

        uint32_t crc =
            forgedb::calculate_crc32(
                data.data(),
                data.size()
            );

        // Standard CRC-32 test vector.
        assert(crc == 0xCBF43926U);
    }

    {
        const std::string data1 =
            "ForgeDB";

        const std::string data2 =
            "ForgeDB";

        uint32_t crc1 =
            forgedb::calculate_crc32(
                data1.data(),
                data1.size()
            );

        uint32_t crc2 =
            forgedb::calculate_crc32(
                data2.data(),
                data2.size()
            );

        assert(crc1 == crc2);
    }

    {
        const std::string data1 =
            "ForgeDB";

        const std::string data2 =
            "forgedb";

        uint32_t crc1 =
            forgedb::calculate_crc32(
                data1.data(),
                data1.size()
            );

        uint32_t crc2 =
            forgedb::calculate_crc32(
                data2.data(),
                data2.size()
            );

        assert(crc1 != crc2);
    }

    std::cout
        << "test_crc32: PASSED\n";

    return 0;
}
