#include "forgedb/snapshot.hpp"

#include <utility>

namespace forgedb {

Snapshot::Snapshot(
    std::unordered_map<
        std::string,
        std::string
    > data
)
    : data_(std::move(data)) {
}

const std::unordered_map<
    std::string,
    std::string
>& Snapshot::data() const {
    return data_;
}

uint64_t Snapshot::sequence_number() const {
    return sequence_number_;
}

void Snapshot::set_sequence_number(
    uint64_t sequence_number
) {
    sequence_number_ = sequence_number;
}

} // namespace forgedb
