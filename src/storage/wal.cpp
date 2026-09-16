#include "forgedb/wal.hpp"

#include <filesystem>

#include "forgedb/crc32.hpp"
#include "forgedb/serialization.hpp"

namespace forgedb {

namespace {

constexpr uint32_t MAX_KEY_SIZE =
    16 * 1024 * 1024;

constexpr uint32_t MAX_VALUE_SIZE =
    16 * 1024 * 1024;

bool valid_record_type(
    uint8_t type
) {
    return type ==
               static_cast<uint8_t>(
                   WalRecordType::PUT
               ) ||
           type ==
               static_cast<uint8_t>(
                   WalRecordType::DELETE
               );
}

} // namespace

std::string serialize_wal_record(
    const WalRecord& record
) {
    std::string payload;

    /*
     * Logical format:
     *
     * [1 byte type]
     * [4 byte key length]
     * [key bytes]
     * [4 byte value length]
     * [value bytes]
     *
     * DELETE simply has value length = 0.
     */

    payload.push_back(
        static_cast<char>(record.type)
    );

    append_uint32(
        payload,
        static_cast<uint32_t>(
            record.key.size()
        )
    );

    payload.append(record.key);

    append_uint32(
        payload,
        static_cast<uint32_t>(
            record.value.size()
        )
    );

    payload.append(record.value);

    return payload;
}

bool deserialize_wal_record(
    const std::string& payload,
    WalRecord& record
) {
    if (payload.empty()) {
        return false;
    }

    std::size_t offset = 0;

    uint8_t raw_type =
        static_cast<uint8_t>(
            payload[offset++]
        );

    if (!valid_record_type(raw_type)) {
        return false;
    }

    uint32_t key_size = 0;

    if (!read_uint32(
            payload,
            offset,
            key_size
        )) {
        return false;
    }

    if (key_size > MAX_KEY_SIZE ||
        key_size >
            payload.size() - offset) {
        return false;
    }

    record.key =
        payload.substr(
            offset,
            key_size
        );

    offset += key_size;

    uint32_t value_size = 0;

    if (!read_uint32(
            payload,
            offset,
            value_size
        )) {
        return false;
    }

    if (value_size > MAX_VALUE_SIZE ||
        value_size >
            payload.size() - offset) {
        return false;
    }

    record.value =
        payload.substr(
            offset,
            value_size
        );

    offset += value_size;

    /*
     * There should be no unexpected bytes after a
     * complete WAL record.
     */
    if (offset != payload.size()) {
        return false;
    }

    record.type =
        static_cast<WalRecordType>(
            raw_type
        );

    /*
     * DELETE records must not carry a value.
     *
     * This makes the format stricter and helps detect
     * malformed or corrupted records.
     */
    if (record.type ==
            WalRecordType::DELETE &&
        !record.value.empty()) {
        return false;
    }

    return true;
}

Wal::Wal(
    const std::string& filename,
    DurabilityMode durability_mode
)
    : filename_(filename),
      durability_mode_(durability_mode) {

    open_append();
}

Wal::~Wal() {
    if (file_.is_open()) {
        file_.close();
    }
}

bool Wal::open_append() {
    if (file_.is_open()) {
        return true;
    }

    std::filesystem::path path(
        filename_
    );

    if (!path.parent_path().empty()) {
        std::error_code error;

        std::filesystem::create_directories(
            path.parent_path(),
            error
        );

        if (error) {
            return false;
        }
    }

    file_.open(
        filename_,
        std::ios::binary |
            std::ios::out |
            std::ios::app
    );

    return file_.is_open();
}

bool Wal::append_put(
    const std::string& key,
    const std::string& value
) {
    WalRecord record;

    record.type =
        WalRecordType::PUT;

    record.key = key;
    record.value = value;

    return append_record(record);
}

bool Wal::append_delete(
    const std::string& key
) {
    WalRecord record;

    record.type =
        WalRecordType::DELETE;

    record.key = key;
    record.value.clear();

    return append_record(record);
}

bool Wal::append_record(
    const WalRecord& record
) {
    if (!open_append()) {
        return false;
    }

    std::string payload =
        serialize_wal_record(record);

    /*
     * Physical WAL format:
     *
     * [4 byte payload length]
     * [payload]
     * [4 byte CRC32(payload)]
     *
     * The length tells recovery where the record ends.
     * The checksum tells recovery whether the complete
     * record was corrupted.
     */
    uint32_t length =
        static_cast<uint32_t>(
            payload.size()
        );

    uint32_t checksum =
        calculate_crc32(
            payload.data(),
            payload.size()
        );

    file_.write(
        reinterpret_cast<
            const char*
        >(&length),
        sizeof(length)
    );

    if (!payload.empty()) {
        file_.write(
            payload.data(),
            static_cast<
                std::streamsize
            >(payload.size())
        );
    }

    file_.write(
        reinterpret_cast<
            const char*
        >(&checksum),
        sizeof(checksum)
    );

    if (!file_) {
        return false;
    }

    /*
     * flush() moves C++ buffered data toward the OS.
     *
     * Our durability mode decides when this happens.
     */
    if (should_sync()) {
        return sync();
    }

    return true;
}

bool Wal::should_sync() const {
    return durability_mode_ ==
           DurabilityMode::SYNC;
}

bool Wal::sync() {
    if (!file_.is_open()) {
        return false;
    }

    file_.flush();

    return static_cast<bool>(file_);
}

bool Wal::truncate(
    uint64_t size
) {
    /*
     * Close first because filesystem::resize_file()
     * changes the underlying file.
     */
    if (file_.is_open()) {
        file_.flush();
        file_.close();
    }

    std::error_code error;

    if (!std::filesystem::exists(
            filename_,
            error
        )) {
        if (error) {
            return false;
        }

        std::ofstream create(
            filename_,
            std::ios::binary
        );

        if (!create) {
            return false;
        }

        create.close();
    }

    std::filesystem::resize_file(
        filename_,
        size,
        error
    );

    if (error) {
        return false;
    }

    return open_append();
}

uint64_t Wal::size() const {
    std::error_code error;

    uint64_t file_size =
        std::filesystem::file_size(
            filename_,
            error
        );

    if (error) {
        return 0;
    }

    return file_size;
}

const std::string& Wal::filename() const {
    return filename_;
}

} // namespace forgedb
