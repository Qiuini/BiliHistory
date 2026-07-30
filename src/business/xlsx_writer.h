#pragma once

#include "core/models.h"

#include <QString>
#include <QStringList>
#include <QVariant>
#include <QVariantList>

#include <vector>

namespace bili::business {

// 最小化的 .xlsx 写入器，基于 miniz 生成 ZIP 包，不依赖 QtXlsx。
class XlsxWriter {
public:
    explicit XlsxWriter(const QString& sheetName = QStringLiteral("Sheet1"));

    void setHeaders(const QStringList& headers);
    void addRow(const QVariantList& row);

    bool save(const QString& filePath) const;

private:
    QString escapeXml(const QString& text) const;
    QString cellReference(int row, int col) const;
    QByteArray buildContentTypes() const;
    QByteArray buildRels() const;
    QByteArray buildWorkbookRels() const;
    QByteArray buildWorkbook() const;
    QByteArray buildStyles() const;
    QByteArray buildWorksheet() const;

    QString m_sheetName;
    QStringList m_headers;
    std::vector<QVariantList> m_rows;
};

QString exportXlsx(const RecordList& records, const QString& filePath);

} // namespace bili::business
