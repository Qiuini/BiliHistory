#include "filter_dialog.h"

#include "theme.h"

#include <QCheckBox>
#include <QComboBox>
#include <QDateTimeEdit>
#include <QDialogButtonBox>
#include <QGridLayout>
#include <QGroupBox>
#include <QHash>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QSet>
#include <QSpinBox>
#include <QTimer>
#include <QVBoxLayout>

namespace bili::gui {

namespace {

QStringList collectCategories(const RecordList& records)
{
    QSet<QString> set;
    for (const auto& record : records) {
        if (record && !record->category.isEmpty()) {
            set.insert(record->category);
        }
    }
    QStringList list = set.values();
    std::sort(list.begin(), list.end());
    return list;
}

QStringList collectAuthors(const RecordList& records)
{
    QSet<QString> set;
    for (const auto& record : records) {
        if (record && !record->authorName.isEmpty()) {
            set.insert(record->authorName);
        }
    }
    QStringList list = set.values();
    std::sort(list.begin(), list.end());
    return list;
}

QLabel* makeHint(const QString& text, QWidget* parent)
{
    auto* label = new QLabel(text, parent);
    label->setStyleSheet(QStringLiteral("color: %1; font-size: 11px;").arg(theme::TEXT_3));
    label->setWordWrap(true);
    return label;
}

} // namespace

class FilterDialog::Impl {
public:
    Impl(FilterDialog* q_, const RecordList& records_)
        : q(q_)
        , records(records_)
    {
        buildUi();
    }

    void buildUi()
    {
        q->setWindowTitle(QStringLiteral("批量筛选 / 高级导出"));
        q->setMinimumWidth(520);
        q->setFixedWidth(520);

        auto* layout = new QVBoxLayout(q);
        layout->setContentsMargins(20, 20, 20, 20);
        layout->setSpacing(16);

        auto* title = new QLabel(QStringLiteral("筛选条件"), q);
        title->setStyleSheet(QStringLiteral("color: %1; font-size: 16px; font-weight: 600;").arg(theme::TEXT));
        layout->addWidget(title);

        layout->addWidget(buildTimeGroup());
        layout->addWidget(buildTypeGroup());
        layout->addWidget(buildMetaGroup());
        layout->addWidget(buildCompletionGroup());
        layout->addWidget(buildKeywordGroup());

        resultCountLabel = new QLabel(q);
        resultCountLabel->setStyleSheet(QStringLiteral("color: %1; font-size: 12px; font-weight: 600;").arg(theme::TEXT));
        resultCountLabel->setAlignment(Qt::AlignCenter);
        layout->addWidget(resultCountLabel);

        auto* hint = makeHint(QStringLiteral("留空表示不限制该条件。筛选结果可在下一步选择 CSV / Excel / PDF / Markdown / JSON / HTML 等格式导出。"), q);
        layout->addWidget(hint);

        layout->addWidget(buildButtons());
        connectSignals();
        // 注意：不能在此处调用 updateResultCount()，因为它通过 q->criteria()
        // 访问 d-> 成员，而 FilterDialog 构造函数的 d 尚未完成赋值（仍在
        // d(std::make_unique<Impl>(...)) 初始化列表中）。改由 FilterDialog
        // 构造函数体在 d 赋值完毕后调用。
    }

    void connectSignals()
    {
        countDebounce = new QTimer(q);
        countDebounce->setSingleShot(true);
        countDebounce->setInterval(150);
        q->connect(countDebounce, &QTimer::timeout, q, [this]() { updateResultCount(); });

        const auto scheduleUpdate = [this]() { countDebounce->start(); };

        q->connect(startTimeCheck, &QCheckBox::toggled, q, scheduleUpdate);
        q->connect(startTimeEdit, &QDateTimeEdit::dateTimeChanged, q, scheduleUpdate);
        q->connect(endTimeCheck, &QCheckBox::toggled, q, scheduleUpdate);
        q->connect(endTimeEdit, &QDateTimeEdit::dateTimeChanged, q, scheduleUpdate);
        q->connect(videoCheck, &QCheckBox::toggled, q, scheduleUpdate);
        q->connect(liveCheck, &QCheckBox::toggled, q, scheduleUpdate);
        q->connect(articleCheck, &QCheckBox::toggled, q, scheduleUpdate);
        q->connect(categoryCombo, &QComboBox::currentTextChanged, q, scheduleUpdate);
        q->connect(authorCombo, &QComboBox::currentTextChanged, q, scheduleUpdate);
        q->connect(minSpin, QOverload<int>::of(&QSpinBox::valueChanged), q, scheduleUpdate);
        q->connect(maxSpin, QOverload<int>::of(&QSpinBox::valueChanged), q, scheduleUpdate);
        q->connect(keywordEdit, &QLineEdit::textChanged, q, scheduleUpdate);
    }

    void updateResultCount()
    {
        const auto matched = bili::business::filterRecords(records, q->criteria());
        const QString& color = matched.empty() ? theme::DANGER : theme::SUCCESS;
        resultCountLabel->setStyleSheet(QStringLiteral("color: %1; font-size: 12px; font-weight: 600;").arg(color));
        resultCountLabel->setText(QStringLiteral("符合当前条件的记录：%1 / %2").arg(matched.size()).arg(records.size()));
    }

    QDialogButtonBox* buildButtons()
    {
        auto* buttons = new QDialogButtonBox(QDialogButtonBox::Cancel | QDialogButtonBox::Ok, q);
        buttons->button(QDialogButtonBox::Cancel)->setText(QStringLiteral("取消"));
        buttons->button(QDialogButtonBox::Ok)->setText(QStringLiteral("确认筛选"));

        auto* resetBtn = new QPushButton(QStringLiteral("重置"), q);
        buttons->addButton(resetBtn, QDialogButtonBox::ResetRole);

        q->connect(buttons, &QDialogButtonBox::accepted, q, &FilterDialog::onAccepted);
        q->connect(buttons, &QDialogButtonBox::rejected, q, &QDialog::reject);
        q->connect(resetBtn, &QPushButton::clicked, q, &FilterDialog::onReset);
        return buttons;
    }

    QGroupBox* buildTimeGroup()
    {
        auto* group = new QGroupBox(QStringLiteral("时间范围"), q);
        auto* grid = new QGridLayout(group);
        grid->setContentsMargins(12, 16, 12, 12);
        grid->setSpacing(10);

        startTimeCheck = new QCheckBox(QStringLiteral("启用"), q);
        startTimeEdit = new QDateTimeEdit(q);
        startTimeEdit->setCalendarPopup(true);
        startTimeEdit->setDisplayFormat(QStringLiteral("yyyy-MM-dd hh:mm"));
        startTimeEdit->setDateTime(QDateTime::currentDateTime().addYears(-1));
        startTimeEdit->setEnabled(false);
        q->connect(startTimeCheck, &QCheckBox::toggled, startTimeEdit, &QDateTimeEdit::setEnabled);

        endTimeCheck = new QCheckBox(QStringLiteral("启用"), q);
        endTimeEdit = new QDateTimeEdit(q);
        endTimeEdit->setCalendarPopup(true);
        endTimeEdit->setDisplayFormat(QStringLiteral("yyyy-MM-dd hh:mm"));
        endTimeEdit->setDateTime(QDateTime::currentDateTime());
        endTimeEdit->setEnabled(false);
        q->connect(endTimeCheck, &QCheckBox::toggled, endTimeEdit, &QDateTimeEdit::setEnabled);

        grid->addWidget(new QLabel(QStringLiteral("开始时间:"), q), 0, 0);
        grid->addWidget(startTimeEdit, 0, 1);
        grid->addWidget(startTimeCheck, 0, 2);
        grid->addWidget(new QLabel(QStringLiteral("结束时间:"), q), 1, 0);
        grid->addWidget(endTimeEdit, 1, 1);
        grid->addWidget(endTimeCheck, 1, 2);

        return group;
    }

    QGroupBox* buildTypeGroup()
    {
        auto* group = new QGroupBox(QStringLiteral("记录类型"), q);
        auto* layout = new QHBoxLayout(group);
        layout->setSpacing(16);
        layout->setContentsMargins(12, 16, 12, 12);

        videoCheck = new QCheckBox(QStringLiteral("视频"), q);
        liveCheck = new QCheckBox(QStringLiteral("直播"), q);
        articleCheck = new QCheckBox(QStringLiteral("专栏"), q);
        videoCheck->setChecked(true);
        liveCheck->setChecked(true);
        articleCheck->setChecked(true);

        layout->addWidget(videoCheck);
        layout->addWidget(liveCheck);
        layout->addWidget(articleCheck);
        layout->addStretch();

        return group;
    }

    QGroupBox* buildMetaGroup()
    {
        auto* group = new QGroupBox(QStringLiteral("分类与 UP 主"), q);
        auto* grid = new QGridLayout(group);
        grid->setContentsMargins(12, 16, 12, 12);
        grid->setSpacing(10);

        categoryCombo = new QComboBox(q);
        categoryCombo->setEditable(true);
        categoryCombo->addItem(QStringLiteral("全部"));
        categoryCombo->addItems(collectCategories(records));
        categoryCombo->setCurrentText(QStringLiteral("全部"));

        authorCombo = new QComboBox(q);
        authorCombo->setEditable(true);
        authorCombo->addItem(QStringLiteral("全部"));
        authorCombo->addItems(collectAuthors(records));
        authorCombo->setCurrentText(QStringLiteral("全部"));

        grid->addWidget(new QLabel(QStringLiteral("分区/分类:"), q), 0, 0);
        grid->addWidget(categoryCombo, 0, 1);
        grid->addWidget(new QLabel(QStringLiteral("UP 主:"), q), 1, 0);
        grid->addWidget(authorCombo, 1, 1);

        return group;
    }

    QGroupBox* buildCompletionGroup()
    {
        auto* group = new QGroupBox(QStringLiteral("完成度范围 (%)"), q);
        auto* layout = new QHBoxLayout(group);
        layout->setSpacing(12);
        layout->setContentsMargins(12, 16, 12, 12);

        minSpin = new QSpinBox(q);
        minSpin->setRange(0, 100);
        minSpin->setSuffix(QStringLiteral("%"));
        minSpin->setValue(0);

        maxSpin = new QSpinBox(q);
        maxSpin->setRange(0, 100);
        maxSpin->setSuffix(QStringLiteral("%"));
        maxSpin->setValue(100);

        layout->addWidget(new QLabel(QStringLiteral("最小:"), q));
        layout->addWidget(minSpin);
        layout->addWidget(new QLabel(QStringLiteral("最大:"), q));
        layout->addWidget(maxSpin);
        layout->addStretch();

        return group;
    }

    QGroupBox* buildKeywordGroup()
    {
        auto* group = new QGroupBox(QStringLiteral("关键词"), q);
        auto* layout = new QHBoxLayout(group);
        layout->setSpacing(10);
        layout->setContentsMargins(12, 16, 12, 12);

        keywordEdit = new QLineEdit(q);
        keywordEdit->setPlaceholderText(QStringLiteral("标题 / BV 号 / 分类 / UP 主"));

        layout->addWidget(keywordEdit);

        return group;
    }

    void reset()
    {
        startTimeCheck->setChecked(false);
        endTimeCheck->setChecked(false);
        videoCheck->setChecked(true);
        liveCheck->setChecked(true);
        articleCheck->setChecked(true);
        categoryCombo->setCurrentText(QStringLiteral("全部"));
        authorCombo->setCurrentText(QStringLiteral("全部"));
        minSpin->setValue(0);
        maxSpin->setValue(100);
        keywordEdit->clear();
    }

    FilterDialog* q = nullptr;
    RecordList records;

    QCheckBox* startTimeCheck = nullptr;
    QDateTimeEdit* startTimeEdit = nullptr;
    QCheckBox* endTimeCheck = nullptr;
    QDateTimeEdit* endTimeEdit = nullptr;
    QCheckBox* videoCheck = nullptr;
    QCheckBox* liveCheck = nullptr;
    QCheckBox* articleCheck = nullptr;
    QComboBox* categoryCombo = nullptr;
    QComboBox* authorCombo = nullptr;
    QSpinBox* minSpin = nullptr;
    QSpinBox* maxSpin = nullptr;
    QLineEdit* keywordEdit = nullptr;
    QLabel* resultCountLabel = nullptr;
    QTimer* countDebounce = nullptr;
};

FilterDialog::FilterDialog(const RecordList& records, QWidget* parent)
    : QDialog(parent)
    , d(std::make_unique<Impl>(this, records))
{
    d->updateResultCount();
}

FilterDialog::~FilterDialog() = default;

bili::business::FilterCriteria FilterDialog::criteria() const
{
    bili::business::FilterCriteria criteria;

    if (d->startTimeCheck->isChecked()) {
        criteria.startTime = d->startTimeEdit->dateTime();
    }
    if (d->endTimeCheck->isChecked()) {
        criteria.endTime = d->endTimeEdit->dateTime();
    }

    if (d->videoCheck->isChecked()) {
        criteria.types.insert(bili::RecordType::Video);
    }
    if (d->liveCheck->isChecked()) {
        criteria.types.insert(bili::RecordType::Live);
    }
    if (d->articleCheck->isChecked()) {
        criteria.types.insert(bili::RecordType::Article);
    }

    const QString category = d->categoryCombo->currentText().trimmed();
    if (category != QStringLiteral("全部") && !category.isEmpty()) {
        criteria.category = category;
    }

    const QString author = d->authorCombo->currentText().trimmed();
    if (author != QStringLiteral("全部") && !author.isEmpty()) {
        criteria.author = author;
    }

    criteria.minProgress = d->minSpin->value();
    criteria.maxProgress = d->maxSpin->value();

    criteria.keyword = d->keywordEdit->text().trimmed();

    return criteria;
}

void FilterDialog::onAccepted()
{
    if (criteria().minProgress > criteria().maxProgress) {
        d->minSpin->setValue(criteria().maxProgress);
    }
    accept();
}

void FilterDialog::onReset()
{
    d->reset();
}

} // namespace bili::gui
