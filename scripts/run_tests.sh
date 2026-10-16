#!/usr/bin/env bash

set -e

PROJECT_ROOT="$(cd "$(dirname "$0")/.." && pwd)"

cd "$PROJECT_ROOT"

echo "========================================"
echo "Building ForgeDB and tests"
echo "========================================"

cmake -S . -B build
cmake --build build

echo
echo "========================================"
echo "Running ForgeDB tests"
echo "========================================"

TESTS=(
    test_crc32
    test_protocol
    test_parser
    test_sstable
    test_restart
    test_recovery
    test_compaction
    test_concurrency
    crash_after_write
    crash_during_recovery
    crash_during_compaction
)

for test in "${TESTS[@]}"; do
    echo
    echo "----------------------------------------"
    echo "Running: $test"
    echo "----------------------------------------"

    "./build/$test"
done

echo
echo "========================================"
echo "All ForgeDB tests passed successfully."
echo "========================================"
