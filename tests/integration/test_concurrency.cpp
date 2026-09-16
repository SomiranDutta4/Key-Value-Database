#include <cassert>
#include <filesystem>
#include <iostream>
#include <string>
#include <thread>
#include <vector>

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
        "test_data_concurrency";

    const std::string wal =
        directory + "/wal/active.log";

    std::filesystem::remove_all(directory);

    forgedb::Database database(
        wal,
        forgedb::DurabilityMode::SYNC
    );

    std::string error;

    assert(database.initialize(error));

    constexpr int THREAD_COUNT = 8;
    constexpr int WRITES_PER_THREAD = 50;

    std::vector<std::thread> threads;

    for (int thread_id = 0;
         thread_id < THREAD_COUNT;
         ++thread_id) {

        threads.emplace_back(
            [&database, thread_id]() {

                for (int i = 0;
                     i < WRITES_PER_THREAD;
                     ++i) {

                    const std::string key =
                        "thread_" +
                        std::to_string(thread_id) +
                        "_key_" +
                        std::to_string(i);

                    const std::string value =
                        "value_" +
                        std::to_string(thread_id) +
                        "_" +
                        std::to_string(i);

                    const std::string result =
                        database.execute(
                            put_command(
                                key,
                                value
                            )
                        );

                    assert(result == "OK");
                }
            }
        );
    }

    for (auto& thread : threads) {
        thread.join();
    }

    for (int thread_id = 0;
         thread_id < THREAD_COUNT;
         ++thread_id) {

        const std::string key =
            "thread_" +
            std::to_string(thread_id) +
            "_key_0";

        const std::string expected =
            "value_" +
            std::to_string(thread_id) +
            "_0";

        assert(
            database.execute(
                get_command(key)
            ) == expected
        );
    }

    std::filesystem::remove_all(directory);

    std::cout
        << "test_concurrency: PASSED\n";

    return 0;
}
