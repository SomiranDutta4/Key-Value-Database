#include "forgedb/sstable.hpp"

#include <algorithm>
#include <fstream>
#include <vector>

#include "forgedb/crc32.hpp"
#include "forgedb/serialization.hpp"

namespace forgedb {

namespace {

constexpr uint32_t SSTABLE_MAGIC = 0x46444231;
constexpr uint32_t SSTABLE_VERSION = 2;

struct Entry {
    std::string key;
    MemTableEntry entry;
};

std::string serialize_entries(
    const std::vector<Entry>& entries
) {
    std::string buffer;

    for (const auto& item : entries) {
        append_uint32(
            buffer,
            static_cast<uint32_t>(
                item.key.size()
            )
        );

        buffer.append(item.key);

        buffer.push_back(
            item.entry.deleted ? 1 : 0
        );

        append_uint32(
            buffer,
            static_cast<uint32_t>(
                item.entry.value.size()
            )
        );

        buffer.append(
            item.entry.value
        );
    }

    return buffer;
}

bool deserialize_entries(
    const std::string& buffer,
    uint64_t entry_count,
    std::unordered_map<
        std::string,
        MemTableEntry
    >& data
) {
    std::size_t offset = 0;

    for (uint64_t i = 0;
         i < entry_count;
         ++i) {
        uint32_t key_size = 0;
        uint32_t value_size = 0;

        if (!read_uint32(
                buffer,
                offset,
                key_size
            )) {
            return false;
        }

        if (key_size >
            buffer.size() - offset) {
            return false;
        }

        std::string key =
            buffer.substr(
                offset,
                key_size
            );

        offset += key_size;

        if (offset >= buffer.size()) {
            return false;
        }

        bool deleted =
            buffer[offset++] != 0;

        if (!read_uint32(
                buffer,
                offset,
                value_size
            )) {
            return false;
        }

        if (value_size >
            buffer.size() - offset) {
            return false;
        }

        std::string value =
            buffer.substr(
                offset,
                value_size
            );

        offset += value_size;

        data[std::move(key)] =
            MemTableEntry{
                std::move(value),
                deleted
            };
    }

    return offset == buffer.size();
}

bool read_file(
    const std::string& filename,
    std::string& contents,
    std::string& error
) {
    std::ifstream file(
        filename,
        std::ios::binary
    );

    if (!file) {
        error =
            "Failed to open SSTable: " +
            filename;
        return false;
    }

    file.seekg(0, std::ios::end);

    std::streamoff size =
        file.tellg();

    if (size < 0) {
        error =
            "Failed to determine SSTable size";
        return false;
    }

    file.seekg(0, std::ios::beg);

    contents.resize(
        static_cast<std::size_t>(size)
    );

    if (size > 0) {
        file.read(
            contents.data(),
            size
        );

        if (!file) {
            error =
                "Failed to read SSTable";
            return false;
        }
    }

    return true;
}

bool parse_table(
    const std::string& contents,
    std::unordered_map<
        std::string,
        MemTableEntry
    >& data,
    std::string& error
) {
    std::size_t offset = 0;

    uint32_t magic = 0;
    uint32_t version = 0;
    uint64_t entry_count = 0;

    if (!read_uint32(
            contents,
            offset,
            magic
        ) ||
        !read_uint32(
            contents,
            offset,
            version
        ) ||
        !read_uint64(
            contents,
            offset,
            entry_count
        )) {
        error =
            "SSTable header is incomplete";
        return false;
    }

    if (magic != SSTABLE_MAGIC) {
        error =
            "Invalid SSTable magic";
        return false;
    }

    if (version != SSTABLE_VERSION) {
        error =
            "Unsupported SSTable version";
        return false;
    }

    if (contents.size() <
        offset + sizeof(uint32_t)) {
        error =
            "SSTable checksum is missing";
        return false;
    }

    std::size_t checksum_offset =
        contents.size() -
        sizeof(uint32_t);

    uint32_t stored_checksum = 0;

    std::size_t checksum_read_offset =
        checksum_offset;

    if (!read_uint32(
            contents,
            checksum_read_offset,
            stored_checksum
        )) {
        error =
            "Failed to read SSTable checksum";
        return false;
    }

    std::string payload =
        contents.substr(
            offset,
            checksum_offset - offset
        );

    uint32_t calculated_checksum =
        calculate_crc32(
            payload.data(),
            payload.size()
        );

    if (stored_checksum !=
        calculated_checksum) {
        error =
            "SSTable checksum mismatch";
        return false;
    }

    data.clear();

    if (!deserialize_entries(
            payload,
            entry_count,
            data
        )) {
        error =
            "Invalid SSTable entry data";
        return false;
    }

    return true;
}

} // namespace

bool SSTable::write(
    const std::string& filename,
    uint64_t id,
    const std::unordered_map<
        std::string,
        MemTableEntry
    >& data,
    SSTableMetadata& metadata,
    std::string& error
) {
    std::vector<Entry> entries;
    entries.reserve(data.size());

    for (const auto& [key, entry] : data) {
        entries.push_back(
            {key, entry}
        );
    }

    std::sort(
        entries.begin(),
        entries.end(),
        [](const Entry& a,
           const Entry& b) {
            return a.key < b.key;
        }
    );

    std::string payload =
        serialize_entries(entries);

    uint32_t checksum =
        calculate_crc32(
            payload.data(),
            payload.size()
        );

    std::string contents;

    append_uint32(
        contents,
        SSTABLE_MAGIC
    );

    append_uint32(
        contents,
        SSTABLE_VERSION
    );

    append_uint64(
        contents,
        static_cast<uint64_t>(
            entries.size()
        )
    );

    contents.append(payload);

    append_uint32(
        contents,
        checksum
    );

    std::ofstream file(
        filename,
        std::ios::binary |
            std::ios::trunc
    );

    if (!file) {
        error =
            "Failed to create SSTable: " +
            filename;
        return false;
    }

    file.write(
        contents.data(),
        static_cast<std::streamsize>(
            contents.size()
        )
    );

    file.flush();

    if (!file) {
        error =
            "Failed to write SSTable";
        return false;
    }

    metadata.id = id;
    metadata.filename = filename;

    metadata.entry_count =
        static_cast<uint64_t>(
            entries.size()
        );

    if (!entries.empty()) {
        metadata.smallest_key =
            entries.front().key;

        metadata.largest_key =
            entries.back().key;
    } else {
        metadata.smallest_key.clear();
        metadata.largest_key.clear();
    }

    return true;
}

bool SSTable::load(
    const SSTableMetadata& metadata,
    std::unordered_map<
        std::string,
        MemTableEntry
    >& data,
    std::string& error
) {
    std::string contents;

    if (!read_file(
            metadata.filename,
            contents,
            error
        )) {
        return false;
    }

    return parse_table(
        contents,
        data,
        error
    );
}

bool SSTable::get(
    const SSTableMetadata& metadata,
    const std::string& key,
    std::string& value,
    bool& found,
    bool& deleted,
    std::string& error
) {
    found = false;
    deleted = false;

    if (metadata.entry_count == 0) {
        return true;
    }

    if (key < metadata.smallest_key ||
        key > metadata.largest_key) {
        return true;
    }

    std::unordered_map<
        std::string,
        MemTableEntry
    > data;

    if (!load(
            metadata,
            data,
            error
        )) {
        return false;
    }

    auto it = data.find(key);

    if (it == data.end()) {
        return true;
    }

    found = true;
    deleted = it->second.deleted;

    if (!deleted) {
        value = it->second.value;
    } else {
        value.clear();
    }

    return true;
}

} // namespace forgedb
