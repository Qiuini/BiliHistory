#include "machine_id.h"

#include <QCryptographicHash>
#include <QNetworkInterface>
#include <QSysInfo>

#include <algorithm>

namespace bili {

namespace {

// 选取稳定的物理网卡 MAC：仅取非回环、非虚拟、当前 Up 的接口。
// 多网卡时按硬件地址字典序排序后全部拼接，避免"取第一个"导致顺序漂移。
QString stableMacFingerprint()
{
    const auto interfaces = QNetworkInterface::allInterfaces();
    QStringList macs;
    macs.reserve(interfaces.size());

    for (const QNetworkInterface& iface : interfaces) {
        if (!(iface.flags() & QNetworkInterface::IsUp)) continue;
        if (iface.flags() & QNetworkInterface::IsLoopBack) continue;
        const QString hardware = iface.hardwareAddress();
        if (hardware.isEmpty()) continue;

        const QString name = iface.humanReadableName().toLower();
        if (name.contains(QStringLiteral("vmware")) ||
            name.contains(QStringLiteral("virtualbox")) ||
            name.contains(QStringLiteral("docker")) ||
            name.contains(QStringLiteral("veth")) ||
            name.contains(QStringLiteral("tap")) ||
            name.contains(QStringLiteral("tun")) ||
            name.contains(QStringLiteral("bluetooth"))) {
            continue;
        }
        macs.append(hardware);
    }

    std::sort(macs.begin(), macs.end());
    return macs.join(QLatin1Char(';'));
}

} // namespace

QString MachineId::current()
{
    // 优先使用系统级 UUID（Qt 6 跨平台）：
    //   Windows  -> HKLM\Software\Microsoft\Cryptography\MachineGuid
    //   Linux    -> /etc/machine-id 或 /var/lib/dbus/machine-id
    //   macOS    -> IOPlatformUUID
    // 这些标识在系统重装前保持稳定，不随网卡/USB 设备变化。
    const QByteArray machineUuid = QSysInfo::machineUniqueId();

    const QString hostName = QSysInfo::machineHostName();
    const QString kernel = QSysInfo::kernelType();
    const QString arch = QSysInfo::currentCpuArchitecture();
    const QString macFingerprint = stableMacFingerprint();

    // 多源聚合：machineUuid 为主，其余为兜底，任一来源缺失仍可生成稳定标识
    const QString raw = QStringLiteral("%1|%2|%3|%4|%5")
                            .arg(QString::fromLatin1(machineUuid.toHex()),
                                 hostName,
                                 kernel,
                                 arch,
                                 macFingerprint);

    const QByteArray hash = QCryptographicHash::hash(raw.toUtf8(), QCryptographicHash::Sha256);
    return QString::fromUtf8(hash.toHex()).left(16);
}

} // namespace bili
