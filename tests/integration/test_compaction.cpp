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
    command.type = forgedb::CommandType::PUT;
    command.key = key;
    command.value = value;

    return command;
}

forgedb::Command get_command(
    const std::string& key
) {
    forgedb::Command command;
    command.type = forgedb::CommandType::GET;
    command.key = key;

    return command;
}

} // namespace

int main() {
    const std::string directory =
        "test_data_compaction";

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

        // Enough writes to trigger multiple
        // MemTable flushes and eventually compaction.
        for (int round = 0;
             round < 5;
             ++round) {

            for (int key = 0;
                 key < 4;
                 ++key) {

                std::string name =
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

        assert(
            database.execute(
                get_command("key_4_3")
            ) == large_value
        );
    }

    std::filesystem::remove_all(directory);

    std::cout
        << "test_compaction: PASSED\n";

    return 0;
}
