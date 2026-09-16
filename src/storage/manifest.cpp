#include "forgedb/manifest.hpp"
#include "forgedb/memtable.hpp"

#include <algorithm>
#include <cerrno>
#include <cstdio>
#include <fstream>
#include <set>
#include <filesystem>
#include <unordered_map>

#include <unistd.h>

namespace forgedb {

Manifest::Manifest(
    const std::string& filename
)
    : filename_(filename) {
}

bool Manifest::load(
    std::string& error
) {
    tables_.clear();
    next_table_id_ = 1;

    std::ifstream file(filename_);

    if (!file) {
        // No MANIFEST yet is valid for a new database.
        return true;
    }

    uint64_t table_count = 0;

    if (!(file >> next_table_id_)) {
        error =
            "Invalid MANIFEST header";
        return false;
    }

    if (!(file >> table_count)) {
        error =
            "Invalid MANIFEST table count";
        return false;
    }

    for (uint64_t i = 0;
         i < table_count;
         ++i) {
        SSTableMetadata metadata;

        if (!(file >> metadata.id >>
              metadata.filename >>
              metadata.smallest_key >>
              metadata.largest_key >>
              metadata.entry_count)) {
            error =
                "Invalid MANIFEST entry";
            return false;
        }

        tables_.push_back(
            std::move(metadata)
        );
    }

    if (!file.eof()) {
        std::string extra;

        if (file >> extra) {
            error =
                "Unexpected data in MANIFEST";
            return false;
        }
    }

    return true;
}

bool Manifest::add_table(
    const SSTableMetadata& table,
    std::string& error
) {
    tables_.push_back(table);

    if (table.id >= next_table_id_) {
        next_table_id_ =
            table.id + 1;
    }

    if (!persist(error)) {
        tables_.pop_back();

        if (tables_.empty()) {
            next_table_id_ = 1;
        }

        return false;
    }

    return true;
}

bool Manifest::remove_tables(
    const std::vector<uint64_t>& table_ids,
    std::string& error
) {
    std::set<uint64_t> ids(
        table_ids.begin(),
        table_ids.end()
    );

    auto old_tables = tables_;

    tables_.erase(
        std::remove_if(
            tables_.begin(),
            tables_.end(),
            [&ids](
                const SSTableMetadata& table
            ) {
                return ids.count(
                    table.id
                ) > 0;
            }
        ),
        tables_.end()
    );

    if (!persist(error)) {
        tables_ = std::move(old_tables);
        return false;
    }

    return true;
}

bool Manifest::rebuild_from_directory(
    const std::string& tables_directory,
    std::string& error
) {
    std::vector<
        SSTableMetadata
    > rebuilt_tables;

    uint64_t largest_id = 0;

    try {
        for (const auto& entry :
             std::filesystem::directory_iterator(
                 tables_directory
             )) {

            if (!entry.is_regular_file()) {
                continue;
            }

            if (entry.path().extension() !=
                ".sst") {
                continue;
            }

            SSTableMetadata metadata;

            /*
             * Extract the table ID from names like:
             *
             * table_5.sst
             */
            std::string filename =
                entry.path().filename().string();

            constexpr const char* prefix =
                "table_";

            if (filename.rfind(
                    prefix,
                    0
                ) != 0) {
                continue;
            }

            std::string id_string =
                filename.substr(
                    6,
                    filename.size() - 10
                );

            try {
                metadata.id =
                    std::stoull(id_string);
            } catch (...) {
                continue;
            }

            metadata.filename =
                entry.path().string();

            /*
             * Load the SSTable to reconstruct its
             * metadata.
             */
            std::unordered_map<
                std::string,
                MemTableEntry
            > data;

            std::string load_error;

            if (!SSTable::load(
                    metadata,
                    data,
                    load_error
                )) {
                continue;
            }

            if (data.empty()) {
                continue;
            }

            metadata.entry_count =
                data.size();

            for (const auto& [key, value] :
                 data) {

                if (metadata.smallest_key.empty() ||
                    key < metadata.smallest_key) {
                    metadata.smallest_key = key;
                }

                if (metadata.largest_key.empty() ||
                    key > metadata.largest_key) {
                    metadata.largest_key = key;
                }
            }

            rebuilt_tables.push_back(
                std::move(metadata)
            );

            largest_id =
                std::max(
                    largest_id,
                    rebuilt_tables.back().id
                );
        }

    } catch (
        const std::filesystem::filesystem_error& e
    ) {
        error =
            std::string(
                "Failed to rebuild MANIFEST: "
            ) +
            e.what();

        return false;
    }

    std::sort(
        rebuilt_tables.begin(),
        rebuilt_tables.end(),
        [](
            const SSTableMetadata& left,
            const SSTableMetadata& right
        ) {
            return left.id < right.id;
        }
    );

    tables_ =
        std::move(rebuilt_tables);

    next_table_id_ =
        largest_id + 1;

    if (next_table_id_ == 0) {
        next_table_id_ = 1;
    }

    return persist(error);
}

const std::vector<
    SSTableMetadata
>& Manifest::tables() const {
    return tables_;
}

uint64_t Manifest::next_table_id() const {
    return next_table_id_;
}

bool Manifest::persist(
    std::string& error
) {
    const std::string temporary =
        filename_ + ".tmp";

    {
        std::ofstream file(
            temporary,
            std::ios::trunc
        );

        if (!file) {
            error =
                "Failed to create temporary MANIFEST";
            return false;
        }

        file << next_table_id_ << '\n';
        file << tables_.size() << '\n';

        for (const auto& table :
             tables_) {
            file
                << table.id << ' '
                << table.filename << ' '
                << table.smallest_key << ' '
                << table.largest_key << ' '
                << table.entry_count
                << '\n';
        }

        file.flush();

        if (!file) {
            error =
                "Failed to write MANIFEST";
            return false;
        }
    }

    if (std::rename(
            temporary.c_str(),
            filename_.c_str()
        ) != 0) {
        error =
            "Failed to replace MANIFEST";
        return false;
    }

    return true;
}

} // namespace forgedb
