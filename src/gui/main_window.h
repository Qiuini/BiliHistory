#pragma once

#include <QMainWindow>
#include <QStackedWidget>

#include "core/models.h"
#include "favorites_page.h"
#include "following_page.h"
#include "history_table_model.h"

class QLabel;
class QLineEdit;
class QProgressBar;
class QTableView;
class QTimer;

namespace bili::business {
class FetchWorker;
}

namespace bili::gui {

class StatsPage;

class MainWindow : public QMainWindow {
    Q_OBJECT
public:
    explicit MainWindow(QWidget* parent = nullptr);
    ~MainWindow() override;

    void setHistoryRecords(const RecordList& records);
    void setFollowingUsers(const FollowingList& users);

private slots:
    void onFetchHistory();
    void onFetchFavorites();
    void onSettings();
    void onActivate();
    void onExport();
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
    void buildSidebar();
    void buildPages();
    void buildBanner();
    void updateStatusBar();
    void applySearchFilter();
    void showBanner(const QString& message, bool error = false);
    void hideBanner();

    QWidget* m_sidebar = nullptr;
    QStackedWidget* m_stack = nullptr;
    QTableView* m_historyTable = nullptr;
    HistoryTableModel* m_historyModel = nullptr;
    FollowingPage* m_followingPage = nullptr;
    FavoritesPage* m_favoritesPage = nullptr;
    StatsPage* m_statsPage = nullptr;
    QWidget* m_historyPage = nullptr;

    QLineEdit* m_searchEdit = nullptr;
    QProgressBar* m_progressBar = nullptr;
    QLabel* m_statusLabel = nullptr;
    QWidget* m_banner = nullptr;
    QLabel* m_bannerLabel = nullptr;
    QTimer* m_searchDebounce = nullptr;

    RecordList m_allRecords;
    RecordList m_filteredRecords;
    bili::business::FetchWorker* m_fetchWorker = nullptr;
    bool m_fetching = false;
};

} // namespace bili::gui
