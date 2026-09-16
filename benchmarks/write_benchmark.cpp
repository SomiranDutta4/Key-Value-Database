#include <chrono>
#include <filesystem>
#include <iomanip>
#include <iostream>
#include <string>

#include "forgedb/database.hpp"
#include "forgedb/durability.hpp"

namespace {

constexpr int OPERATION_COUNT = 10000;

forgedb::Command make_put_command(
    const std::string& key,
    const std::string& value
) {
    forgedb::Command command;

    command.type = forgedb::CommandType::PUT;
    command.key = key;
    command.value = value;

    return command;
}

} // namespace

int main() {
    const std::string directory =
        "benchmark_data_write";

    const std::string wal =
        directory + "/wal/active.log";

    std::filesystem::remove_all(directory);

    forgedb::Database database(
        wal,
        forgedb::DurabilityMode::ASYNC
    );

    std::string error;

    if (!database.initialize(error)) {
        std::cerr
            << "Failed to initialize database: "
            << error
            << '\n';

        return 1;
    }

    const std::string value =
        "benchmark_value";

    const auto start =
        std::chrono::steady_clock::now();

    for (int i = 0;
         i < OPERATION_COUNT;
         ++i) {

        const std::string key =
            "key_" +
            std::to_string(i);

        const std::string result =
            database.execute(
                make_put_command(
                    key,
                    value
                )
            );

        if (result != "OK") {
            std::cerr
                << "Write failed at operation "
                << i
                << '\n';

            std::cerr
                << "Database result: "
                << result
                << '\n';

            return 1;
        }
    }

    const auto end =
        std::chrono::steady_clock::now();

    const std::chrono::duration<double>
        elapsed = end - start;

    const double seconds =
        elapsed.count();

    const double operations_per_second =
        static_cast<double>(
            OPERATION_COUNT
        ) / seconds;

    const double average_microseconds =
        (seconds * 1'000'000.0) /
        OPERATION_COUNT;

    std::cout
        << std::fixed
        << std::setprecision(2);

    std::cout
        << "\n=== ForgeDB Write Benchmark ===\n\n";

    std::cout
        << "Operations: "
        << OPERATION_COUNT
        << '\n';

    std::cout
        << "Total time: "
        << seconds
        << " seconds\n";

    std::cout
        << "Throughput: "
        << operations_per_second
        << " writes/second\n";

    std::cout
        << "Average latency: "
        << average_microseconds
        << " microseconds/write\n";

    std::filesystem::remove_all(directory);

    return 0;
}
