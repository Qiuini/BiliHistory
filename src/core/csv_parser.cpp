#include "csv_parser.h"

#include <QSet>

#include <algorithm>

namespace bili {

CsvParser::CsvParser(QChar delimiter)
    : m_delimiter(delimiter)
{
}

QStringList CsvParser::parseLine(const QString& line) const
{
    QStringList result;
    QString current;
    current.reserve(64);

    bool inQuotes = false;
    const int len = line.size();

    for (int i = 0; i < len; ++i) {
        const QChar c = line[i];

        if (c == '"') {
            if (inQuotes && i + 1 < len && line[i + 1] == '"') {
                current.append('"');
                ++i; // 跳过成对引号的第二个
            } else {
                inQuotes = !inQuotes;
            }
        } else if (c == m_delimiter && !inQuotes) {
            result.append(current);
            current.clear();
        } else {
            current.append(c);
        }
    }

    result.append(current);
    return result;
}

QString CsvParser::escapeField(const QString& field, QChar delimiter)
{
    const bool needsEscape = field.contains(delimiter)
                             || field.contains('"')
                             || field.contains('\n')
                             || field.contains('\r');
    if (!needsEscape) {
        return field;
    }

    QString escaped;
    escaped.reserve(field.size() + 2);
    escaped.append('"');
    for (const QChar c : field) {
        if (c == '"') {
            escaped.append("\"\"");
        } else {
            escaped.append(c);
        }
    }
    escaped.append('"');
    return escaped;
}

QString CsvParser::joinFields(const QStringList& fields, QChar delimiter)
{
    if (fields.isEmpty()) {
        return QString();
    }

    QStringList escaped;
    escaped.reserve(fields.size());
    for (const QString& field : fields) {
        escaped.append(escapeField(field, delimiter));
    }
    return escaped.join(delimiter);
}

bool CsvParser::looksLikeHeader(const QStringList& fields,
                                const QStringList& expectedHeaders,
                                int minMatchCount) const
{
    if (fields.isEmpty() || expectedHeaders.isEmpty()) return false;

    // 阈值夹到 [1, expectedHeaders.size()]：单列表头也能识别，避免 minMatchCount 越界。
    const int headerCount = static_cast<int>(expectedHeaders.size());
    const int threshold = std::max(1, std::min(minMatchCount, headerCount));
    const QSet<QString> expected(expectedHeaders.begin(), expectedHeaders.end());

    int matches = 0;
    for (const QString& f : fields) {
        if (expected.contains(f) && ++matches >= threshold) {
            return true;
        }
    }
    return false;
}

} // namespace bili
