#include <QApplication>

#include "core/config.h"
#include "core/logger.h"
#include "core/paths.h"
#include "core/version.h"
#include "gui/activation_dialog.h"
#include "gui/main_window.h"
#include "gui/trial_expired_dialog.h"
#include "licensing/license_manager.h"
#include "licensing/trial.h"

int main(int argc, char* argv[])
{
    QApplication app(argc, argv);
    app.setApplicationName(bili::Paths::appName());
    app.setOrganizationName(bili::Paths::organization());

    bili::Logger::init();
    bili::Config::instance().loadDefaults();
    bili::Logger::info(QStringLiteral("BiliHistory C++ v%1 started").arg(bili::Version::toString()));

    const QString licensePath = bili::Paths::licensePath();

    if (!bili::LicenseManager::isLicensed(licensePath)) {
        const QString trialPath = bili::Paths::configDir() + QStringLiteral("/trial.json");
        if (!bili::Trial::isActive(trialPath)) {
            bili::gui::TrialExpiredDialog trialDialog;
            if (trialDialog.exec() != QDialog::Accepted) {
                return 0;
            }
            // 激活成功后继续打开主窗口
        }
    }

    bili::gui::MainWindow window;
    window.show();

    return app.exec();
}
