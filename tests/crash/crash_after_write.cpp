#include <cassert>
#include <filesystem>
#include <iostream>
#include <string>

#include "forgedb/database.hpp"
#include "forgedb/durability.hpp"

int main() {
    const std::string directory =
        "test_data_crash_after_write";

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

        forgedb::Command command;
        command.type =
            forgedb::CommandType::PUT;
        command.key = "important_key";
        command.value = "important_value";

        assert(
            database.execute(command) == "OK"
        );

        // Database object is destroyed here.
        // Its in-memory state disappears.
        // The WAL should preserve the write.
    }

    {
        forgedb::Database database(
            wal,
            forgedb::DurabilityMode::SYNC
        );

        std::string error;

        assert(database.initialize(error));

        forgedb::Command command;
        command.type =
            forgedb::CommandType::GET;
        command.key = "important_key";

        assert(
            database.execute(command) ==
            "important_value"
        );
    }

    std::filesystem::remove_all(directory);

    std::cout
        << "crash_after_write: PASSED\n";

    return 0;
}
