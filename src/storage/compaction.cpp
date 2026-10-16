#include "forgedb/compaction.hpp"

#include "forgedb/storage.hpp"

namespace forgedb {

Compaction::Compaction(
    Storage& storage
)
    : storage_(storage) {
}

bool Compaction::should_compact() const {
    return storage_.table_count() >=
           COMPACTION_TABLE_THRESHOLD;
}

bool Compaction::run(
    std::string& error
) {
    return storage_.compact(error);
}

} // namespace forgedb
