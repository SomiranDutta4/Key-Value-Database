#include <cassert>
#include <filesystem>
#include <iostream>
#include <string>
#include <unordered_map>

#include "forgedb/memtable.hpp"
#include "forgedb/sstable.hpp"

int main() {
    const std::string test_directory =
        "test_data_sstable";

    const std::string filename =
        test_directory + "/table_000001.sst";

    std::filesystem::remove_all(
        test_directory
    );

    std::filesystem::create_directories(
        test_directory
    );

    std::unordered_map<
        std::string,
        forgedb::MemTableEntry
    > data;

    data["name"] = {
        "Mohit",
        false
    };

    data["project"] = {
        "ForgeDB",
        false
    };

    data["deleted_key"] = {
        "",
        true
    };

    forgedb::SSTableMetadata metadata;
    std::string error;

    bool success =
        forgedb::SSTable::write(
            filename,
            1,
            data,
            metadata,
            error
        );

    assert(success);
    assert(error.empty());

    assert(metadata.id == 1);
    assert(metadata.filename == filename);
    assert(metadata.entry_count == 3);

    // ----------------------------------------
    // Test loading the complete SSTable.
    // ----------------------------------------

    std::unordered_map<
        std::string,
        forgedb::MemTableEntry
    > loaded_data;

    success =
        forgedb::SSTable::load(
            metadata,
            loaded_data,
            error
        );

    assert(success);
    assert(error.empty());

    assert(loaded_data.size() == 3);

    assert(
        loaded_data["name"].value ==
        "Mohit"
    );

    assert(
        !loaded_data["name"].deleted
    );

    assert(
        loaded_data["project"].value ==
        "ForgeDB"
    );

    assert(
        !loaded_data["project"].deleted
    );

    assert(
        loaded_data["deleted_key"].deleted
    );

    // ----------------------------------------
    // Test point lookup.
    // ----------------------------------------

    {
        std::string value;
        bool found = false;
        bool deleted = false;

        success =
            forgedb::SSTable::get(
                metadata,
                "name",
                value,
                found,
                deleted,
                error
            );

        assert(success);
        assert(found);
        assert(!deleted);
        assert(value == "Mohit");
    }

    // ----------------------------------------
    // Test deleted key / tombstone.
    // ----------------------------------------

    {
        std::string value;
        bool found = false;
        bool deleted = false;

        success =
            forgedb::SSTable::get(
                metadata,
                "deleted_key",
                value,
                found,
                deleted,
                error
            );

        assert(success);
        assert(found);
        assert(deleted);
    }

    // ----------------------------------------
    // Test missing key.
    // ----------------------------------------

    {
        std::string value;
        bool found = false;
        bool deleted = false;

        success =
            forgedb::SSTable::get(
                metadata,
                "does_not_exist",
                value,
                found,
                deleted,
                error
            );

        assert(success);
        assert(!found);
    }

    std::filesystem::remove_all(
        test_directory
    );

    std::cout
        << "test_sstable: PASSED\n";

    return 0;
}
