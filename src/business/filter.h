#pragma once

#include "core/models.h"

#include <QDateTime>
#include <QSet>

namespace bili::business {

// 批量筛选条件
struct FilterCriteria {
    QDateTime startTime;                 // 包含该时间之后
    QDateTime endTime;                   // 包含该时间之前
    QSet<RecordType> types;              // 为空表示不限制类型
    QString category;                    // 分类关键字（模糊匹配）
    QString author;                      // UP 主关键字（模糊匹配）
    int minProgress = 0;                 // 最小完成度 0-100
    int maxProgress = 100;               // 最大完成度 0-100
    QString keyword;                     // 标题 / BV 号关键字
};

bool matchesFilter(const BaseRecord* record, const FilterCriteria& criteria);
RecordList filterRecords(const RecordList& records, const FilterCriteria& criteria);

} // namespace bili::business
