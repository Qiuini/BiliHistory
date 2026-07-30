#include "favorites_page.h"

#include "animation_utils.h"
#include "theme.h"

#include <QDesktopServices>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QTreeWidget>
#include <QUrl>
#include <QVBoxLayout>

#include <map>

namespace bili::gui {

namespace {

QString favoriteItemLink(const FavoriteItem& item)
{
    if (item.type == RecordType::Article && !item.cvId.isEmpty()) {
        return QStringLiteral("https://www.bilibili.com/read/cv%1").arg(item.cvId);
    }
    if (!item.bvid.isEmpty()) {
        return QStringLiteral("https://www.bilibili.com/video/%1").arg(item.bvid);
    }
    if (!item.id.isEmpty()) {
        return QStringLiteral("https://www.bilibili.com/video/%1").arg(item.id);
    }
    return QString();
}

QString recordTypeDisplay(RecordType type)
{
    switch (type) {
    case RecordType::Video:
        return QStringLiteral("视频");
    case RecordType::Live:
        return QStringLiteral("直播");
    case RecordType::Article:
        return QStringLiteral("专栏");
    default:
        return QStringLiteral("其他");
    }
}

} // namespace

class FavoritesPage::Impl {
public:
    bili::IConfig* config = nullptr;
    bili::IFavoritesFetcher* fetcher = nullptr;
    QLabel* subtitle = nullptr;
    QTreeWidget* tree = nullptr;
    QPushButton* refreshBtn = nullptr;

    bili::FavoriteFolderList folders;
    std::map<QString, std::vector<bili::FavoriteItem>> resources;
    QString cookie;
    size_t pendingFolderIndex = 0;
    bool fetching = false;
};

FavoritesPage::FavoritesPage(bili::IConfig* config, bili::IFavoritesFetcher* fetcher, QWidget* parent)
    : QWidget(parent)
    , d(std::make_unique<Impl>())
{
    d->config = config;
    d->fetcher = fetcher;
    buildUi();

    if (d->fetcher) {
        d->fetcher->setParent(this);
        connect(d->fetcher, &bili::IFavoritesFetcher::foldersFinished,
                this, &FavoritesPage::onFoldersFinished);
        connect(d->fetcher, &bili::IFavoritesFetcher::resourcesFinished,
                this, &FavoritesPage::onResourcesFinished);
        connect(d->fetcher, &bili::IFavoritesFetcher::error,
                this, &FavoritesPage::onFetcherError);
    }
}

FavoritesPage::~FavoritesPage() = default;

void FavoritesPage::buildUi()
{
    auto* mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(20, 20, 20, 20);
    mainLayout->setSpacing(12);

    auto* headerLayout = new QHBoxLayout();
    headerLayout->setSpacing(12);

    auto* title = new QLabel(QStringLiteral("我的收藏"), this);
    title->setStyleSheet(QStringLiteral("color: %1; font-size: 20px; font-weight: 800;").arg(theme::TEXT));
    headerLayout->addWidget(title);

    headerLayout->addStretch();

    d->refreshBtn = new QPushButton(QStringLiteral("刷新收藏夹"), this);
    d->refreshBtn->setObjectName(QStringLiteral("primaryButton"));
    connect(d->refreshBtn, &QPushButton::clicked, this, &FavoritesPage::refreshRequested);
    headerLayout->addWidget(d->refreshBtn);

    mainLayout->addLayout(headerLayout);

    d->subtitle = new QLabel(QStringLiteral("点击刷新收藏夹加载数据"), this);
    d->subtitle->setStyleSheet(QStringLiteral("color: %1; font-size: 12px;").arg(theme::TEXT_3));
    mainLayout->addWidget(d->subtitle);

    d->tree = new QTreeWidget(this);
    d->tree->setObjectName(QStringLiteral("FavoritesTree"));
    d->tree->setHeaderHidden(true);
    d->tree->setColumnCount(3);
    d->tree->setColumnWidth(0, 420);
    d->tree->setColumnWidth(1, 140);
    d->tree->setColumnWidth(2, 100);
    d->tree->setIndentation(20);
    d->tree->setFrameShape(QFrame::NoFrame);
    d->tree->setStyleSheet(QStringLiteral(
        "QTreeWidget#FavoritesTree {"
        "  background-color: %1;"
        "  border: 1px solid %2;"
        "  border-radius: 12px;"
        "  outline: none;"
        "}"
        "QTreeWidget::item {"
        "  padding: 10px 16px;"
        "  border-bottom: 1px solid %3;"
        "}"
        "QTreeWidget::item:selected {"
        "  background: %4;"
        "  color: %5;"
        "}"
        "QTreeWidget::item:hover {"
        "  background: %6;"
        "}"
    ).arg(theme::CARD, theme::BORDER, theme::BORDER, theme::PINK_LIGHT, theme::TEXT, theme::SURFACE_HOVER));

    connect(d->tree, &QTreeWidget::itemDoubleClicked, this, &FavoritesPage::onItemDoubleClicked);
    connect(d->tree, &QTreeWidget::itemExpanded, this, &FavoritesPage::onItemExpanded);
    mainLayout->addWidget(d->tree, 1);

    animation::installButtonScaleAnimation(d->refreshBtn);
}

void FavoritesPage::refresh(const QString& cookie)
{
    if (d->fetching) {
        cancel();
    }

    if (cookie.isEmpty()) {
        emit error(QStringLiteral("未设置 Cookie，无法加载收藏夹"));
        return;
    }

    if (!d->fetcher) {
        emit error(QStringLiteral("收藏夹抓取器未注入"));
        return;
    }

    d->cookie = cookie;
    d->folders.clear();
    d->resources.clear();
    d->pendingFolderIndex = 0;
    d->fetching = true;
    clearTree();
    updateSubtitle();

    emit statusChanged(QStringLiteral("正在加载收藏夹..."));
    d->fetcher->fetchFolders(cookie);
}

void FavoritesPage::cancel()
{
    if (d->fetcher && d->fetching) {
        d->fetcher->cancel();
    }
    d->fetching = false;
    updateSubtitle();
}

void FavoritesPage::clearTree()
{
    d->tree->clear();
}

void FavoritesPage::updateSubtitle()
{
    const int folderCount = static_cast<int>(d->folders.size());
    int resourceCount = 0;
    for (const auto& folder : d->folders) {
        resourceCount += static_cast<int>(folder.items.size());
    }

    if (d->fetching) {
        d->subtitle->setText(QStringLiteral("共 %1 个收藏夹，已加载 %2 条内容")
                                .arg(folderCount)
                                .arg(resourceCount));
    } else {
        d->subtitle->setText(QStringLiteral("共 %1 个收藏夹，%2 条内容")
                                .arg(folderCount)
                                .arg(resourceCount));
    }
}

void FavoritesPage::onFoldersFinished(const bili::FavoriteFolderList& folders)
{
    d->folders = folders;
    for (const auto& folder : d->folders) {
        d->resources[QString::number(folder.id)] = {};
    }
    renderFolders();
    loadNextFolderResources();
}

void FavoritesPage::renderFolders()
{
    clearTree();
    for (const auto& folder : d->folders) {
        auto* item = new QTreeWidgetItem(d->tree);
        item->setText(0, QStringLiteral("📁 %1").arg(folder.name));
        item->setText(1, QStringLiteral("%1 条").arg(folder.mediaCount));
        item->setData(0, Qt::UserRole, QStringLiteral("folder:%1").arg(folder.id));
        item->setExpanded(true);
        item->setFlags(item->flags() | Qt::ItemIsAutoTristate);
    }
    updateSubtitle();
}

void FavoritesPage::loadNextFolderResources()
{
    if (d->pendingFolderIndex >= d->folders.size()) {
        d->fetching = false;
        updateSubtitle();
        emit statusChanged(QStringLiteral("收藏夹加载完成"));
        return;
    }

    const qint64 folderId = d->folders[d->pendingFolderIndex].id;
    emit statusChanged(QStringLiteral("正在加载收藏夹 %1 / %2")
                           .arg(d->pendingFolderIndex + 1)
                           .arg(d->folders.size()));
    d->fetcher->fetchResources(QString::number(folderId), d->config->favoritesPageSize(), d->cookie);
}

void FavoritesPage::onResourcesFinished(const QString& folderId,
                                        const std::vector<bili::FavoriteItem>& items)
{
    d->resources[folderId] = items;

    // 找到对应文件夹节点并填充内容
    const qint64 id = folderId.toLongLong();
    for (int i = 0; i < d->tree->topLevelItemCount(); ++i) {
        auto* folderItem = d->tree->topLevelItem(i);
        const QString data = folderItem->data(0, Qt::UserRole).toString();
        if (data == QStringLiteral("folder:%1").arg(id)) {
            // 清除旧占位
            while (folderItem->childCount() > 0) {
                delete folderItem->takeChild(0);
            }
            for (const auto& item : items) {
                auto* child = new QTreeWidgetItem(folderItem);
                child->setText(0, item.title);
                child->setText(1, item.upperName);
                child->setText(2, recordTypeDisplay(item.type));
                child->setData(0, Qt::UserRole, favoriteItemLink(item));
            }
            break;
        }
    }

    ++d->pendingFolderIndex;
    updateSubtitle();
    loadNextFolderResources();
}

void FavoritesPage::onFetcherError(const bili::NetworkException& e)
{
    d->fetching = false;
    updateSubtitle();
    emit error(e.message());
}

void FavoritesPage::onItemDoubleClicked(QTreeWidgetItem* item, int /*column*/)
{
    if (!item) return;
    const QString link = item->data(0, Qt::UserRole).toString();
    if (!link.isEmpty() && link.startsWith(QStringLiteral("http"))) {
        QDesktopServices::openUrl(QUrl(link));
    }
}

void FavoritesPage::onItemExpanded(QTreeWidgetItem* item)
{
    if (!item) return;
    const QString data = item->data(0, Qt::UserRole).toString();
    if (data.startsWith(QStringLiteral("folder:"))) {
        QString text = item->text(0);
        if (text.startsWith(QStringLiteral("📁"))) {
            text.replace(0, 2, QStringLiteral("📂"));
            item->setText(0, text);
        }
    }
}

} // namespace bili::gui
