#pragma once

#include <QString>
#include <tuple>

namespace bili {

struct Version {
    static constexpr int Major = 2;
    static constexpr int Minor = 0;
    static constexpr int Patch = 0;

    static QString toString();
    static std::tuple<int, int, int> toTuple();
};

} // namespace bili
