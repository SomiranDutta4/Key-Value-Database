#include "forgedb/fault_injection.hpp"

namespace forgedb {

FaultPoint FaultInjector::active_fault_ =
    FaultPoint::NONE;

void FaultInjector::set_fault(
    FaultPoint point
) {
    active_fault_ = point;
}

void FaultInjector::clear() {
    active_fault_ = FaultPoint::NONE;
}

bool FaultInjector::should_fail(
    FaultPoint point
) {
    return active_fault_ == point;
}

} // namespace forgedb
