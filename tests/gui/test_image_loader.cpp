#include <gtest/gtest.h>

#include <QApplication>
#include <QColor>
#include <QDir>
#include <QFile>
#include <QPainter>
#include <QPixmap>
#include <QTemporaryDir>
#include <QUrl>

#include "gui/image_loader.h"

using namespace bili;

class ImageLoaderTest : public ::testing::Test {
protected:
    void SetUp() override {
        tempDir = std::make_unique<QTemporaryDir>();
        ASSERT_TRUE(tempDir->isValid());
        loader = std::make_unique<gui::ImageLoader>();
    }
    void TearDown() override {
        loader->clearDisk();
        loader.reset();
    }

    static QString createImageFile(const QString& dir, const QString& name, const QColor& color) {
        QPixmap pix(100, 100);
        pix.fill(color);
        const QString path = QDir(dir).filePath(name);
        pix.save(path);
        return path;
    }

    std::unique_ptr<QTemporaryDir> tempDir;
    std::unique_ptr<gui::ImageLoader> loader;
};

TEST_F(ImageLoaderTest, emptyUrlNoop) {
    bool called = false;
    loader->load(QUrl(), [&called](const QPixmap&) { called = true; });
    EXPECT_FALSE(called);
}

TEST_F(ImageLoaderTest, memoryCacheHit) {
    const QString path = createImageFile(tempDir->path(), QStringLiteral("a.png"), Qt::red);
    int count = 0;
    loader->load(QUrl::fromLocalFile(path), [&count](const QPixmap&) { ++count; });
    QApplication::processEvents();
    EXPECT_EQ(count, 1);

    loader->load(QUrl::fromLocalFile(path), [&count](const QPixmap&) { ++count; });
    EXPECT_EQ(count, 2);
}

TEST_F(ImageLoaderTest, diskCacheRestore) {
    const QString path = createImageFile(tempDir->path(), QStringLiteral("b.png"), Qt::blue);
    int count = 0;
    loader->load(QUrl::fromLocalFile(path), [&count](const QPixmap&) { ++count; });
    QApplication::processEvents();
    EXPECT_EQ(count, 1);

    loader->clearMemory();
    loader->load(QUrl::fromLocalFile(path), [&count](const QPixmap&) { ++count; });
    EXPECT_EQ(count, 2);
}

TEST_F(ImageLoaderTest, lruEviction) {
    for (int i = 0; i < 201; ++i) {
        const QString p = createImageFile(tempDir->path(),
                                          QStringLiteral("img%1.png").arg(i),
                                          QColor::fromHsv((i * 17) % 360, 200, 200));
        bool called = false;
        loader->load(QUrl::fromLocalFile(p), [&called](const QPixmap&) { called = true; });
        QApplication::processEvents();
        EXPECT_TRUE(called);
    }
    EXPECT_LE(loader->memoryCacheSize(), 200);
}

TEST_F(ImageLoaderTest, requestDeduplication) {
    const QString path = createImageFile(tempDir->path(), QStringLiteral("dup.png"), Qt::green);
    int count = 0;
    loader->load(QUrl::fromLocalFile(path), [&count](const QPixmap&) { ++count; });
    loader->load(QUrl::fromLocalFile(path), [&count](const QPixmap&) { ++count; });
    QApplication::processEvents();
    EXPECT_EQ(count, 2);
}

TEST_F(ImageLoaderTest, failureHandling) {
    const QString invalid = tempDir->filePath(QStringLiteral("notanimage.txt"));
    QFile f(invalid);
    ASSERT_TRUE(f.open(QIODevice::WriteOnly));
    f.write("not image");
    f.close();

    int callbackCount = 0;
    int failCount = 0;
    QObject::connect(loader.get(), &gui::ImageLoader::failed,
                     [&failCount](const QString&) { ++failCount; });
    loader->load(QUrl::fromLocalFile(invalid), [&callbackCount](const QPixmap&) { ++callbackCount; });
    QApplication::processEvents();

    EXPECT_EQ(callbackCount, 0);
    EXPECT_EQ(failCount, 1);
}
