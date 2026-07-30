#include "stats_page.h"

#include "business/analytics.h"
#include "theme.h"

#include <QDateTime>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QProgressBar>
#include <QScrollArea>
#include <QVBoxLayout>

namespace bili::gui {

namespace {

QWidget* createSection(QWidget* parent, const QString& title)
{
    auto* section = new QWidget(parent);
    auto* layout = new QVBoxLayout(section);
    layout->setContentsMargins(0, 16, 0, 0);
    layout->setSpacing(12);

    auto* label = new QLabel(title, section);
    label->setStyleSheet(QStringLiteral("color: %1; font-size: 14px; font-weight: 600;").arg(theme::TEXT));
    layout->addWidget(label);

    auto* container = new QWidget(section);
    container->setStyleSheet(QStringLiteral(
        "QWidget { background-color: %1; border: 1px solid %2; border-radius: 12px; }"
    ).arg(theme::CARD, theme::BORDER));
    auto* containerLayout = new QVBoxLayout(container);
    containerLayout->setContentsMargins(16, 16, 16, 16);
    containerLayout->setSpacing(12);
    layout->addWidget(container);

    return container;
}

QLabel* createCardValue(QWidget* parent, const QString& value, const QString& label)
{
    auto* card = new QWidget(parent);
    card->setStyleSheet(QStringLiteral(
        "QWidget { background-color: %1; border: 1px solid %2; border-radius: 10px; }"
    ).arg(theme::CARD, theme::BORDER));
    auto* layout = new QVBoxLayout(card);
    layout->setContentsMargins(12, 12, 12, 12);
    layout->setSpacing(4);

    auto* valueLabel = new QLabel(value, card);
    valueLabel->setStyleSheet(QStringLiteral("color: %1; font-size: 18px; font-weight: 700;").arg(theme::PINK));
    valueLabel->setWordWrap(true);
    layout->addWidget(valueLabel);

    auto* nameLabel = new QLabel(label, card);
    nameLabel->setStyleSheet(QStringLiteral("color: %1; font-size: 12px;").arg(theme::TEXT_3));
    layout->addWidget(nameLabel);

    return valueLabel;
}

void clearLayout(QLayout* layout)
{
    if (!layout) return;
    while (QLayoutItem* item = layout->takeAt(0)) {
        if (item->widget()) {
            item->widget()->deleteLater();
        }
        delete item;
    }
}

QWidget* addBarRow(QWidget* parent,
                   QLayout* layout,
                   const QString& label,
                   int value,
                   int maxValue,
                   const QString& barColor,
                   bool showValue = false,
                   int labelWidth = 0)
{
    auto* row = new QWidget(parent);
    auto* rowLayout = new QHBoxLayout(row);
    rowLayout->setContentsMargins(0, 0, 0, 0);
    rowLayout->setSpacing(8);

    auto* labelWidget = new QLabel(label, row);
    labelWidget->setStyleSheet(QStringLiteral("color: %1; font-size: 12px; border: none; background: transparent;").arg(theme::TEXT));
    if (labelWidth > 0) {
        labelWidget->setFixedWidth(labelWidth);
    }
    labelWidget->setWordWrap(true);
    rowLayout->addWidget(labelWidget, 1);

    auto* bar = new QProgressBar(row);
    bar->setRange(0, maxValue);
    bar->setValue(value);
    if (showValue) {
        bar->setFormat(QStringLiteral("%v"));
        bar->setAlignment(Qt::AlignCenter);
        bar->setFixedHeight(14);
        bar->setStyleSheet(QStringLiteral(
            "QProgressBar { border: none; background-color: %1; border-radius: 7px; color: %2; font-size: 10px; }"
            "QProgressBar::chunk { background-color: %3; border-radius: 7px; }"
        ).arg(theme::SURFACE_HOVER, theme::TEXT, barColor));
    } else {
        bar->setTextVisible(false);
        bar->setFixedHeight(6);
        bar->setStyleSheet(QStringLiteral(
            "QProgressBar { border: none; background-color: %1; border-radius: 3px; }"
            "QProgressBar::chunk { background-color: %2; border-radius: 3px; }"
        ).arg(theme::SURFACE_HOVER, barColor));
    }
    rowLayout->addWidget(bar, showValue ? 1 : 0);

    layout->addWidget(row);
    return row;
}

void updateTrendBars(QWidget* container, const QVariantList& trend, const QString& barColor)
{
    auto* layout = container->layout();
    clearLayout(layout);

    int maxCount = 1;
    for (const auto& item : trend) {
        const int count = item.toMap().value(QStringLiteral("count")).toInt();
        if (count > maxCount) maxCount = count;
    }

    for (const auto& item : trend) {
        const QVariantMap map = item.toMap();
        const QString date = map.value(QStringLiteral("date")).toString();
        const int count = map.value(QStringLiteral("count")).toInt();
        addBarRow(container, layout, date, count, maxCount, barColor, true, 80);
    }
}

} // namespace

class StatsPage::Impl {
public:
    QWidget* cardsContainer = nullptr;
    QWidget* authorsContainer = nullptr;
    QWidget* categoriesContainer = nullptr;
    QWidget* timeOfDayContainer = nullptr;
    QWidget* dailyTrendContainer = nullptr;
    QWidget* monthlyTrendContainer = nullptr;
    QWidget* yearlyTrendContainer = nullptr;
    QLabel* emptyLabel = nullptr;
};

StatsPage::StatsPage(QWidget* parent)
    : QWidget(parent)
    , d(std::make_unique<Impl>())
{
    buildUi();
}

StatsPage::~StatsPage() = default;

void StatsPage::buildUi()
{
    auto* mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(0, 0, 0, 0);
    mainLayout->setSpacing(0);

    auto* scroll = new QScrollArea(this);
    scroll->setWidgetResizable(true);
    scroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    scroll->setStyleSheet(QStringLiteral("QScrollArea { border: none; background-color: transparent; }"));

    auto* content = new QWidget(scroll);
    auto* contentLayout = new QVBoxLayout(content);
    contentLayout->setContentsMargins(20, 20, 20, 20);
    contentLayout->setSpacing(0);

    d->emptyLabel = new QLabel(QStringLiteral("暂无数据，请先抓取历史记录。"), content);
    d->emptyLabel->setStyleSheet(QStringLiteral("color: %1; font-size: 13px; padding: 40px;").arg(theme::TEXT_3));
    d->emptyLabel->setAlignment(Qt::AlignCenter);
    contentLayout->addWidget(d->emptyLabel);

    d->cardsContainer = new QWidget(content);
    auto* cardsLayout = new QGridLayout(d->cardsContainer);
    cardsLayout->setContentsMargins(0, 0, 0, 0);
    cardsLayout->setSpacing(12);
    d->cardsContainer->hide();
    contentLayout->addWidget(d->cardsContainer);

    d->authorsContainer = createSection(content, QStringLiteral("最多观看 UP 主"));
    d->authorsContainer->hide();
    contentLayout->addWidget(d->authorsContainer->parentWidget());

    d->categoriesContainer = createSection(content, QStringLiteral("最常看分类"));
    d->categoriesContainer->hide();
    contentLayout->addWidget(d->categoriesContainer->parentWidget());

    d->timeOfDayContainer = createSection(content, QStringLiteral("观看时段分布"));
    d->timeOfDayContainer->hide();
    contentLayout->addWidget(d->timeOfDayContainer->parentWidget());

    d->dailyTrendContainer = createSection(content, QStringLiteral("近 30 天观看趋势"));
    d->dailyTrendContainer->hide();
    contentLayout->addWidget(d->dailyTrendContainer->parentWidget());

    d->monthlyTrendContainer = createSection(content, QStringLiteral("近 12 个月观看趋势"));
    d->monthlyTrendContainer->hide();
    contentLayout->addWidget(d->monthlyTrendContainer->parentWidget());

    d->yearlyTrendContainer = createSection(content, QStringLiteral("年度观看趋势"));
    d->yearlyTrendContainer->hide();
    contentLayout->addWidget(d->yearlyTrendContainer->parentWidget());

    contentLayout->addStretch();
    scroll->setWidget(content);
    mainLayout->addWidget(scroll);
}

void StatsPage::setRecords(const RecordList& records)
{
    const bool hasData = !records.empty();
    d->emptyLabel->setVisible(!hasData);
    d->cardsContainer->setVisible(hasData);
    d->authorsContainer->parentWidget()->setVisible(hasData);
    d->categoriesContainer->parentWidget()->setVisible(hasData);
    d->timeOfDayContainer->parentWidget()->setVisible(hasData);
    d->dailyTrendContainer->parentWidget()->setVisible(hasData);
    d->monthlyTrendContainer->parentWidget()->setVisible(hasData);
    d->yearlyTrendContainer->parentWidget()->setVisible(hasData);

    if (!hasData) {
        return;
    }

    updateCards(bili::business::computeBasicStats(records));
    updateList(d->authorsContainer, bili::business::topAuthors(records), QStringLiteral("author"));
    updateList(d->categoriesContainer, bili::business::topCategories(records), QStringLiteral("category"));
    updateTimeOfDay(bili::business::timeOfDayDistribution(records));
    updateDailyTrend(bili::business::dailyTrend(records));
    updateMonthlyTrend(bili::business::monthlyTrend(records));
    updateYearlyTrend(bili::business::yearlyTrend(records));
}

void StatsPage::updateCards(const QVariantMap& stats)
{
    clearLayout(d->cardsContainer->layout());
    auto* grid = qobject_cast<QGridLayout*>(d->cardsContainer->layout());

    const QVector<QPair<QString, QString>> cards = {
        { stats.value(QStringLiteral("total_records")).toString(), QStringLiteral("总记录") },
        { stats.value(QStringLiteral("total_videos")).toString(), QStringLiteral("视频") },
        { stats.value(QStringLiteral("total_lives")).toString(), QStringLiteral("直播") },
        { stats.value(QStringLiteral("total_articles")).toString(), QStringLiteral("专栏") },
        { stats.value(QStringLiteral("total_watch_time_text")).toString(), QStringLiteral("总观看时长") },
        { stats.value(QStringLiteral("unique_authors")).toString(), QStringLiteral("不同 UP 主") },
        { stats.value(QStringLiteral("avg_completion_text")).toString(), QStringLiteral("平均完成度") }
    };

    constexpr int columns = 4;
    for (int i = 0; i < cards.size(); ++i) {
        auto* valueLabel = createCardValue(d->cardsContainer, cards[i].first, cards[i].second);
        grid->addWidget(valueLabel->parentWidget(), i / columns, i % columns);
    }
}

void StatsPage::updateList(QWidget* container, const QVariantList& list, const QString& /*labelKey*/)
{
    auto* layout = container->layout();
    clearLayout(layout);

    if (list.isEmpty()) {
        auto* label = new QLabel(QStringLiteral("暂无数据"), container);
        label->setStyleSheet(QStringLiteral("color: %1; font-size: 12px;").arg(theme::TEXT_3));
        layout->addWidget(label);
        return;
    }

    int maxCount = 1;
    for (const auto& item : list) {
        const int count = item.toMap().value(QStringLiteral("count")).toInt();
        if (count > maxCount) maxCount = count;
    }

    for (const auto& item : list) {
        const QVariantMap map = item.toMap();
        const QString name = map.value(QStringLiteral("name")).toString();
        const int count = map.value(QStringLiteral("count")).toInt();
        const QString watchText = map.value(QStringLiteral("watch_time_text")).toString();
        const QString completionText = map.value(QStringLiteral("avg_completion_text")).toString();
        const QString label = QStringLiteral("%1 (%2 次, %3, 完成度 %4)")
                                  .arg(name)
                                  .arg(count)
                                  .arg(watchText)
                                  .arg(completionText);
        addBarRow(container, layout, label, count, maxCount, theme::PINK);
    }
}

void StatsPage::updateTimeOfDay(const QVariantMap& distribution)
{
    auto* layout = d->timeOfDayContainer->layout();
    clearLayout(layout);

    int total = 0;
    for (const QString& key : distribution.keys()) {
        total += distribution.value(key).toInt();
    }
    if (total == 0) total = 1;

    const QStringList order = {
        QStringLiteral("凌晨"),
        QStringLiteral("上午"),
        QStringLiteral("下午"),
        QStringLiteral("晚上")
    };

    for (const QString& key : order) {
        const int count = distribution.value(key).toInt();
        addBarRow(d->timeOfDayContainer, layout, QStringLiteral("%1 (%2)").arg(key).arg(count), count, total, theme::BLUE);
    }
}

void StatsPage::updateDailyTrend(const QVariantList& trend)
{
    updateTrendBars(d->dailyTrendContainer, trend, theme::PINK);
}

void StatsPage::updateMonthlyTrend(const QVariantList& trend)
{
    updateTrendBars(d->monthlyTrendContainer, trend, theme::BLUE);
}

void StatsPage::updateYearlyTrend(const QVariantList& trend)
{
    updateTrendBars(d->yearlyTrendContainer, trend, theme::SUCCESS);
}

} // namespace bili::gui
