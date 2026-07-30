#pragma once

#include "core/models.h"

#include <QWidget>

#include <memory>

class QLabel;
class QWidget;

namespace bili::gui {

class StatsPage : public QWidget {
    Q_OBJECT
public:
    explicit StatsPage(QWidget* parent = nullptr);
    ~StatsPage() override;

    void setRecords(const RecordList& records);

private:
    void buildUi();
    void updateCards(const QVariantMap& stats);
    void updateList(QWidget* container, const QVariantList& list, const QString& labelKey);
    void updateTimeOfDay(const QVariantMap& distribution);
    void updateDailyTrend(const QVariantList& trend);
    void updateMonthlyTrend(const QVariantList& trend);
    void updateYearlyTrend(const QVariantList& trend);

    class Impl;
    std::unique_ptr<Impl> d;
};

} // namespace bili::gui
