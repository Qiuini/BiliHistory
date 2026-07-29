#include "machine_id.h"

#include <QCryptographicHash>
#include <QNetworkInterface>
#include <QSysInfo>

namespace bili {

QString MachineId::current()
{
    const QString hostName = QSysInfo::machineHostName();

    QString macAddress;
    const auto interfaces = QNetworkInterface::allInterfaces();
    for (const QNetworkInterface& iface : interfaces) {
        if ((iface.flags() & QNetworkInterface::IsUp) &&
            !(iface.flags() & QNetworkInterface::IsLoopBack)) {
            const QString addr = iface.hardwareAddress();
            if (!addr.isEmpty()) {
                macAddress = addr;
                break;
            }
        }
    }

    const QString platform = QSysInfo::kernelType();
    const QString machine = QSysInfo::currentCpuArchitecture();

    const QString raw = QStringLiteral("%1|%2|%3|%4")
                            .arg(hostName, macAddress, platform, machine);

    const QByteArray hash = QCryptographicHash::hash(raw.toUtf8(), QCryptographicHash::Sha256);
    return QString::fromUtf8(hash.toHex()).left(16);
}

} // namespace bili
