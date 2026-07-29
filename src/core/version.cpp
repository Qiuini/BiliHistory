#include "version.h"

namespace bili {

QString Version::toString() {
    return QStringLiteral("%1.%2.%3").arg(Major).arg(Minor).arg(Patch);
}

std::tuple<int, int, int> Version::toTuple() {
    return {Major, Minor, Patch};
}

} // namespace bili
