#include "forgedb/database.hpp"

#include <string>
#include <unordered_map>

#include "forgedb/compaction.hpp"
#include "forgedb/memtable.hpp"
#include "forgedb/storage.hpp"

namespace forgedb {

Database::Database(
    const std::string& wal_filename,
    DurabilityMode durability_mode
)
    : memtable_(
          std::make_unique<MemTable>()
      ),
      storage_(
          std::make_unique<Storage>(
              wal_filename,
              durability_mode
          )
      ) {
}

Database::~Database() = default;

bool Database::initialize(
    std::string& error
) {
    if (!memtable_ || !storage_) {
        error =
            "Database is not initialized";

        return false;
    }

    if (!storage_->initialize(error)) {
        return false;
    }

    std::unordered_map<
        std::string,
        MemTableEntry
    > recovered_data;

    if (!storage_->load(
            recovered_data,
            error
        )) {
        return false;
    }

    memtable_->replace(
        std::move(recovered_data)
    );

    compaction_ =
        std::make_unique<Compaction>(
            *storage_
        );

    return true;
}

std::string Database::execute(
    const Command& command
) {
    std::lock_guard<std::mutex> lock(
        mutex_
    );

    switch (command.type) {
    case CommandType::PUT:
        return execute_put(command);

    case CommandType::GET:
        return execute_get(command);

    case CommandType::DELETE_KEY:
        return execute_delete(command);

    case CommandType::EXIT:
        return "BYE";

    case CommandType::INVALID:
        return "ERROR invalid command";
    }

    return "ERROR unknown command";
}

std::string Database::execute_put(
    const Command& command
) {
    /*
     * WAL first.
     */
    if (!storage_->append_put(
            command.key,
            command.value
        )) {
        return "ERROR failed to write WAL";
    }

    memtable_->put(
        command.key,
        command.value
    );

    if (memtable_->size_in_bytes() >=
        MEMTABLE_FLUSH_THRESHOLD) {

        std::string error;

        if (!flush_memtable(error)) {
            return
                "ERROR flush failed: " +
                error;
        }

        if (!maybe_compact(error)) {
            return
                "ERROR compaction failed: " +
                error;
        }
    }

    return "OK";
}

std::string Database::execute_get(
    const Command& command
) {
    std::string value;
    bool deleted = false;
    // first check the memtable
    bool found_in_memtable =
        memtable_->get(
            command.key,
            value,
            deleted
        );

    if (found_in_memtable) {
        if (deleted) {
            return "NOT_FOUND";
        }

        return value;
    }

    bool found = false;
    std::string error;

    if (!storage_->get_from_sstables(
            command.key,
            value,
            found,
            deleted,
            error
        )) {
        return
            "ERROR read failed: " +
            error;
    }

    if (!found || deleted) {
        return "NOT_FOUND";
    }

    return value;
}

std::string Database::execute_delete(
    const Command& command
) {
    /*
     * WAL first, then tombstone in MemTable.
     */
    if (!storage_->append_delete(
            command.key
        )) {
        return "ERROR failed to write WAL";
    }

    memtable_->erase(
        command.key
    );

    if (memtable_->size_in_bytes() >=
        MEMTABLE_FLUSH_THRESHOLD) {

        std::string error;

        if (!flush_memtable(error)) {
            return
                "ERROR flush failed: " +
                error;
        }

        if (!maybe_compact(error)) {
            return
                "ERROR compaction failed: " +
                error;
        }
    }

    return "OK";
}

bool Database::flush_memtable(
    std::string& error
) {
    auto snapshot =
        memtable_->snapshot();

    if (snapshot.empty()) {
        return true;
    }

    if (!storage_->flush_memtable(
            snapshot,
            error
        )) {
        return false;
    }

    memtable_->clear();

    return true;
}

bool Database::maybe_compact(
    std::string& error
) {
    if (!compaction_) {
        return true;
    }

    if (!compaction_->should_compact()) {
        return true;
    }

    return compaction_->run(error);
}

} // namespace forgedb
