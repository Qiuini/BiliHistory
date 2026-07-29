#pragma once

#include <QCache>
#include <QHash>
#include <QNetworkAccessManager>
#include <QObject>
#include <QPixmap>
#include <QSize>
#include <QUrl>

#include <functional>

namespace bili::gui {

class ImageLoader : public QObject {
    Q_OBJECT
public:
    explicit ImageLoader(QObject* parent = nullptr);
    ~ImageLoader() override;

    void load(const QUrl& url, std::function<void(QPixmap)> callback, QSize targetSize = {});
    void clearMemory();
    void clearDisk();

    // 测试辅助：当前内存缓存条目数
    int memoryCacheSize() const;

signals:
    void loaded(const QPixmap& pixmap);
    void failed(const QString& reason);

private:
    void startNetworkRequest(const QUrl& url, const QString& key);
    void deliver(const QString& key, const QPixmap& pixmap);
    void fail(const QString& key, const QString& reason);

    static QString cacheKey(const QUrl& url);
    static QString diskPath(const QString& key);

    struct Pending {
        QList<std::pair<std::function<void(QPixmap)>, QSize>> callbacks;
    };

    QNetworkAccessManager* m_manager = nullptr;
    QCache<QString, QPixmap> m_memoryCache;
    QHash<QString, Pending> m_pending;
};

} // namespace bili::gui
