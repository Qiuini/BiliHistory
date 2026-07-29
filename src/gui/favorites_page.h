#pragma once

#include "core/exceptions.h"
#include "core/models.h"

#include <QTreeWidget>
#include <QWidget>

class QLabel;
class QPushButton;
class QTreeWidgetItem;

namespace bili {
class ApiClient;
class FavoritesFetcher;
} // namespace bili

namespace bili::gui {

class FavoritesPage : public QWidget {
    Q_OBJECT
public:
    explicit FavoritesPage(QWidget* parent = nullptr);

public slots:
    void refresh(const QString& cookie);
    void cancel();

signals:
    void statusChanged(const QString& message);
    void error(const QString& message);
    void refreshRequested();

private slots:
    void onFoldersFinished(const bili::FavoriteFolderList& folders);
    void onResourcesFinished(const QString& folderId, const std::vector<bili::FavoriteItem>& items);
    void onFetcherError(const bili::NetworkException& e);
    void onItemDoubleClicked(QTreeWidgetItem* item, int column);
    void onItemExpanded(QTreeWidgetItem* item);

private:
    void buildUi();
    void clearTree();
    void renderFolders();
    void loadNextFolderResources();
    void updateSubtitle();

    QLabel* m_subtitle = nullptr;
    QTreeWidget* m_tree = nullptr;
    QPushButton* m_refreshBtn = nullptr;

    bili::ApiClient* m_client = nullptr;
    bili::FavoritesFetcher* m_fetcher = nullptr;
    bili::FavoriteFolderList m_folders;
    std::map<QString, std::vector<bili::FavoriteItem>> m_resources;
    QString m_cookie;
    size_t m_pendingFolderIndex = 0;
    bool m_fetching = false;
};

} // namespace bili::gui
