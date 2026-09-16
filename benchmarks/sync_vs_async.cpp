#include <chrono>
#include <filesystem>
#include <iomanip>
#include <iostream>
#include <string>

#include "forgedb/database.hpp"
#include "forgedb/durability.hpp"

namespace {

constexpr int OPERATION_COUNT = 5000;

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

struct BenchmarkResult {
    double seconds = 0.0;
    double operations_per_second = 0.0;
    double average_microseconds = 0.0;
};

BenchmarkResult run_benchmark(
    forgedb::DurabilityMode mode,
    const std::string& directory
) {
    const std::string wal =
        directory + "/wal/active.log";

    std::filesystem::remove_all(directory);

    forgedb::Database database(
        wal,
        mode
    );

    std::string error;

    if (!database.initialize(error)) {
        std::cerr
            << "Failed to initialize database: "
            << error
            << '\n';

        std::exit(1);
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
                << "Write failed\n";

            std::exit(1);
        }
    }

    const auto end =
        std::chrono::steady_clock::now();

    const std::chrono::duration<double>
        elapsed = end - start;

    const double seconds =
        elapsed.count();

    BenchmarkResult result;

    result.seconds = seconds;

    result.operations_per_second =
        static_cast<double>(
            OPERATION_COUNT
        ) / seconds;

    result.average_microseconds =
        (seconds * 1'000'000.0) /
        OPERATION_COUNT;

    return result;
}

void print_result(
    const std::string& name,
    const BenchmarkResult& result
) {
    std::cout
        << name
        << '\n';

    std::cout
        << "  Total time: "
        << result.seconds
        << " seconds\n";

    std::cout
        << "  Throughput: "
        << result.operations_per_second
        << " writes/second\n";

    std::cout
        << "  Average latency: "
        << result.average_microseconds
        << " microseconds/write\n\n";
}

} // namespace

int main() {
    std::cout
        << std::fixed
        << std::setprecision(2);

    std::cout
        << "\n=== ForgeDB SYNC vs ASYNC Benchmark ===\n\n";

    const BenchmarkResult async_result =
        run_benchmark(
            forgedb::DurabilityMode::ASYNC,
            "benchmark_data_async"
        );

    const BenchmarkResult sync_result =
        run_benchmark(
            forgedb::DurabilityMode::SYNC,
            "benchmark_data_sync"
        );

    print_result(
        "ASYNC mode:",
        async_result
    );

    print_result(
        "SYNC mode:",
        sync_result
    );

    const double throughput_ratio =
        async_result.operations_per_second /
        sync_result.operations_per_second;

    std::cout
        << "ASYNC / SYNC throughput ratio: "
        << throughput_ratio
        << "x\n";

    std::filesystem::remove_all(
        "benchmark_data_async"
    );

    std::filesystem::remove_all(
        "benchmark_data_sync"
    );

    return 0;
}
