#include "image_loader.h"

#include "core/paths.h"

#include <QDir>
#include <QFile>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QCryptographicHash>

namespace bili::gui {

ImageLoader::ImageLoader(QObject* parent)
    : QObject(parent)
    , m_manager(new QNetworkAccessManager(this))
    , m_memoryCache(200)
{
}

ImageLoader::~ImageLoader() = default;

void ImageLoader::load(const QUrl& url, std::function<void(QPixmap)> callback, QSize targetSize)
{
    if (!url.isValid() || url.isEmpty()) {
        return;
    }

    const QString key = cacheKey(url);

    if (QPixmap* cached = m_memoryCache.object(key)) {
        QPixmap pix = targetSize.isValid()
            ? cached->scaled(targetSize, Qt::KeepAspectRatioByExpanding, Qt::SmoothTransformation)
            : *cached;
        if (callback) callback(pix);
        return;
    }

    const QString path = diskPath(key);
    if (QFile::exists(path)) {
        QPixmap pix;
        if (pix.load(path)) {
            m_memoryCache.insert(key, new QPixmap(pix), 1);
            if (targetSize.isValid()) {
                pix = pix.scaled(targetSize, Qt::KeepAspectRatioByExpanding, Qt::SmoothTransformation);
            }
            if (callback) callback(pix);
            return;
        }
    }

    if (m_pending.contains(key)) {
        m_pending[key].callbacks.append({ std::move(callback), targetSize });
        return;
    }

    Pending pending;
    pending.callbacks.append({ std::move(callback), targetSize });
    m_pending[key] = std::move(pending);
    startNetworkRequest(url, key);
}

void ImageLoader::startNetworkRequest(const QUrl& url, const QString& key)
{
    QNetworkRequest request(url);
    request.setAttribute(QNetworkRequest::RedirectPolicyAttribute, QNetworkRequest::NoLessSafeRedirectPolicy);
    request.setHeader(QNetworkRequest::UserAgentHeader,
                      QStringLiteral("Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/120.0.0.0 Safari/537.36"));
    request.setRawHeader(QByteArrayLiteral("Referer"), QByteArrayLiteral("https://www.bilibili.com/"));

    QNetworkReply* reply = m_manager->get(request);
    connect(reply, &QNetworkReply::finished, this, [this, reply, key, url]() {
        reply->deleteLater();
        if (reply->error() != QNetworkReply::NoError) {
            fail(key, QStringLiteral("network error: %1").arg(reply->errorString()));
            return;
        }

        const QByteArray data = reply->readAll();
        QPixmap pixmap;
        if (!pixmap.loadFromData(data)) {
            fail(key, QStringLiteral("invalid image data from %1").arg(url.toString()));
            return;
        }

        const QString path = diskPath(key);
        QDir dir(Paths::imageCacheDir());
        dir.mkpath(QStringLiteral("."));
        pixmap.save(path, "PNG");

        m_memoryCache.insert(key, new QPixmap(pixmap), 1);
        deliver(key, pixmap);
    });
}

void ImageLoader::deliver(const QString& key, const QPixmap& pixmap)
{
    auto it = m_pending.find(key);
    if (it == m_pending.end()) return;

    Pending pending = std::move(it.value());
    m_pending.erase(it);

    for (const auto& cb : pending.callbacks) {
        QPixmap pix = pixmap;
        if (cb.second.isValid()) {
            pix = pixmap.scaled(cb.second, Qt::KeepAspectRatioByExpanding, Qt::SmoothTransformation);
        }
        if (cb.first) cb.first(pix);
    }
    emit loaded(pixmap);
}

void ImageLoader::fail(const QString& key, const QString& reason)
{
    auto it = m_pending.find(key);
    if (it != m_pending.end()) {
        m_pending.erase(it);
    }
    emit failed(reason);
}

void ImageLoader::clearMemory()
{
    m_memoryCache.clear();
}

void ImageLoader::clearDisk()
{
    QDir dir(Paths::imageCacheDir());
    if (!dir.exists()) return;
    const auto entries = dir.entryList(QDir::Files);
    for (const QString& name : entries) {
        dir.remove(name);
    }
}

int ImageLoader::memoryCacheSize() const
{
    return m_memoryCache.totalCost();
}

QString ImageLoader::cacheKey(const QUrl& url)
{
    return QCryptographicHash::hash(url.toEncoded(), QCryptographicHash::Sha256).toHex();
}

QString ImageLoader::diskPath(const QString& key)
{
    return QDir(Paths::imageCacheDir()).filePath(key + QStringLiteral(".png"));
}

} // namespace bili::gui
