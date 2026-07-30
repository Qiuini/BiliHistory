#pragma once

#include <QObject>
#include <QPixmap>
#include <QSize>
#include <QUrl>

#include <functional>
#include <memory>

namespace bili { class IConfig; }

namespace bili::gui {

// 异步图片加载器：网络请求、磁盘 IO 与图片解码均在独立工作线程执行，
// GUI 线程仅保留内存缓存与回调分发。
class ImageLoader : public QObject {
    Q_OBJECT
public:
    // config 可空：提供时从 config->userAgents() 随机选取 UA，否则使用默认 UA。
    explicit ImageLoader(IConfig* config = nullptr, QObject* parent = nullptr);
    ~ImageLoader() override;

    void load(const QUrl& url, std::function<void(QPixmap)> callback, QSize targetSize = {});
    void clearMemory();
    void clearDisk();

    // 测试辅助：当前内存缓存条目数
    int memoryCacheSize() const;

signals:
    void loaded(const QPixmap& pixmap);
    void failed(const QString& reason);

private slots:
    void onImageLoaded(qint64 requestId, const QString& key, const QImage& image);
    void onImageFailed(qint64 requestId, const QString& reason);

private:
    class Impl;
    std::unique_ptr<Impl> d;
};

} // namespace bili::gui
