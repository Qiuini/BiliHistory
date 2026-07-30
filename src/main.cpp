#include <QApplication>

#include "business/fetch_worker.h"
#include "core/config.h"
#include "core/logger.h"
#include "core/paths.h"
#include "core/version.h"
#include "gui/activation_dialog.h"
#include "gui/main_window.h"
#include "gui/trial_expired_dialog.h"
#include "licensing/feature_access.h"
#include "licensing/license_manager.h"
#include "licensing/trial.h"
#include "network/api_client.h"
#include "network/fetchers.h"
#include "network/http_client.h"

#include <memory>

int main(int argc, char* argv[])
{
    QApplication app(argc, argv);
    app.setApplicationName(bili::Paths::appName());
    app.setOrganizationName(bili::Paths::organization());

    // 日志：main 持有 LogWriter 所有权，装配为 ILogger 注入到静态门面 Logger。
    // 在 Config 之前完成，保证后续所有 Logger::info 等调用写入同一实例。
    auto logger = std::make_unique<bili::LogWriter>();
    logger->init();
    bili::Logger::setInstance(logger.get());

    bili::Logger::info(QStringLiteral("BiliHistory C++ v%1 started").arg(bili::Version::toString()));

    // 配置：独立实例，main 持有所有权，向下以 IConfig* 注入
    auto config = std::make_unique<bili::Config>();
    config->loadDefaults();
    bili::Config* const configPtr = config.get();

    // 授权：单实例装配，注入到 FeatureAccess / GUI
    bili::LicenseManager licenseManager;

    const QString licensePath = bili::Paths::licensePath();
    const QString trialPath = bili::Paths::configDir() + QStringLiteral("/trial.json");

    // 首次运行时持久化试用开始时间，避免每次启动都重新计算。
    if (!licenseManager.isLicensed(licensePath)) {
        bili::Trial::consume(trialPath);
    }

    bili::FeatureAccess featureAccess(licenseManager);
    if (!featureAccess.isProUnlocked()) {
        bili::gui::TrialExpiredDialog trialDialog(licenseManager);
        if (trialDialog.exec() != QDialog::Accepted) {
            return 0;
        }
        // 激活成功后继续打开主窗口
    }

    if (!featureAccess.isProUnlocked()) {
        return 0;
    }

    // 组装依赖：工厂 lambda 在 worker 线程中创建 ApiClient + HistoryFetcher。
    // 关键：ApiClient 挂在 HistoryFetcher 下（setParent），cleanup() 中
    // deleteLater(m_fetcher) 会级联释放 ApiClient/HttpClient/QNetworkAccessManager，
    // 避免每次抓取累积泄漏重量级网络对象。
    auto historyFactory = [configPtr](QObject* parent) -> bili::IHistoryFetcher* {
        auto* httpClient = new bili::HttpClient(configPtr);
        auto* client = new bili::ApiClient(configPtr, httpClient, nullptr);
        httpClient->setParent(client);
        auto* fetcher = new bili::HistoryFetcher(client, configPtr, parent);
        client->setParent(fetcher);
        return fetcher;
    };

    // FetchWorker 会 moveToThread，无法带 QObject parent（Qt 限制）。
    // 用 unique_ptr 在 main 持有所有权：window 先于 fetchWorker 析构（栈/堆逆序），
    // 析构内会 quit+wait 工作线程，再释放对象，安全跨线程清理。
    auto fetchWorker = std::make_unique<bili::business::FetchWorker>(configPtr, historyFactory);

    // 共享 ApiClient：HttpClient 注入并由 ApiClient 持有，随 app 释放。
    auto* sharedHttpClient = new bili::HttpClient(configPtr);
    auto* sharedApiClient = new bili::ApiClient(configPtr, sharedHttpClient, &app);
    sharedHttpClient->setParent(sharedApiClient);
    auto* favoritesFetcher = new bili::FavoritesFetcher(sharedApiClient, configPtr, &app);

    bili::gui::MainWindow window(configPtr, fetchWorker.get(), favoritesFetcher, sharedApiClient, &featureAccess, &licenseManager);
    window.show();

    return app.exec();
}