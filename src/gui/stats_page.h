#pragma once

#include "core/models.h"

#include <QWidget>

class QLabel;
class QProgressBar;

namespace bili::gui {

class StatsPage : public QWidget {
    Q_OBJECT
public:
    explicit StatsPage(QWidget* parent = nullptr);

    void setRecords(const RecordList& records);

private:
    void buildUi();
    void updateCards(const QVariantMap& stats);
    void updateList(QWidget* container, const QVariantList& list, const QString& labelKey);
    void updateTimeOfDay(const QVariantMap& distribution);
    void updateDailyTrend(const QVariantList& trend);

    QWidget* m_cardsContainer = nullptr;
    QWidget* m_authorsContainer = nullptr;
    QWidget* m_categoriesContainer = nullptr;
    QWidget* m_timeOfDayContainer = nullptr;
    QWidget* m_trendContainer = nullptr;
    QLabel* m_emptyLabel = nullptr;
};

} // namespace bili::gui
