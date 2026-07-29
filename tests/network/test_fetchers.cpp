#include <gtest/gtest.h>

#include <QEventLoop>
#include <QObject>
#include <QTimer>

#include "api_client.h"
#include "config.h"
#include "fetchers.h"
#include "test_http_server.h"

using namespace bili;

namespace {

class FetchersTest : public ::testing::Test {
protected:
    void SetUp() override {
        Config::instance().loadDefaults();
        Config::instance().setValue(QStringLiteral("retry_wait_ms"), 50);
        Config::instance().setValue(QStringLiteral("http_backoff_factor"), 0.5);
        Config::instance().setValue(QStringLiteral("http_total_retries"), 1);
        Config::instance().setValue(QStringLiteral("page_size"), 5);

        m_server = std::make_unique<TestHttpServer>();
        ASSERT_TRUE(m_server->start());

        const QString base = QStringLiteral("http://127.0.0.1:%1").arg(m_server->serverPort());
        Config::instance().setValue(QStringLiteral("base_url"), base);

        m_api = std::make_unique<ApiClient>();
    }

    void TearDown() override {
        m_api.reset();
        m_server.reset();
    }

    std::unique_ptr<TestHttpServer> m_server;
    std::unique_ptr<ApiClient> m_api;
};

} // namespace

TEST_F(FetchersTest, HistoryFetcherPagination) {
    const QByteArray page1 = R"({
        "code": 0,
        "data": {
            "list": [{
                "title": "视频1",
                "view_at": 1700000000,
                "duration": 120,
                "progress": 60,
                "author_name": "UP主",
                "cover": "cover.jpg",
                "history": {"business": "archive", "bvid": "BV1xx411c7mD"},
                "tname": "动画"
            }],
            "cursor": {"max": 12345, "view_at": 1700000001, "business": "archive"}
        }
    })";
    const QByteArray page2 = R"({
        "code": 0,
        "data": {
            "list": [{
                "title": "视频2",
                "view_at": 1699999999,
                "duration": 60,
                "progress": 30,
                "author_name": "UP主2",
                "cover": "cover2.jpg",
                "history": {"business": "archive", "bvid": "BV2yy411c7mD"},
                "tname": "游戏"
            }],
            "cursor": {"max": 0, "view_at": 0, "business": ""}
        }
    })";

    m_server->enqueueResponse(200, page1);
    m_server->enqueueResponse(200, page2);

    HistoryFetcher fetcher(m_api.get());

    bool finished = false;
    int pageCount = 0;
    int progressTotal = 0;
    RecordList allRecords;

    QEventLoop loop;
    QObject::connect(&fetcher, &HistoryFetcher::pageFetched, &loop, [&](const RecordList&, int page, int) {
        pageCount = page;
    });
    QObject::connect(&fetcher, &HistoryFetcher::progress, &loop, [&](int total) {
        progressTotal = total;
    });
    QObject::connect(&fetcher, &HistoryFetcher::finished, &loop, [&](const RecordList& records) {
        finished = true;
        allRecords = records;
        loop.quit();
    });
    QObject::connect(&fetcher, &HistoryFetcher::error, &loop, &QEventLoop::quit);

    fetcher.fetchAll(QStringLiteral("SESSDATA=test"));

    QTimer::singleShot(15000, &loop, &QEventLoop::quit);
    loop.exec();

    ASSERT_TRUE(finished);
    EXPECT_EQ(allRecords.size(), 2u);
    EXPECT_EQ(pageCount, 2);
    EXPECT_EQ(progressTotal, 2);
}

TEST_F(FetchersTest, FollowingFetcherPagination) {
    const QByteArray page1 = R"({
        "code": 0,
        "data": {
            "list": [{"mid": 1, "uname": "UP主A", "sign": "", "face": "", "level": 5}],
            "total": 100
        }
    })";
    const QByteArray page2 = R"({
        "code": 0,
        "data": {
            "list": [{"mid": 2, "uname": "UP主B", "sign": "", "face": "", "level": 6}],
            "total": 100
        }
    })";

    m_server->enqueueResponse(200, page1);
    m_server->enqueueResponse(200, page2);

    FollowingFetcher fetcher(m_api.get());

    bool finished = false;
    FollowingList allUsers;

    QEventLoop loop;
    QObject::connect(&fetcher, &FollowingFetcher::finished, &loop, [&](const FollowingList& users) {
        finished = true;
        allUsers = users;
        loop.quit();
    });
    QObject::connect(&fetcher, &FollowingFetcher::error, &loop, &QEventLoop::quit);

    fetcher.fetchAll(QStringLiteral("123"), QStringLiteral("SESSDATA=test"));

    QTimer::singleShot(15000, &loop, &QEventLoop::quit);
    loop.exec();

    ASSERT_TRUE(finished);
    ASSERT_EQ(allUsers.size(), 2u);
    EXPECT_EQ(allUsers[0].name, QStringLiteral("UP主A"));
    EXPECT_EQ(allUsers[1].level, 6);
}

TEST_F(FetchersTest, HistoryFetcherCancel) {
    m_server->enqueueResponse(200, QByteArrayLiteral("{}"), 5000);

    HistoryFetcher fetcher(m_api.get());

    bool cancelled = false;

    QEventLoop loop;
    QObject::connect(&fetcher, &HistoryFetcher::finished, &loop, &QEventLoop::quit);
    QObject::connect(&fetcher, &HistoryFetcher::error, &loop, &QEventLoop::quit);
    QObject::connect(&fetcher, &HistoryFetcher::cancelled, &loop, [&]() {
        cancelled = true;
        loop.quit();
    });

    fetcher.fetchAll(QStringLiteral("SESSDATA=test"));
    QTimer::singleShot(100, &fetcher, &HistoryFetcher::cancel);
    QTimer::singleShot(10000, &loop, &QEventLoop::quit);
    loop.exec();

    EXPECT_TRUE(cancelled);
}
