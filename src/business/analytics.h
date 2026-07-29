#pragma once

#include "core/models.h"

#include <QVariantList>
#include <QVariantMap>

namespace bili::business {

QVariantMap computeBasicStats(const RecordList& records);
QVariantList topAuthors(const RecordList& records, int topN = 10);
QVariantList topCategories(const RecordList& records, int topN = 10);
QVariantMap timeOfDayDistribution(const RecordList& records);
QVariantList topAuthorsByCompletion(const RecordList& records, int minCount = 3, int topN = 10);
QVariantList dailyTrend(const RecordList& records, int days = 30);
QString formatDuration(int totalSeconds);

} // namespace bili::business
