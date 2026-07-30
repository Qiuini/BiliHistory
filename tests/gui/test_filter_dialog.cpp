#include <gtest/gtest.h>

#include <QAbstractSpinBox>
#include <QCheckBox>
#include <QComboBox>
#include <QDateTime>
#include <QDialog>
#include <QGroupBox>
#include <QLineEdit>
#include <QSpinBox>
#include <QStringList>

#include "business/filter.h"
#include "core/models.h"
#include "gui/filter_dialog.h"

using namespace bili;

namespace {

RecordPtr makeVideo(const QString& title,
                    const QString& author,
                    const QString& category)
{
    auto r = std::make_shared<VideoRecord>();
    r->type = RecordType::Video;
    r->bvId = title;
    r->bvid = title;
    r->title = title;
    r->authorName = author;
    r->category = category;
    r->viewAt = QDateTime::currentDateTime();
    return r;
}

// collectCategories / collectAuthors 是 filter_dialog.cpp 内匿名命名空间中的文件局部函数，
// 无法直接调用。但它们的输出会被填充到 FilterDialog 的两个 QComboBox 中。
// 这里通过 findChildren 找到对应 combo 并校验其条目，间接验证收集逻辑。
QStringList comboItems(const QComboBox* combo)
{
    QStringList items;
    for (int i = 0; i < combo->count(); ++i) {
        items << combo->itemText(i);
    }
    return items;
}

// 在对话框中找到包含指定条目文本的 QComboBox。
QComboBox* findComboWithItem(QDialog* dlg, const QString& itemText)
{
    const auto combos = dlg->findChildren<QComboBox*>();
    for (QComboBox* combo : combos) {
        for (int i = 0; i < combo->count(); ++i) {
            if (combo->itemText(i) == itemText) {
                return combo;
            }
        }
    }
    return nullptr;
}

} // namespace

class FilterDialogTest : public ::testing::Test {
protected:
    // 构造一组带重复分类 / UP 主的记录，用于验证去重、排序、跳过空值
    static RecordList sampleRecords()
    {
        RecordList records;
        records.push_back(makeVideo(QStringLiteral("t1"), QStringLiteral("UP_B"), QStringLiteral("科技")));
        records.push_back(makeVideo(QStringLiteral("t2"), QStringLiteral("UP_A"), QStringLiteral("动画")));
        records.push_back(makeVideo(QStringLiteral("t3"), QStringLiteral("UP_A"), QStringLiteral("动画")));   // 重复
        records.push_back(makeVideo(QStringLiteral("t4"), QStringLiteral("UP_C"), QStringLiteral("")));       // 空分类
        records.push_back(makeVideo(QStringLiteral("t5"), QStringLiteral(""), QStringLiteral("音乐")));     // 空 UP 主
        records.push_back(makeVideo(QStringLiteral("t6"), QStringLiteral("UP_B"), QStringLiteral("科技")));   // 重复
        return records;
    }
};

// collectCategories 逻辑：去重 + 排序 + 跳过空分类，并在首部插入 "全部"
TEST_F(FilterDialogTest, CategoriesCollectedDedupSortedWithAllPrefix)
{
    gui::FilterDialog dlg(sampleRecords());

    QComboBox* combo = findComboWithItem(&dlg, QStringLiteral("动画"));
    ASSERT_NE(combo, nullptr);

    const QStringList items = comboItems(combo);
    // 首项应为 "全部"，之后是按字母序去重后的非空分类
    EXPECT_EQ(items, (QStringList{
        QStringLiteral("全部"),
        QStringLiteral("动画"),
        QStringLiteral("科技"),
        QStringLiteral("音乐"),
    }));
}

// collectAuthors 逻辑：去重 + 排序 + 跳过空 UP 主
TEST_F(FilterDialogTest, AuthorsCollectedDedupSortedWithAllPrefix)
{
    gui::FilterDialog dlg(sampleRecords());

    QComboBox* combo = findComboWithItem(&dlg, QStringLiteral("UP_A"));
    ASSERT_NE(combo, nullptr);

    const QStringList items = comboItems(combo);
    EXPECT_EQ(items, (QStringList{
        QStringLiteral("全部"),
        QStringLiteral("UP_A"),
        QStringLiteral("UP_B"),
        QStringLiteral("UP_C"),
    }));
}

// 空记录集时 combo 只应包含 "全部"
TEST_F(FilterDialogTest, EmptyRecordsYieldOnlyAllItem)
{
    gui::FilterDialog dlg(RecordList{});

    const auto combos = dlg.findChildren<QComboBox*>();
    ASSERT_EQ(combos.size(), 2);
    for (QComboBox* combo : combos) {
        EXPECT_EQ(combo->count(), 1);
        EXPECT_EQ(combo->itemText(0), QStringLiteral("全部"));
    }
}

// 默认 criteria：三种类型全选、无时间、无分类/UP、进度 0-100、无关键词
TEST_F(FilterDialogTest, DefaultCriteriaHasNoFilters)
{
    gui::FilterDialog dlg(sampleRecords());

    const business::FilterCriteria c = dlg.criteria();
    EXPECT_FALSE(c.startTime.isValid());
    EXPECT_FALSE(c.endTime.isValid());
    EXPECT_TRUE(c.types.contains(RecordType::Video));
    EXPECT_TRUE(c.types.contains(RecordType::Live));
    EXPECT_TRUE(c.types.contains(RecordType::Article));
    EXPECT_TRUE(c.category.isEmpty());
    EXPECT_TRUE(c.author.isEmpty());
    EXPECT_EQ(c.minProgress, 0);
    EXPECT_EQ(c.maxProgress, 100);
    EXPECT_TRUE(c.keyword.isEmpty());
}

// 选中某个分类后，criteria.category 应返回该值
TEST_F(FilterDialogTest, CriteriaReflectsSelectedCategory)
{
    gui::FilterDialog dlg(sampleRecords());

    QComboBox* combo = findComboWithItem(&dlg, QStringLiteral("动画"));
    ASSERT_NE(combo, nullptr);
    combo->setCurrentText(QStringLiteral("动画"));

    const business::FilterCriteria c = dlg.criteria();
    EXPECT_EQ(c.category, QStringLiteral("动画"));
}

// 选中某个 UP 主后，criteria.author 应返回该值
TEST_F(FilterDialogTest, CriteriaReflectsSelectedAuthor)
{
    gui::FilterDialog dlg(sampleRecords());

    QComboBox* combo = findComboWithItem(&dlg, QStringLiteral("UP_A"));
    ASSERT_NE(combo, nullptr);
    combo->setCurrentText(QStringLiteral("UP_A"));

    const business::FilterCriteria c = dlg.criteria();
    EXPECT_EQ(c.author, QStringLiteral("UP_A"));
}

// "全部" 不应被当作筛选条件
TEST_F(FilterDialogTest, AllKeywordIsNotTreatedAsFilter)
{
    gui::FilterDialog dlg(sampleRecords());

    const auto combos = dlg.findChildren<QComboBox*>();
    for (QComboBox* combo : combos) {
        combo->setCurrentText(QStringLiteral("全部"));
    }
    const business::FilterCriteria c = dlg.criteria();
    EXPECT_TRUE(c.category.isEmpty());
    EXPECT_TRUE(c.author.isEmpty());
}

// 关键词应被 trim 后写入 criteria
TEST_F(FilterDialogTest, CriteriaKeywordIsTrimmed)
{
    gui::FilterDialog dlg(sampleRecords());

    // categoryCombo / authorCombo 都是 setEditable(true)，内部各有一个 QLineEdit；
    // QDateTimeEdit / QSpinBox 内部也包含 QLineEdit。
    // keywordEdit 有唯一的 placeholder 文本，用它来精确定位。
    const auto allEdits = dlg.findChildren<QLineEdit*>();
    QLineEdit* edit = nullptr;
    for (QLineEdit* e : allEdits) {
        if (e->placeholderText() == QStringLiteral("标题 / BV 号 / 分类 / UP 主")) {
            edit = e;
            break;
        }
    }
    ASSERT_NE(edit, nullptr);
    edit->setText(QStringLiteral("  关键词  "));

    const business::FilterCriteria c = dlg.criteria();
    EXPECT_EQ(c.keyword, QStringLiteral("关键词"));
}

// 取消勾选某类型后，criteria.types 不再包含该类型
TEST_F(FilterDialogTest, CriteriaTypesReflectCheckboxes)
{
    gui::FilterDialog dlg(sampleRecords());

    const auto checks = dlg.findChildren<QCheckBox*>();
    // 取消所有勾选
    for (QCheckBox* cb : checks) {
        cb->setChecked(false);
    }

    const business::FilterCriteria c = dlg.criteria();
    EXPECT_TRUE(c.types.isEmpty());
}

// 进度范围应反映 SpinBox 的值
TEST_F(FilterDialogTest, CriteriaProgressRangeReflectsSpinboxes)
{
    gui::FilterDialog dlg(sampleRecords());

    const auto spins = dlg.findChildren<QSpinBox*>();
    ASSERT_EQ(spins.size(), 2);
    int minVal = 999, maxVal = -1;
    for (QSpinBox* spin : spins) {
        const int v = spin->value();
        if (v < minVal) minVal = v;
        if (v > maxVal) maxVal = v;
    }
    // 设置为自定义范围
    for (QSpinBox* spin : spins) {
        if (spin->value() == minVal) spin->setValue(30);
        else spin->setValue(80);
    }

    const business::FilterCriteria c = dlg.criteria();
    EXPECT_EQ(c.minProgress, 30);
    EXPECT_EQ(c.maxProgress, 80);
}
