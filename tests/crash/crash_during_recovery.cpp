#include <cassert>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>
#include <unordered_map>

#include "forgedb/durability.hpp"
#include "forgedb/recovery.hpp"
#include "forgedb/wal.hpp"

int main() {
    const std::string directory =
        "test_data_crash_during_recovery";

    const std::string wal_filename =
        directory + "/wal.log";

    std::filesystem::remove_all(directory);
    std::filesystem::create_directories(
        directory
    );

    {
        forgedb::Wal wal(
            wal_filename,
            forgedb::DurabilityMode::SYNC
        );

        assert(
            wal.append_put(
                "name",
                "Mohit"
            )
        );

        assert(
            wal.append_put(
                "project",
                "ForgeDB"
            )
        );

        assert(wal.sync());
    }

    // Simulate a crash in the middle of writing
    // another record by appending an incomplete tail.
    {
        std::ofstream file(
            wal_filename,
            std::ios::binary |
            std::ios::app
        );

        assert(file.is_open());

        const char partial_bytes[] = {
            0x01,
            0x02,
            0x03
        };

        file.write(
            partial_bytes,
            sizeof(partial_bytes)
        );
    }

    std::unordered_map<
        std::string,
        forgedb::MemTableEntry
    > recovered_data;

    forgedb::RecoveryResult result =
        forgedb::recover_wal(
            wal_filename,
            recovered_data
        );

    // A complete valid prefix must survive.
    assert(
        result.status ==
            forgedb::RecoveryStatus::SUCCESS ||
        result.status ==
            forgedb::RecoveryStatus::
                RECOVERED_INCOMPLETE_TAIL
    );

    assert(
        recovered_data["name"].value ==
        "Mohit"
    );

    assert(
        !recovered_data["name"].deleted
    );

    assert(
        recovered_data["project"].value ==
        "ForgeDB"
    );

    assert(
        !recovered_data["project"].deleted
    );

    std::filesystem::remove_all(directory);

    std::cout
        << "crash_during_recovery: PASSED\n";

    return 0;
}
