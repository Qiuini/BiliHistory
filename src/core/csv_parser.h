#pragma once

#include <QString>
#include <QStringList>
#include <QVector>

namespace bili {

// 轻量级 RFC 4180 风格 CSV 解析器。
// 将手写字符状态机封装为可独立测试的工具类。
class CsvParser {
public:
    explicit CsvParser(QChar delimiter = ',');

    // 解析单行 CSV，正确处理引号与转义
    QStringList parseLine(const QString& line) const;

    // 转义单个字段
    static QString escapeField(const QString& field, QChar delimiter = ',');

    // 组合多个字段为一行 CSV
    static QString joinFields(const QStringList& fields, QChar delimiter = ',');

    // 判断某行是否为表头：行中至少 minMatchCount 个字段出现在期望表头集合中即认定为表头。
    // 对列顺序变化、新增列、缺失列均具鲁棒性；数据值（如 "video"/"动画"）不会与
    // 表头名（如 "type"/"category"）冲突，因此不会误判普通数据行。
    // minMatchCount 会被夹到 [1, expectedHeaders.size()] 区间，保证单列表头也能识别。
    bool looksLikeHeader(const QStringList& fields,
                         const QStringList& expectedHeaders,
                         int minMatchCount = 2) const;

private:
    QChar m_delimiter;
};

} // namespace bili
