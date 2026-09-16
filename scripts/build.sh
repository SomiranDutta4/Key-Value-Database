#!/usr/bin/env bash

set -e

PROJECT_ROOT="$(cd "$(dirname "$0")/.." && pwd)"

cd "$PROJECT_ROOT"

echo "========================================"
echo "Building ForgeDB"
echo "========================================"

cmake -S . -B build
cmake --build build

echo
echo "Build completed successfully."
