#pragma once

#include "models.h"

#include <QString>
#include <QStringList>

namespace bili {

struct CsvLoadResult {
    RecordList records;
    int errorCount = 0;
    int rowCount = 0;
    int schemaVersion = 1;
};

class CsvStorage {
public:
    static CsvLoadResult load(const QString& filePath);
    static bool save(const QString& filePath, const RecordList& records);

    // 创建带时间戳的备份
    static bool backup(const QString& sourcePath, const QString& backupDir);

private:
    static QStringList headerColumns();
    static QString recordToRow(const RecordPtr& record);
    static RecordPtr rowToRecord(const QStringList& row);
    static QString escapeCsvField(const QString& field);
    static QStringList splitCsvLine(const QString& line);
};

} // namespace bili
