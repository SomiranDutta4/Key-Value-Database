#include <cassert>
#include <filesystem>
#include <iostream>
#include <string>

#include "forgedb/database.hpp"
#include "forgedb/durability.hpp"

namespace {

forgedb::Command make_command(
    forgedb::CommandType type,
    const std::string& key,
    const std::string& value = ""
) {
    forgedb::Command command;

    command.type = type;
    command.key = key;
    command.value = value;

    return command;
}

} // namespace

int main() {
    const std::string directory =
        "test_data_recovery";

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

        assert(
            database.execute(
                make_command(
                    forgedb::CommandType::PUT,
                    "name",
                    "Mohit"
                )
            ) == "OK"
        );

        assert(
            database.execute(
                make_command(
                    forgedb::CommandType::PUT,
                    "name",
                    "UpdatedMohit"
                )
            ) == "OK"
        );

        assert(
            database.execute(
                make_command(
                    forgedb::CommandType::DELETE_KEY,
                    "name"
                )
            ) == "OK"
        );

        assert(
            database.execute(
                make_command(
                    forgedb::CommandType::PUT,
                    "city",
                    "Delhi"
                )
            ) == "OK"
        );
    }

    {
        forgedb::Database database(
            wal,
            forgedb::DurabilityMode::SYNC
        );

        std::string error;

        assert(database.initialize(error));

        std::string deleted_result =
            database.execute(
                make_command(
                    forgedb::CommandType::GET,
                    "name"
                )
            );

        assert(
            deleted_result != "Mohit"
        );

        assert(
            deleted_result != "UpdatedMohit"
        );

        assert(
            database.execute(
                make_command(
                    forgedb::CommandType::GET,
                    "city"
                )
            ) == "Delhi"
        );
    }

    std::filesystem::remove_all(directory);

    std::cout
        << "test_recovery: PASSED\n";

    return 0;
}
