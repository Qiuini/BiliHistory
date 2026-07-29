#include "main_window.h"

#include "animation_utils.h"
#include "settings_dialog.h"
#include "activation_dialog.h"
#include "stats_page.h"
#include "theme.h"
#include "business/exporter.h"
#include "business/fetch_worker.h"
#include "core/config.h"
#include "core/logger.h"
#include "core/paths.h"
#include "core/version.h"
#include "licensing/license_manager.h"
#include "licensing/trial.h"

#include <QApplication>
#include <QFileDialog>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QMenuBar>
#include <QMessageBox>
#include <QProgressBar>
#include <QPushButton>
#include <QStatusBar>
#include <QTableView>
#include <QTimer>
#include <QToolButton>
#include <QVBoxLayout>

namespace bili::gui {

MainWindow::MainWindow(QWidget* parent)
    : QMainWindow(parent)
{
    setMinimumSize(760, 520);
    resize(1600, 900);
    setWindowTitle(QStringLiteral("BiliHistory v%1").arg(Version::toString()));

    if (qApp) {
        qApp->setStyleSheet(theme::globalStyleSheet());
    }

    buildMenu();

    auto* central = new QWidget(this);
    auto* centralLayout = new QVBoxLayout(central);
    centralLayout->setContentsMargins(0, 0, 0, 0);
    centralLayout->setSpacing(0);

    buildBanner();
    centralLayout->addWidget(m_banner);

    auto* body = new QWidget(central);
    auto* bodyLayout = new QHBoxLayout(body);
    bodyLayout->setContentsMargins(0, 0, 0, 0);
    bodyLayout->setSpacing(0);

    buildSidebar();
    buildPages();

    bodyLayout->addWidget(m_sidebar);
    bodyLayout->addWidget(m_stack, 1);

    centralLayout->addWidget(body, 1);

    m_progressBar = new QProgressBar(central);
    m_progressBar->setRange(0, 0);
    m_progressBar->setTextVisible(false);
    m_progressBar->setFixedHeight(2);
    m_progressBar->setStyleSheet(QStringLiteral(
        "QProgressBar { border: none; background-color: %1; }"
        "QProgressBar::chunk { background-color: %2; }"
    ).arg(theme::BORDER, theme::PINK));
    m_progressBar->hide();
    centralLayout->addWidget(m_progressBar);

    setCentralWidget(central);

    m_fetchWorker = new bili::business::FetchWorker(this);
    connect(m_fetchWorker, &bili::business::FetchWorker::started,
            this, &MainWindow::onFetchStarted);
    connect(m_fetchWorker, &bili::business::FetchWorker::progress,
            this, &MainWindow::onFetchProgress);
    connect(m_fetchWorker, &bili::business::FetchWorker::pageFetched,
            this, &MainWindow::onFetchPage);
    connect(m_fetchWorker, &bili::business::FetchWorker::finished,
            this, &MainWindow::onFetchFinished);
    connect(m_fetchWorker, &bili::business::FetchWorker::error,
            this, &MainWindow::onFetchError);
    connect(m_fetchWorker, &bili::business::FetchWorker::cancelled,
            this, &MainWindow::onFetchCancelled);
    connect(m_stack, &QStackedWidget::currentChanged,
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
    m_banner = new QWidget(this);
    m_banner->setFixedHeight(40);
    m_banner->setStyleSheet(QStringLiteral(
        "background-color: %1; color: #FFFFFF;"
    ).arg(theme::SUCCESS));

    auto* layout = new QHBoxLayout(m_banner);
    layout->setContentsMargins(16, 0, 16, 0);

    m_bannerLabel = new QLabel(m_banner);
    m_bannerLabel->setStyleSheet(QStringLiteral("color: #FFFFFF; font-size: 13px;"));
    layout->addWidget(m_bannerLabel);

    auto* closeBtn = new QToolButton(m_banner);
    closeBtn->setText(QStringLiteral("✕"));
    closeBtn->setStyleSheet(QStringLiteral("color: #FFFFFF; border: none; font-size: 13px;"));
    connect(closeBtn, &QToolButton::clicked, this, &MainWindow::hideBanner);
    layout->addWidget(closeBtn);

    m_banner->hide();
}

void MainWindow::buildSidebar()
{
    m_sidebar = new QWidget(this);
    m_sidebar->setFixedWidth(216);
    m_sidebar->setStyleSheet(QStringLiteral(
        "background-color: %1; border-right: 1px solid %2;"
    ).arg(theme::CARD, theme::BORDER));

    auto* layout = new QVBoxLayout(m_sidebar);
    layout->setContentsMargins(16, 24, 16, 16);
    layout->setSpacing(12);

    auto* title = new QLabel(QStringLiteral("BiliHistory"), m_sidebar);
    title->setObjectName(QStringLiteral("title"));
    layout->addWidget(title);
    layout->addSpacing(24);

    auto makeButton = [this](const QString& text, int pageIndex) {
        auto* btn = new QPushButton(text, m_sidebar);
        btn->setObjectName(QStringLiteral("primaryButton"));
        btn->setCheckable(true);
        btn->setAutoExclusive(true);
        connect(btn, &QPushButton::clicked, this, [this, pageIndex]() {
            m_stack->setCurrentIndex(pageIndex);
        });
        return btn;
    };

    auto* historyBtn = makeButton(QStringLiteral("历史记录"), 0);
    auto* followingBtn = makeButton(QStringLiteral("我的关注"), 1);
    auto* favoritesBtn = makeButton(QStringLiteral("我的收藏"), 2);
    auto* statsBtn = makeButton(QStringLiteral("统计"), 3);

    historyBtn->setChecked(true);
    layout->addWidget(historyBtn);
    layout->addWidget(followingBtn);
    layout->addWidget(favoritesBtn);
    layout->addWidget(statsBtn);
    layout->addStretch();
}

void MainWindow::buildPages()
{
    m_stack = new QStackedWidget(this);

    m_historyPage = new QWidget(this);
    auto* historyLayout = new QVBoxLayout(m_historyPage);
    historyLayout->setContentsMargins(20, 20, 20, 20);
    historyLayout->setSpacing(12);

    auto* toolbarLayout = new QHBoxLayout();
    toolbarLayout->setSpacing(12);

    m_searchEdit = new QLineEdit(m_historyPage);
    m_searchEdit->setPlaceholderText(QStringLiteral("搜索历史记录..."));
    m_searchEdit->setClearButtonEnabled(true);
    toolbarLayout->addWidget(m_searchEdit, 1);

    auto* fetchBtn = new QPushButton(QStringLiteral("抓取历史记录"), m_historyPage);
    fetchBtn->setObjectName(QStringLiteral("primaryButton"));
    connect(fetchBtn, &QPushButton::clicked, this, &MainWindow::onFetchHistory);
    toolbarLayout->addWidget(fetchBtn);

    auto* exportBtn = new QPushButton(QStringLiteral("导出"), m_historyPage);
    exportBtn->setObjectName(QStringLiteral("secondaryButton"));
    connect(exportBtn, &QPushButton::clicked, this, &MainWindow::onExport);
    toolbarLayout->addWidget(exportBtn);

    historyLayout->addLayout(toolbarLayout);

    m_historyModel = new HistoryTableModel(m_historyPage);
    m_historyTable = new QTableView(m_historyPage);
    m_historyTable->setModel(m_historyModel);
    m_historyTable->setAlternatingRowColors(true);
    m_historyTable->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_historyTable->setHorizontalScrollMode(QAbstractItemView::ScrollPerPixel);
    m_historyTable->setVerticalScrollMode(QAbstractItemView::ScrollPerPixel);
    historyLayout->addWidget(m_historyTable);

    m_followingPage = new FollowingPage(this);
    m_favoritesPage = new FavoritesPage(this);
    m_statsPage = new StatsPage(this);

    connect(m_favoritesPage, &FavoritesPage::refreshRequested,
            this, &MainWindow::onFetchFavorites);
    connect(m_favoritesPage, &FavoritesPage::statusChanged,
            this, [this](const QString& msg) { m_statusLabel->setText(msg); });
    connect(m_favoritesPage, &FavoritesPage::error,
            this, [this](const QString& msg) { showBanner(msg, true); });

    m_stack->addWidget(m_historyPage);
    m_stack->addWidget(m_followingPage);
    m_stack->addWidget(m_favoritesPage);
    m_stack->addWidget(m_statsPage);

    m_searchDebounce = new QTimer(this);
    m_searchDebounce->setSingleShot(true);
    m_searchDebounce->setInterval(200);
    connect(m_searchEdit, &QLineEdit::textChanged, this, [this]() {
        m_searchDebounce->start();
    });
    connect(m_searchDebounce, &QTimer::timeout, this, &MainWindow::onSearchTextChanged);
}

void MainWindow::setHistoryRecords(const RecordList& records)
{
    m_allRecords = records;
    applySearchFilter();
}

void MainWindow::setFollowingUsers(const FollowingList& users)
{
    if (m_followingPage) {
        m_followingPage->loadData(users);
    }
}

void MainWindow::applySearchFilter()
{
    const QString text = m_searchEdit ? m_searchEdit->text().trimmed().toLower() : QString();

    if (text.isEmpty()) {
        m_filteredRecords = m_allRecords;
    } else {
        m_filteredRecords.clear();
        for (const auto& record : m_allRecords) {
            if (!record) continue;
            if (record->title.toLower().contains(text)
                || record->authorName.toLower().contains(text)
                || record->category.toLower().contains(text)
                || record->bvid.toLower().contains(text)) {
                m_filteredRecords.push_back(record);
            }
        }
    }

    if (m_historyModel) {
        m_historyModel->setRows(m_filteredRecords);
    }

    if (m_statusLabel) {
        m_statusLabel->setText(QStringLiteral("显示 %1 / %2 条记录")
                                   .arg(m_filteredRecords.size())
                                   .arg(m_allRecords.size()));
    }
}

void MainWindow::onFetchHistory()
{
    if (m_fetching) {
        m_fetchWorker->cancelFetch();
        return;
    }

    const QString cookie = bili::Config::instance().cookie();
    if (cookie.isEmpty()) {
        showBanner(QStringLiteral("请先设置 Cookie"), true);
        onSettings();
        return;
    }

    m_fetching = true;
    m_fetchWorker->startFetch(cookie);
}

void MainWindow::onFetchFavorites()
{
    const QString cookie = bili::Config::instance().cookie();
    if (cookie.isEmpty()) {
        showBanner(QStringLiteral("请先设置 Cookie"), true);
        onSettings();
        return;
    }

    if (m_favoritesPage) {
        m_favoritesPage->refresh(cookie);
    }
}

void MainWindow::onSettings()
{
    SettingsDialog dialog(this);
    dialog.exec();
}

void MainWindow::onActivate()
{
    ActivationDialog dialog(this);
    dialog.exec();
    updateStatusBar();
}

void MainWindow::onExport()
{
    if (m_allRecords.empty()) {
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
        bili::business::exportRecords(m_allRecords, filePath);
        showBanner(QStringLiteral("导出成功: %1").arg(filePath));
    } catch (const bili::business::ExportException& e) {
        showBanner(QStringLiteral("导出失败: %1").arg(e.message()), true);
    }
}

void MainWindow::onStats()
{
    m_stack->setCurrentIndex(3);
    if (m_statsPage) {
        m_statsPage->setRecords(m_allRecords);
    }
}

void MainWindow::onPageChanged(int index)
{
    if (index == 3 && m_statsPage) {
        m_statsPage->setRecords(m_allRecords);
    }
}

void MainWindow::onSearchTextChanged()
{
    applySearchFilter();
}

void MainWindow::onFetchStarted()
{
    m_fetching = true;
    m_progressBar->show();
    m_statusLabel->setText(QStringLiteral("正在抓取历史记录..."));
}

void MainWindow::onFetchProgress(int total)
{
    m_statusLabel->setText(QStringLiteral("已抓取 %1 条历史记录").arg(total));
}

void MainWindow::onFetchPage(const RecordList& records, int page, int totalSoFar)
{
    Q_UNUSED(page)
    m_allRecords.insert(m_allRecords.end(), records.begin(), records.end());
    applySearchFilter();
    m_statusLabel->setText(QStringLiteral("已抓取 %1 条历史记录").arg(totalSoFar));
}

void MainWindow::onFetchFinished(const RecordList& records)
{
    m_fetching = false;
    m_progressBar->hide();
    m_allRecords = records;
    applySearchFilter();
    if (m_statsPage) {
        m_statsPage->setRecords(m_allRecords);
    }
    showBanner(QStringLiteral("历史记录抓取完成，共 %1 条").arg(m_allRecords.size()));
    updateStatusBar();
}

void MainWindow::onFetchError(const QString& message)
{
    m_fetching = false;
    m_progressBar->hide();
    showBanner(QStringLiteral("抓取失败: %1").arg(message), true);
    updateStatusBar();
}

void MainWindow::onFetchCancelled()
{
    m_fetching = false;
    m_progressBar->hide();
    showBanner(QStringLiteral("已取消抓取"));
    updateStatusBar();
}

void MainWindow::updateStatusBar()
{
    auto* bar = statusBar();
    if (!bar) return;

    bar->clearMessage();
    while (bar->findChild<QWidget*>()) {
        delete bar->findChild<QWidget*>();
    }

    m_statusLabel = new QLabel(QStringLiteral("就绪"), this);
    bar->addWidget(m_statusLabel);

    QString licenseText;
    const QString licensePath = Paths::licensePath();
    if (LicenseManager::isLicensed(licensePath)) {
        licenseText = QStringLiteral("已授权");
    } else {
        const int remaining = Trial::remainingDays(licensePath);
        if (remaining > 0) {
            licenseText = QStringLiteral("试用剩余 %1 天").arg(remaining);
        } else {
            licenseText = QStringLiteral("试用已过期");
        }
    }
    auto* licenseLabel = new QLabel(licenseText, this);
    bar->addPermanentWidget(licenseLabel);
}

void MainWindow::showBanner(const QString& message, bool error)
{
    if (!m_banner || !m_bannerLabel) return;

    m_bannerLabel->setText(message);
    m_banner->setStyleSheet(QStringLiteral(
        "background-color: %1; color: #FFFFFF;"
    ).arg(error ? QStringLiteral("#FF4D4F") : theme::SUCCESS));
    m_banner->show();
    animation::fadeIn(m_banner, 180);

    QTimer::singleShot(4000, this, [this]() {
        hideBanner();
    });
}

void MainWindow::hideBanner()
{
    if (!m_banner) return;
    if (!m_banner->isVisible()) return;

    animation::fadeOut(m_banner, 180);
    QTimer::singleShot(180, this, [this]() {
        m_banner->hide();
    });
}

} // namespace bili::gui
