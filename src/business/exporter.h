#pragma once

#include "core/models.h"

#include <QFileInfo>
#include <QString>
#include <stdexcept>

namespace bili::business {

class ExportException : public std::runtime_error {
public:
    explicit ExportException(const QString& message);
    explicit ExportException(const std::string& message);
    QString message() const;

private:
    QString m_message;
};

QString exportCsv(const RecordList& records, const QString& filePath);
QString exportJson(const RecordList& records, const QString& filePath);
QString exportHtml(const RecordList& records, const QString& filePath);
QString exportMarkdown(const RecordList& records, const QString& filePath);
QString exportRecords(const RecordList& records, const QString& filePath);
QString supportedFilters();

} // namespace bili::business
