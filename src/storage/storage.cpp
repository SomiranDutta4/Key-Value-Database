#include "forgedb/storage.hpp"

#include <filesystem>
#include <memory>
#include <unordered_map>
#include <vector>

#include "forgedb/manifest.hpp"
#include "forgedb/memtable.hpp"
#include "forgedb/recovery.hpp"
#include "forgedb/wal.hpp"

namespace forgedb {

Storage::Storage(
    const std::string& wal_filename,
    DurabilityMode durability_mode
)
    : wal_filename_(wal_filename),
      durability_mode_(durability_mode) {

    std::filesystem::path wal_path(
        wal_filename_
    );

    std::filesystem::path data_directory =
        wal_path.parent_path().parent_path();

    if (data_directory.empty()) {
        data_directory = "data";
    }

    tables_directory_ =
        (data_directory / "tables").string();

    manifest_filename_ =
        (data_directory / "MANIFEST").string();
}

Storage::~Storage() = default;

bool Storage::initialize(
    std::string& error
) {
    if (!ensure_directories(error)) {
        return false;
    }

    wal_ = std::make_unique<Wal>(
        wal_filename_,
        durability_mode_
    );

    manifest_ = std::make_unique<Manifest>(
        manifest_filename_
    );

    if (!manifest_->load(error)) {
        return false;
    }

    /*
     * Verify that every SSTable referenced by the
     * MANIFEST actually exists.
     *
     * A crash during compaction can leave the MANIFEST
     * referring to a newly created replacement table
     * that was never fully written.
     */
    bool missing_table = false;

    for (const auto& table :
         manifest_->tables()) {

        if (!std::filesystem::exists(
                table.filename
            )) {
            missing_table = true;
            break;
        }
    }

    if (missing_table) {
        if (!manifest_->rebuild_from_directory(
                tables_directory_,
                error
            )) {
            return false;
        }
    }

    return true;
}

bool Storage::ensure_directories(
    std::string& error
) {
    try {
        std::filesystem::path wal_path(
            wal_filename_
        );

        if (!wal_path.parent_path().empty()) {
            std::filesystem::create_directories(
                wal_path.parent_path()
            );
        }

        std::filesystem::create_directories(
            tables_directory_
        );

        std::filesystem::path manifest_path(
            manifest_filename_
        );

        if (!manifest_path.parent_path().empty()) {
            std::filesystem::create_directories(
                manifest_path.parent_path()
            );
        }

        return true;

    } catch (
        const std::filesystem::filesystem_error& e
    ) {
        error =
            std::string(
                "Failed to create storage directories: "
            ) +
            e.what();

        return false;
    }
}

bool Storage::append_put(
    const std::string& key,
    const std::string& value
) {
    return wal_ &&
           wal_->append_put(
               key,
               value
           );
}

bool Storage::append_delete(
    const std::string& key
) {
    return wal_ &&
           wal_->append_delete(key);
}

bool Storage::load(
    std::unordered_map<
        std::string,
        MemTableEntry
    >& data,
    std::string& error
) {
    if (!wal_ || !manifest_) {
        error =
            "Storage is not initialized";

        return false;
    }

    data.clear();

    /*
     * Load SSTables from oldest to newest.
     *
     * Newer entries overwrite older entries.
     * This includes tombstones.
     */
    for (const auto& table :
         manifest_->tables()) {

        std::unordered_map<
            std::string,
            MemTableEntry
        > table_data;

        if (!SSTable::load(
                table,
                table_data,
                error
            )) {
            return false;
        }

        for (auto& [key, entry] :
             table_data) {
            data[key] = std::move(entry);
        }
    }

    /*
     * WAL contains the newest operations because it
     * represents writes after the latest flush.
     */
    std::unordered_map<
        std::string,
        MemTableEntry
    > wal_data;

    RecoveryResult result =
        recover_wal(
            wal_->filename(),
            wal_data
        );

    switch (result.status) {
    case RecoveryStatus::SUCCESS:
    case RecoveryStatus::RECOVERED_INCOMPLETE_TAIL:
        break;

    case RecoveryStatus::CORRUPTED:
    case RecoveryStatus::IO_ERROR:
        error = result.message;
        return false;
    }

    /*
     * WAL is newer than every SSTable that existed
     * before the current active WAL.
     *
     * Therefore WAL state overrides SSTable state,
     * including DELETE tombstones.
     */
    for (auto& [key, entry] : wal_data) {
        data[key] = std::move(entry);
    }

    return true;
}

bool Storage::flush_memtable(
    const std::unordered_map<
        std::string,
        MemTableEntry
    >& data,
    std::string& error
) {
    if (!manifest_ || !wal_) {
        error =
            "Storage is not initialized";

        return false;
    }

    if (data.empty()) {
        return true;
    }

    uint64_t table_id =
        manifest_->next_table_id();

    std::string filename =
        table_filename(table_id);

    SSTableMetadata metadata;

    /*
     * 1. Create the SSTable.
     */
    if (!SSTable::write(
            filename,
            table_id,
            data,
            metadata,
            error
        )) {
        return false;
    }

    /*
     * 2. Publish it through MANIFEST.
     */
    if (!manifest_->add_table(
            metadata,
            error
        )) {
        std::error_code ignored;

        std::filesystem::remove(
            filename,
            ignored
        );

        return false;
    }

    /*
     * 3. Now the data is safely represented by an
     * SSTable, so the WAL can be reset.
     */
    if (!reset_wal(error)) {
        return false;
    }

    return true;
}

bool Storage::reset_wal(
    std::string& error
) {
    if (!wal_) {
        error =
            "WAL is not initialized";

        return false;
    }

    if (!wal_->truncate(0)) {
        error =
            "Failed to reset WAL";

        return false;
    }

    return true;
}

bool Storage::get_from_sstables(
    const std::string& key,
    std::string& value,
    bool& found,
    bool& deleted,
    std::string& error
) {
    found = false;
    deleted = false;

    if (!manifest_) {
        error =
            "Storage is not initialized";

        return false;
    }

    const auto& tables =
        manifest_->tables();

    /*
     * Search newest table first.
     */
    for (auto it = tables.rbegin();
         it != tables.rend();
         ++it) {

        bool table_found = false;
        bool table_deleted = false;

        if (!SSTable::get(
                *it,
                key,
                value,
                table_found,
                table_deleted,
                error
            )) {
            return false;
        }

        if (table_found) {
            found = true;
            deleted = table_deleted;

            /*
             * Extremely important:
             *
             * If this is a tombstone, stop here.
             * Do NOT continue to older SSTables.
             */
            return true;
        }
    }

    return true;
}

bool Storage::compact(
    std::string& error
) {
    if (!manifest_) {
        error =
            "Storage is not initialized";

        return false;
    }

    const auto& tables =
        manifest_->tables();

    if (tables.size() < 2) {
        return true;
    }

    /*
     * Merge oldest → newest.
     *
     * Newer entries overwrite older entries,
     * including tombstones.
     */
    std::unordered_map<
        std::string,
        MemTableEntry
    > merged;

    for (const auto& table : tables) {
        std::unordered_map<
            std::string,
            MemTableEntry
        > table_data;

        if (!SSTable::load(
                table,
                table_data,
                error
            )) {
            return false;
        }

        for (auto& [key, entry] :
             table_data) {
            merged[key] = std::move(entry);
        }
    }

    /*
     * Since we are merging ALL existing tables,
     * tombstones no longer need to be kept.
     *
     * There are no older tables left where an old
     * value could reappear from.
     */
    for (auto it = merged.begin();
         it != merged.end();) {

        if (it->second.deleted) {
            it = merged.erase(it);
        } else {
            ++it;
        }
    }

    uint64_t new_table_id =
        manifest_->next_table_id();

    std::string new_filename =
        table_filename(new_table_id);

    SSTableMetadata new_metadata;

    /*
     * Create replacement table first.
     */
    if (!SSTable::write(
            new_filename,
            new_table_id,
            merged,
            new_metadata,
            error
        )) {
        return false;
    }

    std::vector<uint64_t> old_ids;
    old_ids.reserve(tables.size());

    for (const auto& table : tables) {
        old_ids.push_back(table.id);
    }

    /*
     * Publish new table.
     */
    if (!manifest_->add_table(
            new_metadata,
            error
        )) {
        std::error_code ignored;

        std::filesystem::remove(
            new_filename,
            ignored
        );

        return false;
    }

    /*
     * Remove old tables from MANIFEST.
     */
    if (!manifest_->remove_tables(
            old_ids,
            error
        )) {
        return false;
    }

    /*
     * Finally remove old physical files.
     */
    for (const auto& table : tables) {
        std::error_code ignored;

        std::filesystem::remove(
            table.filename,
            ignored
        );
    }

    return true;
}

std::size_t Storage::table_count() const {
    if (!manifest_) {
        return 0;
    }

    return manifest_->tables().size();
}

bool Storage::sync() {
    return wal_ &&
           wal_->sync();
}

uint64_t Storage::log_size() const {
    if (!wal_) {
        return 0;
    }

    return wal_->size();
}

std::string Storage::table_filename(
    uint64_t table_id
) const {
    return
        tables_directory_ +
        "/table_" +
        std::to_string(table_id) +
        ".sst";
}

} // namespace forgedb
