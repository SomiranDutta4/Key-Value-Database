# ForgeDB

A lightweight, persistent key-value database built from scratch in **C++17**.

ForgeDB is a systems-focused project that demonstrates how a database handles **persistent storage, crash recovery, networking, concurrency, and compaction** without relying on an existing database engine.

## Features

- Persistent key-value storage
- `PUT`, `GET`, and `DELETE` operations
- In-memory **MemTable**
- **Write-Ahead Log (WAL)**
- Crash recovery
- Immutable **SSTables**
- **MANIFEST** metadata
- Automatic MemTable flushing
- SSTable compaction
- Tombstone-based deletion
- CRC32 corruption detection
- TCP server and CLI client
- Thread-safe database operations
- Unit, integration, concurrency, and crash tests
- Fault injection

## Architecture

```text
Client
  |
  v
TCP Server
  |
  v
Command Parser
  |
  v
Database
  |
  +--> MemTable
  |
  +--> WAL
  |
  +--> SSTables
  |
  +--> MANIFEST
  |
  +--> Compaction
```

### Data Flow

```text
Client Request
      |
      v
   WAL Write
      |
      v
   MemTable
      |
      v
   SSTable
      |
      v
  Compaction
```

The WAL provides durability and recovery, while SSTables provide persistent immutable storage. The MANIFEST tracks active SSTables, and compaction merges older tables while preserving the latest value for each key.

## Commands

```text
PUT <key> <value>
GET <key>
DELETE <key>
EXIT
```

Example:

```text
PUT name Mohit
GET name
DELETE name
```

## Build

### Requirements

- C++17-compatible compiler
- CMake 3.16+
- Bash / Unix-like environment

### Build

```bash
cmake -S . -B build
cmake --build build
```

### Run

Start the server:

```bash
./build/forgedb
```

In another terminal, start the CLI:

```bash
./build/forgedb_cli
```

## Tests

Run all tests:

```bash
ctest --test-dir build --output-on-failure
```

The test suite covers:

- Command parsing
- Network protocol
- SSTable storage
- Restart persistence
- WAL recovery
- Compaction
- Concurrency
- Crash recovery
- Fault injection

## Technical Concepts

ForgeDB demonstrates practical concepts in:

- **Database Systems:** WAL, MemTables, SSTables, LSM-style storage, tombstones, compaction
- **Operating Systems:** persistent storage, crash consistency, atomic file operations
- **Networking:** TCP client/server communication and request/response protocols
- **Concurrency:** mutexes and thread-safe shared state
- **C++:** RAII, smart pointers, move semantics, binary I/O
- **Software Engineering:** CMake, modular architecture, and automated testing

## Limitations

ForgeDB is an educational database implementation rather than a production system.

Current limitations include:

- No Bloom filters or SSTable indexes
- Single-level compaction
- Synchronous compaction
- Global database mutex
- No transactions
- No replication
- No authentication
- No range queries

## Purpose

ForgeDB was built to understand the internals of persistent storage engines and crash-consistent systems—from **WAL writes and in-memory storage to disk persistence, recovery, and compaction**.

## Author

**Somiran Dutta**
# Key-Value-Database

Made with collaboration with [Mohit Methi](https://github.com/mohitmethi1000)
