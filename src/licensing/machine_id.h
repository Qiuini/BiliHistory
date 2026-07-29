#pragma once

#include <QString>

namespace bili {

class MachineId {
public:
    static QString current();

private:
    MachineId() = delete;
};

} // namespace bili
