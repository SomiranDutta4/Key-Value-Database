#include <iostream>
#include <string>

#include "forgedb/database.hpp"
#include "forgedb/durability.hpp"
#include "forgedb/server.hpp"

int main() {
    constexpr int PORT = 8080;

    /*
     * The WAL is the first durable layer.
     *
     * SYNC mode means ForgeDB attempts to flush the
     * WAL after every write operation.
     */
    const std::string wal_filename =
        "data/wal/active.log";

    forgedb::Database database(
        wal_filename,
        forgedb::DurabilityMode::SYNC
    );

    std::string error;

    /*
     * Startup recovery:
     *
     * 1. Create storage directories.
     * 2. Load MANIFEST.
     * 3. Load SSTables.
     * 4. Replay WAL.
     * 5. Reconstruct the MemTable.
     */
    if (!database.initialize(error)) {
        std::cerr
            << "Failed to initialize ForgeDB: "
            << error
            << "\n";

        return 1;
    }

    forgedb::Server server(
        database,
        PORT
    );

    /*
     * Server initialization:
     *
     * socket()
     *   ↓
     * bind()
     *   ↓
     * listen()
     *   ↓
     * non-blocking mode
     *   ↓
     * register listening socket with kqueue
     */
    if (!server.initialize(error)) {
        std::cerr
            << "Failed to initialize server: "
            << error
            << "\n";

        return 1;
    }

    std::cout
        << "ForgeDB started successfully\n";

    /*
     * This enters the event loop.
     *
     * From here:
     *
     * OS event
     *   ↓
     * EventLoop
     *   ↓
     * Server / Connection callback
     *   ↓
     * Protocol
     *   ↓
     * Parser
     *   ↓
     * Database
     *   ↓
     * response
     */
    server.run();

    return 0;
}
