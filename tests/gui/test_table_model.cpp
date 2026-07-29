#include <gtest/gtest.h>

#include <QCoreApplication>

#include "core/models.h"
#include "gui/history_table_model.h"

using namespace bili;

class HistoryTableModelTest : public ::testing::Test {
protected:
    void SetUp() override {
        model = new gui::HistoryTableModel();
    }
    void TearDown() override {
        delete model;
    }
    gui::HistoryTableModel* model = nullptr;
};

static RecordPtr makeVideo(const QString& bv, const QString& title,
                           const QString& author, int percent) {
    auto r = std::make_shared<VideoRecord>();
    r->type = RecordType::Video;
    r->bvId = bv;
    r->bvid = bv;
    r->title = title;
    r->authorName = author;
    r->progressPercent = percent;
    r->viewAt = QDateTime::currentDateTime();
    return r;
}

TEST_F(HistoryTableModelTest, setRowsEmitsReset) {
    int resetCount = 0;
    QObject::connect(model, &gui::HistoryTableModel::modelReset, [&resetCount]() {
        ++resetCount;
    });

    RecordList rows;
    rows.push_back(makeVideo(QStringLiteral("BV1"), QStringLiteral("A"),
                             QStringLiteral("Up"), 10));
    model->setRows(rows);

    EXPECT_EQ(model->rowCount(), 1);
    EXPECT_EQ(model->columnCount(), 8);
    EXPECT_EQ(resetCount, 1);
}

TEST_F(HistoryTableModelTest, updateRowsInsertRemoveUpdate) {
    int inserted = 0;
    int removed = 0;
    int changed = 0;
    QObject::connect(model, &gui::HistoryTableModel::rowsInserted,
                     [&](const QModelIndex&, int, int) { ++inserted; });
    QObject::connect(model, &gui::HistoryTableModel::rowsRemoved,
                     [&](const QModelIndex&, int, int) { ++removed; });
    QObject::connect(model, &gui::HistoryTableModel::dataChanged,
                     [&](const QModelIndex&, const QModelIndex&) { ++changed; });

    RecordList rows;
    rows.push_back(makeVideo(QStringLiteral("BV1"), QStringLiteral("A"),
                             QStringLiteral("Up1"), 10));
    rows.push_back(makeVideo(QStringLiteral("BV2"), QStringLiteral("B"),
                             QStringLiteral("Up2"), 20));
    model->setRows(rows);

    RecordList rows2;
    rows2.push_back(makeVideo(QStringLiteral("BV2"), QStringLiteral("B"),
                              QStringLiteral("Up2"), 50));
    rows2.push_back(makeVideo(QStringLiteral("BV3"), QStringLiteral("C"),
                              QStringLiteral("Up3"), 30));
    model->updateRows(rows2);

    EXPECT_EQ(model->rowCount(), 2);
    EXPECT_EQ(inserted, 1);
    EXPECT_EQ(removed, 1);
    EXPECT_GE(changed, 1);

    EXPECT_EQ(model->data(model->index(0, 0)).toString(), QStringLiteral("视频"));
    EXPECT_EQ(model->data(model->index(0, 1)).toString(), QStringLiteral("B"));
    EXPECT_EQ(model->data(model->index(1, 1)).toString(), QStringLiteral("C"));
}

TEST_F(HistoryTableModelTest, setNewIdsAndHoverRow) {
    RecordList rows;
    rows.push_back(makeVideo(QStringLiteral("BV1"), QStringLiteral("A"),
                             QStringLiteral("Up"), 10));
    rows.push_back(makeVideo(QStringLiteral("BV2"), QStringLiteral("B"),
                             QStringLiteral("Up"), 20));
    model->setRows(rows);

    int changed = 0;
    QObject::connect(model, &gui::HistoryTableModel::dataChanged,
                     [&](const QModelIndex&, const QModelIndex&) { ++changed; });

    QSet<QString> newIds;
    newIds.insert(QStringLiteral("video:BV1"));
    model->setNewIds(newIds);
    EXPECT_GT(changed, 0);
    EXPECT_TRUE(model->data(model->index(0, 0), gui::HistoryTableModel::NewRole).toBool());
    EXPECT_FALSE(model->data(model->index(1, 0), gui::HistoryTableModel::NewRole).toBool());

    changed = 0;
    model->setHoverRow(1);
    EXPECT_GT(changed, 0);
    EXPECT_TRUE(model->data(model->index(1, 0), gui::HistoryTableModel::HoverRole).toBool());
    EXPECT_FALSE(model->data(model->index(0, 0), gui::HistoryTableModel::HoverRole).toBool());
}

TEST_F(HistoryTableModelTest, customRoles) {
    auto r = makeVideo(QStringLiteral("BV1"), QStringLiteral("A"),
                       QStringLiteral("Up"), 42);
    r->progress = QStringLiteral("12:34");
    RecordList rows;
    rows.push_back(r);
    model->setRows(rows);

    EXPECT_EQ(model->data(model->index(0, 0), gui::HistoryTableModel::TypeRole).toString(),
              QStringLiteral("视频"));
    EXPECT_EQ(model->data(model->index(0, 0), gui::HistoryTableModel::ProgressRole).toInt(), 42);
    EXPECT_EQ(model->data(model->index(0, 0), gui::HistoryTableModel::ProgressTextRole).toString(),
              QStringLiteral("12:34"));
    EXPECT_EQ(model->data(model->index(0, 0), gui::HistoryTableModel::LinkRole).toString(),
              QStringLiteral("video:BV1"));
}
