#!/usr/bin/env bash

set -e

PROJECT_ROOT="$(cd "$(dirname "$0")/.." && pwd)"

cd "$PROJECT_ROOT"

echo "========================================"
echo "Cleaning ForgeDB generated files"
echo "========================================"

echo
echo "Removing build directory..."
rm -rf build

echo "Removing database runtime data..."
rm -rf data

echo "Removing benchmark runtime data..."
rm -rf benchmark_data_write
rm -rf benchmark_data_read
rm -rf benchmark_data_sync_async

echo
echo "Cleaning completed successfully."
echo
echo "The repository is now back to its"
echo "source-code-only state."
