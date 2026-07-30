#include "main_window.h"

#include "animation_utils.h"
#include "settings_dialog.h"
#include "activation_dialog.h"
#include "filter_dialog.h"
#include "image_loader.h"
#include "profile_page.h"
#include "stats_page.h"
#include "theme.h"
#include "business/exporter.h"
#include "business/i_fetch_worker.h"
#include "business/filter.h"
#include "core/i_config.h"
#include "core/i_feature_access.h"
#include "core/logger.h"
#include "core/paths.h"
#include "core/version.h"
#include "favorites_page.h"
#include "following_page.h"
#include "history_table_model.h"
#include "licensing/i_license_manager.h"
#include "network/fetchers.h"
#include "network/i_api_client.h"

#include <QApplication>
#include <QFileDialog>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QMenuBar>
#include <QMessageBox>
#include <QProgressBar>
#include <QPushButton>
#include <QStackedWidget>
#include <QStatusBar>
#include <QTableView>
#include <QTimer>
#include <QToolButton>
#include <QVBoxLayout>

namespace bili::gui {

class MainWindow::Impl {
public:
    Impl(MainWindow* q_, bili::IConfig* config_,
         bili::business::IFetchWorker* fetchWorker_,
         bili::IFavoritesFetcher* favoritesFetcher_,
         bili::IApiClient* apiClient_,
         bili::IFeatureAccess* featureAccess_,
         bili::ILicenseManager* licenseManager_)
        : q(q_)
        , config(config_)
        , fetchWorker(fetchWorker_)
        , favoritesFetcher(favoritesFetcher_)
        , apiClient(apiClient_)
        , featureAccess(featureAccess_)
        , licenseManager(licenseManager_)
    {
    }

    QWidget* sidebar = nullptr;
    QStackedWidget* stack = nullptr;
    QTableView* historyTable = nullptr;
    HistoryTableModel* historyModel = nullptr;
    FollowingPage* followingPage = nullptr;
    FavoritesPage* favoritesPage = nullptr;
    StatsPage* statsPage = nullptr;
    ProfilePage* profilePage = nullptr;
    QWidget* historyPage = nullptr;
    ImageLoader* imageLoader = nullptr;

    QLineEdit* searchEdit = nullptr;
    QProgressBar* progressBar = nullptr;
    QLabel* statusLabel = nullptr;
    QWidget* banner = nullptr;
    QLabel* bannerLabel = nullptr;
    QTimer* searchDebounce = nullptr;
    QPushButton* fetchBtn = nullptr;
    QPushButton* exportBtn = nullptr;
    QPushButton* advancedBtn = nullptr;

    MainWindow* q = nullptr;
    RecordList allRecords;
    RecordList filteredRecords;
    bili::IConfig* config = nullptr;
    bili::business::IFetchWorker* fetchWorker = nullptr;
    bili::IFavoritesFetcher* favoritesFetcher = nullptr;
    bili::IApiClient* apiClient = nullptr;
    bili::IFeatureAccess* featureAccess = nullptr;
    bili::ILicenseManager* licenseManager = nullptr;
    bool fetching = false;
};

MainWindow::MainWindow(bili::IConfig* config,
                       bili::business::IFetchWorker* fetchWorker,
                       bili::IFavoritesFetcher* favoritesFetcher,
                       bili::IApiClient* apiClient,
                       bili::IFeatureAccess* featureAccess,
                       bili::ILicenseManager* licenseManager,
                       QWidget* parent)
    : QMainWindow(parent)
    , d(std::make_unique<Impl>(this, config, fetchWorker, favoritesFetcher, apiClient, featureAccess, licenseManager))
{
    Q_ASSERT(d->config != nullptr);
    setMinimumSize(760, 520);
    resize(1600, 900);
    setWindowTitle(QStringLiteral("BiliHistory v%1").arg(Version::toString()));
    setStatusBar(new QStatusBar(this));

    if (qApp) {
        qApp->setStyleSheet(theme::globalStyleSheet());
    }

    buildMenu();

    auto* central = new QWidget(this);
    auto* centralLayout = new QVBoxLayout(central);
    centralLayout->setContentsMargins(0, 0, 0, 0);
    centralLayout->setSpacing(0);

    buildBanner();
    centralLayout->addWidget(d->banner);

    auto* body = new QWidget(central);
    auto* bodyLayout = new QHBoxLayout(body);
    bodyLayout->setContentsMargins(0, 0, 0, 0);
    bodyLayout->setSpacing(0);

    buildSidebar();
    buildPages();

    bodyLayout->addWidget(d->sidebar);
    bodyLayout->addWidget(d->stack, 1);

    centralLayout->addWidget(body, 1);

    d->progressBar = new QProgressBar(central);
    d->progressBar->setRange(0, 0);
    d->progressBar->setTextVisible(false);
    d->progressBar->setFixedHeight(2);
    d->progressBar->setStyleSheet(QStringLiteral(
        "QProgressBar { border: none; background-color: %1; }"
        "QProgressBar::chunk { background-color: %2; }"
    ).arg(theme::BORDER, theme::PINK));
    d->progressBar->hide();
    centralLayout->addWidget(d->progressBar);

    setCentralWidget(central);

    if (d->fetchWorker) {
        // 注意：FetchWorker 已 moveToThread，不能跨线程 setParent（Qt 禁止）。
        // 所有权由 main 中的 unique_ptr 管理，MainWindow 仅持有非拥有指针。
        connect(d->fetchWorker, &bili::business::IFetchWorker::started,
                this, &MainWindow::onFetchStarted);
        connect(d->fetchWorker, &bili::business::IFetchWorker::progress,
                this, &MainWindow::onFetchProgress);
        connect(d->fetchWorker, &bili::business::IFetchWorker::pageFetched,
                this, &MainWindow::onFetchPage);
        connect(d->fetchWorker, &bili::business::IFetchWorker::finished,
                this, &MainWindow::onFetchFinished);
        connect(d->fetchWorker, &bili::business::IFetchWorker::error,
                this, &MainWindow::onFetchError);
        connect(d->fetchWorker, &bili::business::IFetchWorker::cancelled,
                this, &MainWindow::onFetchCancelled);
    }
    connect(d->stack, &QStackedWidget::currentChanged,
            this, &MainWindow::onPageChanged);

    updateStatusBar();
}

MainWindow::~MainWindow() = default;

void MainWindow::buildMenu()
{
    auto* menuBar = this->menuBar();

    auto* fileMenu = menuBar->addMenu(QStringLiteral("文件"));
    auto* exportAction = fileMenu->addAction(QStringLiteral("导出..."));
    connect(exportAction, &QAction::triggered, this, &MainWindow::onExport);
    auto* advancedExportAction = fileMenu->addAction(QStringLiteral("批量筛选 / 高级导出..."));
    connect(advancedExportAction, &QAction::triggered, this, &MainWindow::onAdvancedExport);

    auto* fetchMenu = menuBar->addMenu(QStringLiteral("抓取"));
    auto* fetchAction = fetchMenu->addAction(QStringLiteral("抓取历史记录"));
    connect(fetchAction, &QAction::triggered, this, &MainWindow::onFetchHistory);

    auto* settingsMenu = menuBar->addMenu(QStringLiteral("设置"));
    auto* cookieAction = settingsMenu->addAction(QStringLiteral("Cookie 设置"));
    connect(cookieAction, &QAction::triggered, this, &MainWindow::onSettings);

    auto* helpMenu = menuBar->addMenu(QStringLiteral("帮助"));
    auto* activateAction = helpMenu->addAction(QStringLiteral("会员激活"));
    connect(activateAction, &QAction::triggered, this, &MainWindow::onActivate);
}

void MainWindow::buildBanner()
{
    d->banner = new QWidget(this);
    d->banner->setFixedHeight(40);
    d->banner->setStyleSheet(QStringLiteral(
        "background-color: %1; color: #FFFFFF;"
    ).arg(theme::SUCCESS));

    auto* layout = new QHBoxLayout(d->banner);
    layout->setContentsMargins(16, 0, 16, 0);

    d->bannerLabel = new QLabel(d->banner);
    d->bannerLabel->setStyleSheet(QStringLiteral("color: #FFFFFF; font-size: 13px;"));
    layout->addWidget(d->bannerLabel);

    auto* closeBtn = new QToolButton(d->banner);
    closeBtn->setText(QStringLiteral("✕"));
    closeBtn->setStyleSheet(QStringLiteral("color: #FFFFFF; border: none; font-size: 13px;"));
    connect(closeBtn, &QToolButton::clicked, this, &MainWindow::hideBanner);
    layout->addWidget(closeBtn);

    d->banner->hide();
}

void MainWindow::buildSidebar()
{
    d->sidebar = new QWidget(this);
    d->sidebar->setFixedWidth(216);
    d->sidebar->setStyleSheet(QStringLiteral(
        "background-color: %1; border-right: 1px solid %2;"
    ).arg(theme::CARD, theme::BORDER));

    auto* layout = new QVBoxLayout(d->sidebar);
    layout->setContentsMargins(16, 24, 16, 16);
    layout->setSpacing(12);

    auto* title = new QLabel(QStringLiteral("BiliHistory"), d->sidebar);
    title->setObjectName(QStringLiteral("title"));
    layout->addWidget(title);
    layout->addSpacing(24);

    auto makeButton = [this](const QString& text, int pageIndex) {
        auto* btn = new QPushButton(text, d->sidebar);
        btn->setObjectName(QStringLiteral("primaryButton"));
        btn->setCheckable(true);
        btn->setAutoExclusive(true);
        connect(btn, &QPushButton::clicked, this, [this, pageIndex]() {
            d->stack->setCurrentIndex(pageIndex);
        });
        return btn;
    };

    auto* historyBtn = makeButton(QStringLiteral("历史记录"), 0);
    auto* followingBtn = makeButton(QStringLiteral("我的关注"), 1);
    auto* favoritesBtn = makeButton(QStringLiteral("我的收藏"), 2);
    auto* statsBtn = makeButton(QStringLiteral("统计"), 3);
    auto* profileBtn = makeButton(QStringLiteral("个人主页"), 4);

    historyBtn->setChecked(true);
    layout->addWidget(historyBtn);
    layout->addWidget(followingBtn);
    layout->addWidget(favoritesBtn);
    layout->addWidget(statsBtn);
    layout->addWidget(profileBtn);
    layout->addStretch();
}

void MainWindow::buildPages()
{
    d->stack = new QStackedWidget(this);

    d->historyPage = new QWidget(this);
    auto* historyLayout = new QVBoxLayout(d->historyPage);
    historyLayout->setContentsMargins(20, 20, 20, 20);
    historyLayout->setSpacing(12);

    auto* toolbarLayout = new QHBoxLayout();
    toolbarLayout->setSpacing(12);

    d->searchEdit = new QLineEdit(d->historyPage);
    d->searchEdit->setPlaceholderText(QStringLiteral("搜索历史记录..."));
    d->searchEdit->setClearButtonEnabled(true);
    toolbarLayout->addWidget(d->searchEdit, 1);

    d->fetchBtn = new QPushButton(QStringLiteral("抓取历史记录"), d->historyPage);
    d->fetchBtn->setObjectName(QStringLiteral("primaryButton"));
    connect(d->fetchBtn, &QPushButton::clicked, this, &MainWindow::onFetchHistory);
    toolbarLayout->addWidget(d->fetchBtn);

    d->exportBtn = new QPushButton(QStringLiteral("导出"), d->historyPage);
    d->exportBtn->setObjectName(QStringLiteral("secondaryButton"));
    connect(d->exportBtn, &QPushButton::clicked, this, &MainWindow::onExport);
    toolbarLayout->addWidget(d->exportBtn);

    d->advancedBtn = new QPushButton(QStringLiteral("批量筛选 / 导出"), d->historyPage);
    d->advancedBtn->setObjectName(QStringLiteral("secondaryButton"));
    connect(d->advancedBtn, &QPushButton::clicked, this, &MainWindow::onAdvancedExport);
    toolbarLayout->addWidget(d->advancedBtn);

    historyLayout->addLayout(toolbarLayout);

    d->historyModel = new HistoryTableModel(d->historyPage);
    d->historyTable = new QTableView(d->historyPage);
    d->historyTable->setModel(d->historyModel);
    d->historyTable->setAlternatingRowColors(true);
    d->historyTable->setSelectionBehavior(QAbstractItemView::SelectRows);
    d->historyTable->setHorizontalScrollMode(QAbstractItemView::ScrollPerPixel);
    d->historyTable->setVerticalScrollMode(QAbstractItemView::ScrollPerPixel);
    historyLayout->addWidget(d->historyTable);

    d->imageLoader = new ImageLoader(d->config, this);
    d->followingPage = new FollowingPage(d->imageLoader, this);
    d->favoritesPage = new FavoritesPage(d->config, d->favoritesFetcher, this);
    d->statsPage = new StatsPage(this);
    // ProfilePage 依赖抽象 IUserProfileFetcher* 与共享 ImageLoader*；
    // UserProfileFetcher 实例挂在本窗口下，随窗口析构释放。
    auto* profileFetcher = new bili::UserProfileFetcher(d->apiClient, this);
    d->profilePage = new ProfilePage(d->config, profileFetcher, d->imageLoader, this);

    connect(d->favoritesPage, &FavoritesPage::refreshRequested,
            this, &MainWindow::onFetchFavorites);
    connect(d->favoritesPage, &FavoritesPage::statusChanged,
            this, [this](const QString& msg) { d->statusLabel->setText(msg); });
    connect(d->favoritesPage, &FavoritesPage::error,
            this, [this](const QString& msg) { showBanner(msg, true); });

    d->stack->addWidget(d->historyPage);
    d->stack->addWidget(d->followingPage);
    d->stack->addWidget(d->favoritesPage);
    d->stack->addWidget(d->statsPage);
    d->stack->addWidget(d->profilePage);

    d->searchDebounce = new QTimer(this);
    d->searchDebounce->setSingleShot(true);
    d->searchDebounce->setInterval(200);
    connect(d->searchEdit, &QLineEdit::textChanged, this, [this]() {
        d->searchDebounce->start();
    });
    connect(d->searchDebounce, &QTimer::timeout, this, &MainWindow::onSearchTextChanged);
}

void MainWindow::setHistoryRecords(const RecordList& records)
{
    d->allRecords = records;
    applySearchFilter();
}

void MainWindow::setFollowingUsers(const FollowingList& users)
{
    if (d->followingPage) {
        d->followingPage->loadData(users);
    }
}

void MainWindow::applySearchFilter()
{
    const QString text = d->searchEdit ? d->searchEdit->text().trimmed().toLower() : QString();

    if (text.isEmpty()) {
        d->filteredRecords = d->allRecords;
    } else {
        d->filteredRecords.clear();
        d->filteredRecords.reserve(d->allRecords.size());
        for (const auto& record : d->allRecords) {
            if (!record) continue;
            if (record->title.toLower().contains(text)
                || record->authorName.toLower().contains(text)
                || record->category.toLower().contains(text)
                || record->bvid.toLower().contains(text)) {
                d->filteredRecords.push_back(record);
            }
        }
    }

    if (d->historyModel) {
        d->historyModel->setRows(d->filteredRecords);
    }

    if (d->statusLabel) {
        d->statusLabel->setText(QStringLiteral("显示 %1 / %2 条记录")
                                   .arg(d->filteredRecords.size())
                                   .arg(d->allRecords.size()));
    }
}

void MainWindow::onFetchHistory()
{
    if (d->fetching) {
        d->fetchWorker->cancelFetch();
        return;
    }

    const QString cookie = d->config->cookie();
    if (cookie.isEmpty()) {
        showBanner(QStringLiteral("请先设置 Cookie"), true);
        onSettings();
        return;
    }

    d->fetching = true;
    d->fetchWorker->startFetch(cookie);
}

void MainWindow::onFetchFavorites()
{
    const QString cookie = d->config->cookie();
    if (cookie.isEmpty()) {
        showBanner(QStringLiteral("请先设置 Cookie"), true);
        onSettings();
        return;
    }

    if (d->favoritesPage) {
        d->favoritesPage->refresh(cookie);
    }
}

void MainWindow::onSettings()
{
    SettingsDialog dialog(d->config, d->featureAccess, this);
    dialog.exec();
}

void MainWindow::onActivate()
{
    if (d->licenseManager) {
        ActivationDialog dialog(*d->licenseManager, this);
        dialog.exec();
    }
    updateStatusBar();
}

void MainWindow::onExport()
{
    if (d->allRecords.empty()) {
        showBanner(QStringLiteral("没有可导出的历史记录"), true);
        return;
    }

    const QString filePath = QFileDialog::getSaveFileName(
        this,
        QStringLiteral("导出历史记录"),
        bili::Paths::appDataDir(),
        bili::business::supportedFilters());

    if (filePath.isEmpty()) {
        return;
    }

    try {
        bili::business::exportRecords(d->allRecords, filePath);
        showBanner(QStringLiteral("导出成功: %1").arg(filePath));
        QMessageBox::information(this, QStringLiteral("导出成功"),
                                 QStringLiteral("已成功导出 %1 条记录到:\n%2")
                                     .arg(d->allRecords.size())
                                     .arg(filePath));
    } catch (const bili::business::ExportException& e) {
        showBanner(QStringLiteral("导出失败: %1").arg(e.message()), true);
        QMessageBox::critical(this, QStringLiteral("导出失败"),
                              QStringLiteral("导出失败: %1").arg(e.message()));
    }
}

void MainWindow::onAdvancedExport()
{
    if (!d->featureAccess || !d->featureAccess->isProUnlocked()) {
        if (d->licenseManager) {
            ActivationDialog dialog(*d->licenseManager, this);
            dialog.exec();
        }
        updateStatusBar();
        return;
    }

    if (d->allRecords.empty()) {
        showBanner(QStringLiteral("没有可导出的历史记录"), true);
        return;
    }

    FilterDialog dialog(d->allRecords, this);
    if (dialog.exec() != QDialog::Accepted) {
        return;
    }

    const bili::business::FilterCriteria criteria = dialog.criteria();
    const bili::RecordList filtered = bili::business::filterRecords(d->allRecords, criteria);
    if (filtered.empty()) {
        showBanner(QStringLiteral("没有符合筛选条件的记录"), true);
        return;
    }

    const QString filePath = QFileDialog::getSaveFileName(
        this,
        QStringLiteral("导出筛选结果"),
        bili::Paths::appDataDir(),
        bili::business::supportedFilters());

    if (filePath.isEmpty()) {
        return;
    }

    try {
        bili::business::exportRecords(filtered, filePath);
        showBanner(QStringLiteral("导出成功: %1 (共 %2 条)")
                       .arg(filePath)
                       .arg(filtered.size()));
        QMessageBox::information(this, QStringLiteral("导出成功"),
                                 QStringLiteral("已成功导出 %1 条筛选记录到:\n%2")
                                     .arg(filtered.size())
                                     .arg(filePath));
    } catch (const bili::business::ExportException& e) {
        showBanner(QStringLiteral("导出失败: %1").arg(e.message()), true);
        QMessageBox::critical(this, QStringLiteral("导出失败"),
                              QStringLiteral("导出失败: %1").arg(e.message()));
    }
}

void MainWindow::onStats()
{
    d->stack->setCurrentIndex(3);
    if (d->statsPage) {
        d->statsPage->setRecords(d->allRecords);
    }
}

void MainWindow::onPageChanged(int index)
{
    if (index == 3 && d->statsPage) {
        d->statsPage->setRecords(d->allRecords);
    }
    if (index == 4 && d->profilePage) {
        d->profilePage->refresh();
    }
}

void MainWindow::onSearchTextChanged()
{
    applySearchFilter();
}

void MainWindow::setFetchActive(bool active)
{
    d->fetching = active;
    d->progressBar->setVisible(active);
    if (d->fetchBtn) {
        d->fetchBtn->setText(active ? QStringLiteral("取消抓取") : QStringLiteral("抓取历史记录"));
    }
    if (d->exportBtn) {
        d->exportBtn->setEnabled(!active);
    }
    if (d->advancedBtn) {
        d->advancedBtn->setEnabled(!active);
    }
}

void MainWindow::onFetchStarted()
{
    setFetchActive(true);
    d->statusLabel->setText(QStringLiteral("正在抓取历史记录..."));
}

void MainWindow::onFetchProgress(int total)
{
    d->statusLabel->setText(QStringLiteral("已抓取 %1 条历史记录").arg(total));
}

void MainWindow::onFetchPage(const RecordList& records, int page, int totalSoFar)
{
    Q_UNUSED(page)
    d->allRecords.insert(d->allRecords.end(), records.begin(), records.end());
    applySearchFilter();
    d->statusLabel->setText(QStringLiteral("已抓取 %1 条历史记录").arg(totalSoFar));
}

void MainWindow::onFetchFinished(const RecordList& records)
{
    setFetchActive(false);
    d->allRecords = records;
    applySearchFilter();
    if (d->statsPage) {
        d->statsPage->setRecords(d->allRecords);
    }
    showBanner(QStringLiteral("历史记录抓取完成，共 %1 条").arg(d->allRecords.size()));
    updateStatusBar();
}

void MainWindow::onFetchError(const QString& message)
{
    setFetchActive(false);
    showBanner(QStringLiteral("抓取失败: %1").arg(message), true);
    updateStatusBar();
}

void MainWindow::onFetchCancelled()
{
    setFetchActive(false);
    showBanner(QStringLiteral("已取消抓取"));
    updateStatusBar();
}

void MainWindow::updateStatusBar()
{
    auto* bar = statusBar();
    if (!bar) return;

    bar->clearMessage();
    // 只删除我们自己添加的 widget，避免误删 QStatusBar 内部子控件导致崩溃。
    for (auto* widget : bar->findChildren<QWidget*>(QStringLiteral("status-bar-item"), Qt::FindDirectChildrenOnly)) {
        delete widget;
    }

    d->statusLabel = new QLabel(QStringLiteral("就绪"), this);
    d->statusLabel->setObjectName(QStringLiteral("status-bar-item"));
    bar->addWidget(d->statusLabel);

    const QString licenseText = d->featureAccess
        ? d->featureAccess->statusText()
        : QStringLiteral("未知");
    auto* licenseLabel = new QLabel(licenseText, this);
    licenseLabel->setObjectName(QStringLiteral("status-bar-item"));
    bar->addPermanentWidget(licenseLabel);
}

void MainWindow::showBanner(const QString& message, bool error)
{
    if (!d->banner || !d->bannerLabel) return;

    d->bannerLabel->setText(message);
    d->banner->setStyleSheet(QStringLiteral(
        "background-color: %1; color: #FFFFFF;"
    ).arg(error ? QStringLiteral("#FF4D4F") : theme::SUCCESS));
    d->banner->show();
    animation::fadeIn(d->banner, 180);

    QTimer::singleShot(4000, this, [this]() {
        hideBanner();
    });
}

void MainWindow::hideBanner()
{
    if (!d->banner) return;
    if (!d->banner->isVisible()) return;

    animation::fadeOut(d->banner, 180);
    QTimer::singleShot(180, this, [this]() {
        d->banner->hide();
    });
}

} // namespace bili::gui
