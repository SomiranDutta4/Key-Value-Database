#include "forgedb/recovery.hpp"

#include <cerrno>
#include <cstring>
#include <fstream>
#include <string>
#include <vector>

#include "forgedb/crc32.hpp"
#include "forgedb/wal.hpp"

namespace forgedb {

namespace {

enum class ReadStatus {
    SUCCESS,
    EOF_REACHED,
    INCOMPLETE,
    CORRUPTED,
    IO_ERROR
};

bool read_exact(
    std::ifstream& file,
    char* buffer,
    std::size_t size
) {
    file.read(
        buffer,
        static_cast<std::streamsize>(size)
    );

    return file.gcount() ==
           static_cast<std::streamsize>(size);
}

ReadStatus read_record(
    std::ifstream& file,
    WalRecord& record,
    std::string& error
) {
    uint32_t length = 0;

    file.read(
        reinterpret_cast<char*>(&length),
        sizeof(length)
    );

    if (file.eof()) {
        return ReadStatus::EOF_REACHED;
    }

    if (file.gcount() !=
        static_cast<std::streamsize>(
            sizeof(length)
        )) {
        return ReadStatus::INCOMPLETE;
    }

    /*
     * A corrupted length can otherwise make us try
     * to allocate a huge amount of memory.
     */
    constexpr uint32_t MAX_RECORD_SIZE =
        16 * 1024 * 1024;

    if (length == 0 ||
        length > MAX_RECORD_SIZE) {
        error =
            "Invalid WAL record length";

        return ReadStatus::CORRUPTED;
    }

    std::string payload(
        length,
        '\0'
    );

    if (!read_exact(
            file,
            payload.data(),
            payload.size()
        )) {
        return ReadStatus::INCOMPLETE;
    }

    uint32_t stored_checksum = 0;

    if (!read_exact(
            file,
            reinterpret_cast<char*>(
                &stored_checksum
            ),
            sizeof(stored_checksum)
        )) {
        return ReadStatus::INCOMPLETE;
    }

    uint32_t calculated_checksum =
        calculate_crc32(
            payload.data(),
            payload.size()
        );

    if (stored_checksum !=
        calculated_checksum) {
        error =
            "WAL checksum mismatch";

        return ReadStatus::CORRUPTED;
    }

    if (!deserialize_wal_record(
            payload,
            record
        )) {
        error =
            "Invalid WAL record format";

        return ReadStatus::CORRUPTED;
    }

    return ReadStatus::SUCCESS;
}

} // namespace

RecoveryResult recover_wal(
    const std::string& filename,
    std::unordered_map<
        std::string,
        MemTableEntry
    >& data
) {
    data.clear();

    std::ifstream file(
        filename,
        std::ios::binary
    );

    /*
     * No WAL yet is not an error.
     */
    if (!file) {
        return {
            RecoveryStatus::SUCCESS,
            ""
        };
    }

    while (true) {
        WalRecord record;
        std::string error;

        ReadStatus status =
            read_record(
                file,
                record,
                error
            );

        switch (status) {
        case ReadStatus::SUCCESS:
            break;

        case ReadStatus::EOF_REACHED:
            return {
                RecoveryStatus::SUCCESS,
                ""
            };

        case ReadStatus::INCOMPLETE:
            /*
             * This commonly means the machine crashed
             * while the final record was being written.
             *
             * Earlier complete records are still valid.
             */
            return {
                RecoveryStatus::
                    RECOVERED_INCOMPLETE_TAIL,
                "Recovered WAL with incomplete final record"
            };

        case ReadStatus::CORRUPTED:
            return {
                RecoveryStatus::CORRUPTED,
                error
            };

        case ReadStatus::IO_ERROR:
            return {
                RecoveryStatus::IO_ERROR,
                error
            };
        }

        switch (record.type) {
        case WalRecordType::PUT:
            data[record.key] =
                MemTableEntry{
                    record.value,
                    false
                };
            break;

        case WalRecordType::DELETE:
            data[record.key] =
                MemTableEntry{
                    "",
                    true
                };
            break;

        default:
            return {
                RecoveryStatus::CORRUPTED,
                "Unknown WAL record type"
            };
        }
    }
}

} // namespace forgedb
