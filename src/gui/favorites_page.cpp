#include "favorites_page.h"

#include "animation_utils.h"
#include "network/api_client.h"
#include "network/fetchers.h"
#include "theme.h"

#include <QDesktopServices>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QTreeWidget>
#include <QUrl>
#include <QVBoxLayout>

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

FavoritesPage::FavoritesPage(QWidget* parent)
    : QWidget(parent)
{
    buildUi();
}

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

    m_refreshBtn = new QPushButton(QStringLiteral("刷新收藏夹"), this);
    m_refreshBtn->setObjectName(QStringLiteral("primaryButton"));
    connect(m_refreshBtn, &QPushButton::clicked, this, &FavoritesPage::refreshRequested);
    headerLayout->addWidget(m_refreshBtn);

    mainLayout->addLayout(headerLayout);

    m_subtitle = new QLabel(QStringLiteral("点击刷新收藏夹加载数据"), this);
    m_subtitle->setStyleSheet(QStringLiteral("color: %1; font-size: 12px;").arg(theme::TEXT_3));
    mainLayout->addWidget(m_subtitle);

    m_tree = new QTreeWidget(this);
    m_tree->setObjectName(QStringLiteral("FavoritesTree"));
    m_tree->setHeaderHidden(true);
    m_tree->setColumnCount(3);
    m_tree->setColumnWidth(0, 420);
    m_tree->setColumnWidth(1, 140);
    m_tree->setColumnWidth(2, 100);
    m_tree->setIndentation(20);
    m_tree->setFrameShape(QFrame::NoFrame);
    m_tree->setStyleSheet(QStringLiteral(
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

    connect(m_tree, &QTreeWidget::itemDoubleClicked, this, &FavoritesPage::onItemDoubleClicked);
    connect(m_tree, &QTreeWidget::itemExpanded, this, &FavoritesPage::onItemExpanded);
    mainLayout->addWidget(m_tree, 1);

    animation::installButtonScaleAnimation(m_refreshBtn);
}

void FavoritesPage::refresh(const QString& cookie)
{
    if (m_fetching) {
        cancel();
    }

    if (cookie.isEmpty()) {
        emit error(QStringLiteral("未设置 Cookie，无法加载收藏夹"));
        return;
    }

    m_cookie = cookie;
    m_folders.clear();
    m_resources.clear();
    m_pendingFolderIndex = 0;
    m_fetching = true;
    clearTree();
    updateSubtitle();

    if (!m_client) {
        m_client = new ApiClient(this);
    }
    if (!m_fetcher) {
        m_fetcher = new FavoritesFetcher(m_client, this);
        connect(m_fetcher, &FavoritesFetcher::foldersFinished,
                this, &FavoritesPage::onFoldersFinished);
        connect(m_fetcher, &FavoritesFetcher::resourcesFinished,
                this, &FavoritesPage::onResourcesFinished);
        connect(m_fetcher, &FavoritesFetcher::error,
                this, &FavoritesPage::onFetcherError);
    }

    emit statusChanged(QStringLiteral("正在加载收藏夹..."));
    m_fetcher->fetchFolders(cookie);
}

void FavoritesPage::cancel()
{
    if (m_fetcher && m_fetching) {
        m_fetcher->cancel();
    }
    m_fetching = false;
    updateSubtitle();
}

void FavoritesPage::clearTree()
{
    m_tree->clear();
}

void FavoritesPage::updateSubtitle()
{
    const int folderCount = static_cast<int>(m_folders.size());
    int resourceCount = 0;
    for (const auto& folder : m_folders) {
        resourceCount += static_cast<int>(folder.items.size());
    }

    if (m_fetching) {
        m_subtitle->setText(QStringLiteral("共 %1 个收藏夹，已加载 %2 条内容")
                                .arg(folderCount)
                                .arg(resourceCount));
    } else {
        m_subtitle->setText(QStringLiteral("共 %1 个收藏夹，%2 条内容")
                                .arg(folderCount)
                                .arg(resourceCount));
    }
}

void FavoritesPage::onFoldersFinished(const bili::FavoriteFolderList& folders)
{
    m_folders = folders;
    for (const auto& folder : m_folders) {
        m_resources[QString::number(folder.id)] = {};
    }
    renderFolders();
    loadNextFolderResources();
}

void FavoritesPage::renderFolders()
{
    clearTree();
    for (const auto& folder : m_folders) {
        auto* item = new QTreeWidgetItem(m_tree);
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
    if (m_pendingFolderIndex >= m_folders.size()) {
        m_fetching = false;
        updateSubtitle();
        emit statusChanged(QStringLiteral("收藏夹加载完成"));
        return;
    }

    const qint64 folderId = m_folders[m_pendingFolderIndex].id;
    emit statusChanged(QStringLiteral("正在加载收藏夹 %1 / %2")
                           .arg(m_pendingFolderIndex + 1)
                           .arg(m_folders.size()));
    m_fetcher->fetchResources(QString::number(folderId), 20, m_cookie);
}

void FavoritesPage::onResourcesFinished(const QString& folderId,
                                        const std::vector<bili::FavoriteItem>& items)
{
    m_resources[folderId] = items;

    // 找到对应文件夹节点并填充内容
    const qint64 id = folderId.toLongLong();
    for (int i = 0; i < m_tree->topLevelItemCount(); ++i) {
        auto* folderItem = m_tree->topLevelItem(i);
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

    ++m_pendingFolderIndex;
    updateSubtitle();
    loadNextFolderResources();
}

void FavoritesPage::onFetcherError(const bili::NetworkException& e)
{
    m_fetching = false;
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
