#include "forgedb/memtable.hpp"

#include <utility>

namespace forgedb {

void MemTable::put(
    const std::string& key,
    const std::string& value
) {
    auto it = data_.find(key);

    if (it != data_.end()) {
        size_in_bytes_ -=
            it->first.size() +
            it->second.value.size();

        it->second.value = value;
        it->second.deleted = false;

        size_in_bytes_ +=
            it->first.size() +
            it->second.value.size();

        return;
    }

    data_.emplace(
        key,
        MemTableEntry{
            value,
            false
        }
    );

    size_in_bytes_ +=
        key.size() +
        value.size();
}

void MemTable::erase(
    const std::string& key
) {
    auto it = data_.find(key);

    if (it != data_.end()) {
        size_in_bytes_ -=
            it->first.size() +
            it->second.value.size();

        it->second.value.clear();
        it->second.deleted = true;

        size_in_bytes_ +=
            it->first.size();

        return;
    }

    data_.emplace(
        key,
        MemTableEntry{
            "",
            true
        }
    );

    size_in_bytes_ += key.size();
}

bool MemTable::get(
    const std::string& key,
    std::string& value,
    bool& deleted
) const {
    auto it = data_.find(key);

    if (it == data_.end()) {
        return false;
    }

    deleted = it->second.deleted;

    if (!deleted) {
        value = it->second.value;
    } else {
        value.clear();
    }

    return true;
}

void MemTable::clear() {
    data_.clear();
    size_in_bytes_ = 0;
}

void MemTable::replace(
    std::unordered_map<
        std::string,
        MemTableEntry
    > data
) {
    data_ = std::move(data);
    recompute_size();
}

std::unordered_map<
    std::string,
    MemTableEntry
> MemTable::snapshot() const {
    return data_;
}

std::size_t MemTable::size_in_bytes() const {
    return size_in_bytes_;
}

std::size_t MemTable::entry_count() const {
    return data_.size();
}

void MemTable::recompute_size() {
    size_in_bytes_ = 0;

    for (const auto& [key, entry] : data_) {
        size_in_bytes_ += key.size();

        if (!entry.deleted) {
            size_in_bytes_ +=
                entry.value.size();
        }
    }
}

} // namespace forgedb
