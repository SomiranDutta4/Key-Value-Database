#include "forgedb/concurrency.hpp"

namespace forgedb {

void DatabaseLock::lock_shared() {
    mutex_.lock_shared();
}

void DatabaseLock::unlock_shared() {
    mutex_.unlock_shared();
}

void DatabaseLock::lock() {
    mutex_.lock();
}

void DatabaseLock::unlock() {
    mutex_.unlock();
}

} // namespace forgedb
