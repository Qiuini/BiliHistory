#include "image_loader.h"

#include "core/i_config.h"
#include "core/paths.h"

#include <QCache>
#include <QCryptographicHash>
#include <QDir>
#include <QFile>
#include <QImage>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QRandomGenerator>
#include <QThread>

#include <QtGlobal>

namespace bili::gui {

namespace {

QString cacheKey(const QUrl& url)
{
    return QCryptographicHash::hash(url.toEncoded(), QCryptographicHash::Sha256).toHex();
}

QString diskPath(const QString& key)
{
    return QDir(Paths::imageCacheDir()).filePath(key + QStringLiteral(".png"));
}

// 默认 UA：config 缺省时的回退值，与 Config::loadDefaults 中的 user_agent 一致。
const QString kDefaultUserAgent = QStringLiteral(
    "Mozilla/5.0 (Windows NT 10.0; Win64; x64) "
    "AppleWebKit/537.36 (KHTML, like Gecko) "
    "Chrome/120.0.0.0 Safari/537.36");

// 从 config 选取 UA：提供 config 时随机选 userAgents()，否则使用默认 UA。
QString pickUserAgent(bili::IConfig* config)
{
    if (config != nullptr) {
        const QStringList agents = config->userAgents();
        if (!agents.isEmpty()) {
            return agents.at(QRandomGenerator::global()->bounded(agents.size()));
        }
    }
    return kDefaultUserAgent;
}

} // namespace

// ---------------------------------------------------------------------------
// Worker: runs in a dedicated QThread. Performs disk IO, network and decoding.
// ---------------------------------------------------------------------------
class ImageLoaderWorker : public QObject {
    Q_OBJECT
public:
    explicit ImageLoaderWorker(QString userAgent, QObject* parent = nullptr)
        : QObject(parent)
        , m_manager(new QNetworkAccessManager(this))
        , m_userAgent(std::move(userAgent))
    {
    }

public slots:
    void doLoad(QUrl url, qint64 requestId)
    {
        const QString key = cacheKey(url);
        const QString path = diskPath(key);

        if (QFile::exists(path)) {
            QImage image;
            if (image.load(path)) {
                emit imageLoaded(requestId, key, image);
                return;
            }
        }

        QNetworkRequest request(url);
        request.setAttribute(QNetworkRequest::RedirectPolicyAttribute,
                             QNetworkRequest::NoLessSafeRedirectPolicy);
        request.setHeader(QNetworkRequest::UserAgentHeader, m_userAgent);
        request.setRawHeader(QByteArrayLiteral("Referer"), QByteArrayLiteral("https://www.bilibili.com/"));

        QNetworkReply* reply = m_manager->get(request);
        connect(reply, &QNetworkReply::finished, this, [this, reply, requestId, url, key]() {
            reply->deleteLater();
            if (reply->error() != QNetworkReply::NoError) {
                emit imageFailed(requestId, QStringLiteral("network error: %1").arg(reply->errorString()));
                return;
            }

            const QByteArray data = reply->readAll();
            QImage image;
            if (!image.loadFromData(data)) {
                emit imageFailed(requestId, QStringLiteral("invalid image data from %1").arg(url.toString()));
                return;
            }

            QDir dir(Paths::imageCacheDir());
            dir.mkpath(QStringLiteral("."));
            image.save(diskPath(key), "PNG");

            emit imageLoaded(requestId, key, image);
        });
    }

signals:
    void imageLoaded(qint64 requestId, const QString& key, const QImage& image);
    void imageFailed(qint64 requestId, const QString& reason);

private:
    QNetworkAccessManager* m_manager = nullptr;
    QString m_userAgent;
};

// ---------------------------------------------------------------------------
// ImageLoader::Impl: lives in GUI thread, owns memory cache and callbacks.
// ---------------------------------------------------------------------------
class ImageLoader::Impl {
public:
    explicit Impl(ImageLoader* q_, bili::IConfig* config)
        : q(q_)
        , thread(new QThread(q_))
        , memoryCache(200)
    {
        worker = new ImageLoaderWorker(pickUserAgent(config));
        worker->moveToThread(thread);
        QObject::connect(thread, &QThread::finished, worker, &QObject::deleteLater);
        QObject::connect(worker, &ImageLoaderWorker::imageLoaded,
                         q, &ImageLoader::onImageLoaded);
        QObject::connect(worker, &ImageLoaderWorker::imageFailed,
                         q, &ImageLoader::onImageFailed);
        thread->start();
    }

    ~Impl()
    {
        thread->quit();
        thread->wait();
    }

    void load(const QUrl& url, std::function<void(QPixmap)> callback, QSize targetSize)
    {
        if (!url.isValid() || url.isEmpty()) {
            return;
        }

        const QString key = cacheKey(url);

        if (QPixmap* cached = memoryCache.object(key)) {
            QPixmap pix = targetSize.isValid()
                ? cached->scaled(targetSize, Qt::KeepAspectRatioByExpanding, Qt::SmoothTransformation)
                : *cached;
            if (callback) callback(pix);
            return;
        }

        auto it = inFlightByKey.find(key);
        if (it != inFlightByKey.end()) {
            pendingById[it.value()].callbacks.append({ std::move(callback), targetSize });
            return;
        }

        const qint64 requestId = nextRequestId++;
        inFlightByKey[key] = requestId;

        Pending pending;
        pending.key = key;
        pending.callbacks.append({ std::move(callback), targetSize });
        pendingById[requestId] = std::move(pending);

        QMetaObject::invokeMethod(worker, "doLoad", Qt::QueuedConnection,
                                  Q_ARG(QUrl, url), Q_ARG(qint64, requestId));
    }

    void clearMemory()
    {
        memoryCache.clear();
    }

    void clearDisk()
    {
        QDir dir(Paths::imageCacheDir());
        if (!dir.exists()) return;
        const auto entries = dir.entryList(QDir::Files);
        for (const QString& name : entries) {
            dir.remove(name);
        }
    }

    int memoryCacheSize() const
    {
        return memoryCache.totalCost();
    }

    void handleLoaded(qint64 requestId, const QString& key, const QImage& image)
    {
        auto it = pendingById.find(requestId);
        if (it == pendingById.end()) return;

        Pending pending = std::move(it.value());
        pendingById.erase(it);
        inFlightByKey.remove(key);

        if (image.isNull()) {
            emit q->failed(QStringLiteral("decoded image is null"));
            return;
        }

        const QPixmap pixmap = QPixmap::fromImage(image);
        memoryCache.insert(key, new QPixmap(pixmap), 1);

        for (const auto& cb : pending.callbacks) {
            QPixmap pix = pixmap;
            if (cb.second.isValid()) {
                pix = pixmap.scaled(cb.second, Qt::KeepAspectRatioByExpanding, Qt::SmoothTransformation);
            }
            if (cb.first) cb.first(pix);
        }
        emit q->loaded(pixmap);
    }

    void handleFailed(qint64 requestId, const QString& reason)
    {
        auto it = pendingById.find(requestId);
        if (it == pendingById.end()) return;

        const QString key = it.value().key;
        pendingById.erase(it);
        inFlightByKey.remove(key);

        emit q->failed(reason);
    }

private:
    struct Pending {
        QString key;
        QList<std::pair<std::function<void(QPixmap)>, QSize>> callbacks;
    };

    ImageLoader* q = nullptr;
    QThread* thread = nullptr;
    ImageLoaderWorker* worker = nullptr;
    QCache<QString, QPixmap> memoryCache;
    QHash<qint64, Pending> pendingById;
    QHash<QString, qint64> inFlightByKey;
    qint64 nextRequestId = 1;
};

ImageLoader::ImageLoader(IConfig* config, QObject* parent)
    : QObject(parent)
    , d(std::make_unique<Impl>(this, config))
{
}

ImageLoader::~ImageLoader() = default;

void ImageLoader::load(const QUrl& url, std::function<void(QPixmap)> callback, QSize targetSize)
{
    d->load(url, std::move(callback), targetSize);
}

void ImageLoader::clearMemory()
{
    d->clearMemory();
}

void ImageLoader::clearDisk()
{
    d->clearDisk();
}

int ImageLoader::memoryCacheSize() const
{
    return d->memoryCacheSize();
}

void ImageLoader::onImageLoaded(qint64 requestId, const QString& key, const QImage& image)
{
    d->handleLoaded(requestId, key, image);
}

void ImageLoader::onImageFailed(qint64 requestId, const QString& reason)
{
    d->handleFailed(requestId, reason);
}

} // namespace bili::gui

#include "image_loader.moc"
