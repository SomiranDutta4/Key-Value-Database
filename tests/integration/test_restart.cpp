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
        "test_data_restart";

    const std::string wal =
        directory + "/wal/active.log";

    std::filesystem::remove_all(directory);
    std::filesystem::create_directories(
        directory + "/wal"
    );

    {
        forgedb::Database database(
            wal,
            forgedb::DurabilityMode::SYNC
        );

        std::string error;

        assert(database.initialize(error));

        assert(
            database.execute(
                put_command("name", "Mohit")
            ) == "OK"
        );

        assert(
            database.execute(
                put_command("project", "ForgeDB")
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

        assert(
            database.execute(
                get_command("name")
            ) == "Mohit"
        );

        assert(
            database.execute(
                get_command("project")
            ) == "ForgeDB"
        );
    }

    std::filesystem::remove_all(directory);

    std::cout
        << "test_restart: PASSED\n";

    return 0;
}
