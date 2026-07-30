#pragma once

#include "core/exceptions.h"
#include "core/models.h"
#include "network/i_fetcher.h"
#include "core/i_config.h"

#include <QWidget>

#include <memory>
#include <vector>

class QLabel;
class QPushButton;
class QTreeWidgetItem;

namespace bili::gui {

class FavoritesPage : public QWidget {
    Q_OBJECT
public:
    explicit FavoritesPage(bili::IConfig* config, bili::IFavoritesFetcher* fetcher, QWidget* parent = nullptr);
    ~FavoritesPage() override;

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

    class Impl;
    std::unique_ptr<Impl> d;
};

} // namespace bili::gui
