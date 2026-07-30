#include "feature_access.h"

#include "trial.h"
#include "core/paths.h"

namespace bili {

FeatureAccess::FeatureAccess(LicenseManager& licenseManager)
    : FeatureAccess(licenseManager,
                    Paths::licensePath(),
                    Paths::configDir() + QStringLiteral("/trial.json"))
{
}

FeatureAccess::FeatureAccess(LicenseManager& licenseManager,
                             QString licensePath,
                             QString trialPath)
    : m_licenseManager(licenseManager)
    , m_licensePath(std::move(licensePath))
    , m_trialPath(std::move(trialPath))
{
}

bool FeatureAccess::isProUnlocked() const
{
    if (m_licenseManager.isLicensed(m_licensePath)) {
        return true;
    }
    return Trial::isActive(m_trialPath);
}

int FeatureAccess::remainingTrialDays() const
{
    if (m_licenseManager.isLicensed(m_licensePath)) {
        return 0;
    }
    return Trial::remainingDays(m_trialPath);
}

QString FeatureAccess::statusText() const
{
    if (m_licenseManager.isLicensed(m_licensePath)) {
        return QStringLiteral("已授权");
    }
    const int remaining = remainingTrialDays();
    if (remaining > 0) {
        return QStringLiteral("试用剩余 %1 天").arg(remaining);
    }
    return QStringLiteral("试用已过期");
}

} // namespace bili
