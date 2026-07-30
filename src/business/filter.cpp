#include "filter.h"

namespace bili::business {

bool matchesFilter(const BaseRecord* record, const FilterCriteria& criteria)
{
    if (!record) {
        return false;
    }

    if (criteria.startTime.isValid() && record->viewAt < criteria.startTime) {
        return false;
    }
    if (criteria.endTime.isValid() && record->viewAt > criteria.endTime) {
        return false;
    }

    if (!criteria.types.isEmpty() && !criteria.types.contains(record->type)) {
        return false;
    }

    if (!criteria.category.isEmpty()
        && !record->category.contains(criteria.category, Qt::CaseInsensitive)) {
        return false;
    }

    if (!criteria.author.isEmpty()
        && !record->authorName.contains(criteria.author, Qt::CaseInsensitive)) {
        return false;
    }

    const int completion = record->effectiveCompletionPercent();
    if (completion < criteria.minProgress || completion > criteria.maxProgress) {
        return false;
    }

    if (!criteria.keyword.isEmpty()) {
        const QString lower = criteria.keyword.toLower();
        if (!record->title.toLower().contains(lower)
            && !record->bvid.toLower().contains(lower)
            && !record->category.toLower().contains(lower)
            && !record->authorName.toLower().contains(lower)) {
            return false;
        }
    }

    return true;
}

RecordList filterRecords(const RecordList& records, const FilterCriteria& criteria)
{
    RecordList result;
    result.reserve(records.size());
    for (const auto& record : records) {
        if (matchesFilter(record.get(), criteria)) {
            result.push_back(record);
        }
    }
    return result;
}

} // namespace bili::business
