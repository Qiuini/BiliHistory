#pragma once

#include "models.h"

#include <QHash>
#include <QString>
#include <QStringList>

namespace bili {

struct CsvLoadResult {
    RecordList records;
    int errorCount = 0;
    int rowCount = 0;
    int schemaVersion = 1;
};

// CSV 持久化。
// 读取时按表头列名建索引（QHash<QString,int>），列顺序变更或新增列不再导致解析错乱；
// 写入时仍使用固定列顺序，保证可读性。
class CsvStorage {
public:
    static CsvLoadResult load(const QString& filePath);
    static bool save(const QString& filePath, const RecordList& records);

    // 创建带时间戳的备份
    static bool backup(const QString& sourcePath, const QString& backupDir);

    // 派生字段列名（与 BaseRecord::derivedCsvFields 顺序对齐）
    static QStringList derivedColumnNames();

private:
    static QStringList headerColumns();
    static QString recordToRow(const RecordPtr& record);
    static RecordPtr rowToRecord(const QStringList& row, const QHash<QString, int>& index);
};

} // namespace bili
