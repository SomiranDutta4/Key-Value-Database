cmake_minimum_required(VERSION 3.16)

project(
    ForgeDB
    VERSION 1.0
    LANGUAGES CXX
)

set(
    CMAKE_CXX_STANDARD
    17
)

set(
    CMAKE_CXX_STANDARD_REQUIRED
    ON
)

set(
    CMAKE_CXX_EXTENSIONS
    OFF
)

# ============================================================
# Common source files
# ============================================================

set(
    FORGEDB_CORE_SOURCES

    # Networking
    src/network/protocol.cpp

    # Command
    src/command/parser.cpp
    src/command/command.cpp

    # Database
    src/database/database.cpp
    src/database/memtable.cpp
    src/database/concurrency.cpp

    # Storage
    src/storage/storage.cpp
    src/storage/wal.cpp
    src/storage/recovery.cpp
    src/storage/crc32.cpp
    src/storage/serialization.cpp
    src/storage/snapshot.cpp
    src/storage/sstable.cpp
    src/storage/compaction.cpp
    src/storage/manifest.cpp

    # Testing / fault injection support
    src/testing/fault_injection.cpp
)

# ============================================================
# ForgeDB Server
# ============================================================

add_executable(
    forgedb

    src/main.cpp

    src/network/server.cpp
    src/network/connection.cpp
    src/network/event_loop.cpp

    ${FORGEDB_CORE_SOURCES}
)

target_include_directories(
    forgedb
    PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_compile_options(
    forgedb
    PRIVATE
    -Wall
    -Wextra
    -Wpedantic
)

# ============================================================
# ForgeDB CLI
# ============================================================

add_executable(
    forgedb_cli

    client/main.cpp
    client/cli.cpp

    src/network/protocol.cpp
    src/storage/serialization.cpp
)

target_include_directories(
    forgedb_cli
    PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_compile_options(
    forgedb_cli
    PRIVATE
    -Wall
    -Wextra
    -Wpedantic
)

# ============================================================
# Tests
# ============================================================

enable_testing()

function(
    add_forgedb_test
    TEST_NAME
    TEST_SOURCE
)

    add_executable(
        ${TEST_NAME}
        ${TEST_SOURCE}
        ${FORGEDB_CORE_SOURCES}
    )

    target_include_directories(
        ${TEST_NAME}
        PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/include
    )

    target_compile_options(
        ${TEST_NAME}
        PRIVATE
        -Wall
        -Wextra
        -Wpedantic
    )

    add_test(
        NAME ${TEST_NAME}
        COMMAND ${TEST_NAME}
    )

endfunction()

# ============================================================
# Unit Tests
# ============================================================

add_forgedb_test(
    test_crc32
    tests/unit/test_crc32.cpp
)

add_forgedb_test(
    test_protocol
    tests/unit/test_protocol.cpp
)

add_forgedb_test(
    test_parser
    tests/unit/test_parser.cpp
)

add_forgedb_test(
    test_sstable
    tests/unit/test_sstable.cpp
)

# ============================================================
# Integration Tests
# ============================================================

add_forgedb_test(
    test_restart
    tests/integration/test_restart.cpp
)

add_forgedb_test(
    test_recovery
    tests/integration/test_recovery.cpp
)

add_forgedb_test(
    test_compaction
    tests/integration/test_compaction.cpp
)

add_forgedb_test(
    test_concurrency
    tests/integration/test_concurrency.cpp
)

# ============================================================
# Crash / Recovery Tests
# ============================================================

add_forgedb_test(
    crash_after_write
    tests/crash/crash_after_write.cpp
)

add_forgedb_test(
    crash_during_recovery
    tests/crash/crash_during_recovery.cpp
)

add_forgedb_test(
    crash_during_compaction
    tests/crash/crash_during_compaction.cpp
)

# ============================================================
# Benchmarks
# ============================================================

add_executable(
    write_benchmark
    benchmarks/write_benchmark.cpp
    ${FORGEDB_CORE_SOURCES}
)

add_executable(
    read_benchmark
    benchmarks/read_benchmark.cpp
    ${FORGEDB_CORE_SOURCES}
)

add_executable(
    sync_vs_async
    benchmarks/sync_vs_async.cpp
    ${FORGEDB_CORE_SOURCES}
)

foreach(
    BENCHMARK_TARGET
    write_benchmark
    read_benchmark
    sync_vs_async
)

    target_include_directories(
        ${BENCHMARK_TARGET}
        PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/include
    )

    target_compile_options(
        ${BENCHMARK_TARGET}
        PRIVATE
        -Wall
        -Wextra
        -Wpedantic
    )

endforeach()

# ============================================================
# Debug Configuration
# ============================================================

if (CMAKE_BUILD_TYPE STREQUAL "Debug")

    target_compile_options(
        forgedb
        PRIVATE
        -g
        -O0
    )

    target_compile_options(
        forgedb_cli
        PRIVATE
        -g
        -O0
    )

endif()
