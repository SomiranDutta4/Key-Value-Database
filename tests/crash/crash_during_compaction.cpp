#include <cassert>
#include <filesystem>
#include <iostream>
#include <string>

#include "forgedb/database.hpp"
#include "forgedb/durability.hpp"

namespace {

forgedb::Command put_command(
    const std::string& key,
    const std::string& value
) {
    forgedb::Command command;

    command.type =
        forgedb::CommandType::PUT;

    command.key = key;
    command.value = value;

    return command;
}

forgedb::Command get_command(
    const std::string& key
) {
    forgedb::Command command;

    command.type =
        forgedb::CommandType::GET;

    command.key = key;

    return command;
}

} // namespace

int main() {
    const std::string directory =
        "test_data_crash_during_compaction";

    const std::string wal =
        directory + "/wal/active.log";

    std::filesystem::remove_all(directory);

    {
        forgedb::Database database(
            wal,
            forgedb::DurabilityMode::SYNC
        );

        std::string error;

        assert(database.initialize(error));

        const std::string large_value(
            300 * 1024,
            'X'
        );

        // Create enough data to cause several
        // MemTable flushes and compaction attempts.
        for (int round = 0;
             round < 5;
             ++round) {

            for (int key = 0;
                 key < 4;
                 ++key) {

                const std::string name =
                    "key_" +
                    std::to_string(round) +
                    "_" +
                    std::to_string(key);

                assert(
                    database.execute(
                        put_command(
                            name,
                            large_value
                        )
                    ) == "OK"
                );
            }
        }
    }

    // Simulate a restart after storage activity.
    // The important property is that committed data
    // remains readable.
    {
        forgedb::Database database(
            wal,
            forgedb::DurabilityMode::SYNC
        );

        std::string error;

        if (!database.initialize(error)) {
            std::cerr
                << "Database initialization failed: "
                << error
                << '\n';

            return 1;
        }

        const std::string expected(
            300 * 1024,
            'X'
        );

        assert(
            database.execute(
                get_command("key_0_0")
            ) == expected
        );

        assert(
            database.execute(
                get_command("key_4_3")
            ) == expected
        );
    }

    std::filesystem::remove_all(directory);

    std::cout
        << "crash_during_compaction: PASSED\n";

    return 0;
}
