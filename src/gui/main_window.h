#pragma once

#include <QMainWindow>
#include <memory>

#include "core/models.h"

class QLabel;

namespace bili {
class IApiClient;
class IConfig;
class IFeatureAccess;
class ILicenseManager;
} // namespace bili

namespace bili::business {
class IFetchWorker;
}

namespace bili {
class IFavoritesFetcher;
}

namespace bili::gui {

class MainWindow : public QMainWindow {
    Q_OBJECT
public:
    explicit MainWindow(bili::IConfig* config,
                        bili::business::IFetchWorker* fetchWorker,
                        bili::IFavoritesFetcher* favoritesFetcher,
                        bili::IApiClient* apiClient,
                        bili::IFeatureAccess* featureAccess,
                        bili::ILicenseManager* licenseManager = nullptr,
                        QWidget* parent = nullptr);
    ~MainWindow() override;

    void setHistoryRecords(const RecordList& records);
    void setFollowingUsers(const FollowingList& users);

private slots:
    void onFetchHistory();
    void onFetchFavorites();
    void onSettings();
    void onActivate();
    void onExport();
    void onAdvancedExport();
    void onStats();
    void onSearchTextChanged();
    void onPageChanged(int index);

    void onFetchStarted();
    void onFetchProgress(int total);
    void onFetchPage(const RecordList& records, int page, int totalSoFar);
    void onFetchFinished(const RecordList& records);
    void onFetchError(const QString& message);
    void onFetchCancelled();

private:
    void buildMenu();
    void buildBanner();
    void buildSidebar();
    void buildPages();

    void applySearchFilter();
    void setFetchActive(bool active);
    void updateStatusBar();
    void showBanner(const QString& message, bool error = false);
    void hideBanner();

    class Impl;
    std::unique_ptr<Impl> d;
};

} // namespace bili::gui
