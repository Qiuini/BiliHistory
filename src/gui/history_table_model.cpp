#include "history_table_model.h"

namespace bili::gui {

namespace {

QString typeDisplayName(RecordType type)
{
    switch (type) {
    case RecordType::Video:   return QStringLiteral("视频");
    case RecordType::Live:    return QStringLiteral("直播");
    case RecordType::Article: return QStringLiteral("专栏");
    default:                  return QStringLiteral("未知");
    }
}

bool recordDataChanged(const RecordPtr& oldRec, const RecordPtr& newRec)
{
    if (!oldRec || !newRec) return oldRec != newRec;
    return oldRec->type != newRec->type
        || oldRec->category != newRec->category
        || oldRec->title != newRec->title
        || oldRec->authorName != newRec->authorName
        || oldRec->authorId != newRec->authorId
        || oldRec->viewAt != newRec->viewAt
        || oldRec->progress != newRec->progress
        || oldRec->progressPercent != newRec->progressPercent
        || oldRec->bvid != newRec->bvid
        || oldRec->coverUrl != newRec->coverUrl;
}

} // namespace

HistoryTableModel::HistoryTableModel(QObject* parent)
    : QAbstractTableModel(parent)
{
}

int HistoryTableModel::rowCount(const QModelIndex& /*parent*/) const
{
    return static_cast<int>(m_rows.size());
}

int HistoryTableModel::columnCount(const QModelIndex& /*parent*/) const
{
    return 8;
}

QVariant HistoryTableModel::data(const QModelIndex& index, int role) const
{
    if (!index.isValid()) return {};
    const int row = index.row();
    if (row < 0 || row >= rowCount()) return {};
    const auto& rec = m_rows[row];

    if (role == Qt::DisplayRole) {
        switch (index.column()) {
        case 0: return typeDisplayName(rec->type);
        case 1: return rec->title;
        case 2: return rec->authorName;
        case 3:
            if (!rec->progress.isEmpty()) {
                return QStringLiteral("%1 (%2%)").arg(rec->progress).arg(rec->progressPercent);
            }
            return QStringLiteral("%1%").arg(rec->progressPercent);
        case 4: return rec->category;
        case 5: return rec->viewAt.toString(QStringLiteral("yyyy-MM-dd hh:mm"));
        case 6: return rec->bvid;
        case 7: return QStringLiteral("查看");
        default: return {};
        }
    }

    switch (role) {
    case TypeRole:        return typeDisplayName(rec->type);
    case ProgressRole:    return rec->progressPercent;
    case ProgressTextRole:return rec->progress;
    case LinkRole:        return rec->uniqueKey();
    case NewRole:         return m_newIds.contains(rec->uniqueKey());
    case HoverRole:       return row == m_hoverRow;
    default:              return {};
    }
}

QVariant HistoryTableModel::headerData(int section, Qt::Orientation orientation, int role) const
{
    if (orientation == Qt::Horizontal && role == Qt::DisplayRole) {
        static const QStringList headers = {
            QStringLiteral("类型"),
            QStringLiteral("视频标题"),
            QStringLiteral("UP主"),
            QStringLiteral("观看进度"),
            QStringLiteral("分类"),
            QStringLiteral("观看时间"),
            QStringLiteral("BV号"),
            QStringLiteral("操作")
        };
        if (section >= 0 && section < headers.size()) {
            return headers.at(section);
        }
    }
    return QAbstractTableModel::headerData(section, orientation, role);
}

void HistoryTableModel::setRows(const RecordList& records)
{
    beginResetModel();
    m_rows = records;
    endResetModel();
}

void HistoryTableModel::updateRows(const RecordList& records)
{
    QSet<QString> newKeys;
    newKeys.reserve(static_cast<int>(records.size()));
    for (const auto& rec : records) {
        newKeys.insert(rec->uniqueKey());
    }

    // Remove rows that no longer exist.
    for (int i = rowCount() - 1; i >= 0; --i) {
        if (!newKeys.contains(m_rows[i]->uniqueKey())) {
            beginRemoveRows(QModelIndex(), i, i);
            m_rows.erase(m_rows.begin() + i);
            endRemoveRows();
        }
    }

    // Map each remaining key to its current row index.
    QHash<QString, int> indexMap;
    indexMap.reserve(rowCount());
    for (int i = 0; i < rowCount(); ++i) {
        indexMap[m_rows[i]->uniqueKey()] = i;
    }

    // Reorder / insert to match the new record list.
    for (int i = 0; i < static_cast<int>(records.size()); ++i) {
        const QString& key = records[i]->uniqueKey();
        auto it = indexMap.find(key);
        if (it == indexMap.end()) {
            beginInsertRows(QModelIndex(), i, i);
            m_rows.insert(m_rows.begin() + i, records[i]);
            endInsertRows();
            for (auto& idx : indexMap) {
                if (idx >= i) ++idx;
            }
            indexMap[key] = i;
        } else {
            const int oldIdx = it.value();
            if (oldIdx != i) {
                beginRemoveRows(QModelIndex(), oldIdx, oldIdx);
                RecordPtr moved = m_rows[oldIdx];
                m_rows.erase(m_rows.begin() + oldIdx);
                endRemoveRows();
                for (auto& idx : indexMap) {
                    if (idx > oldIdx) --idx;
                }
                beginInsertRows(QModelIndex(), i, i);
                m_rows.insert(m_rows.begin() + i, moved);
                endInsertRows();
                for (auto& idx : indexMap) {
                    if (idx >= i) ++idx;
                }
                indexMap[key] = i;
            }
        }
    }

    // Emit dataChanged for rows whose content actually changed.
    int firstChanged = -1;
    int lastChanged = -1;
    for (int i = 0; i < static_cast<int>(records.size()); ++i) {
        if (recordDataChanged(m_rows[i], records[i])) {
            if (firstChanged == -1) firstChanged = i;
            lastChanged = i;
            m_rows[i] = records[i];
        }
    }
    if (firstChanged != -1) {
        emit dataChanged(index(firstChanged, 0),
                         index(lastChanged, columnCount() - 1));
    }
}

void HistoryTableModel::setNewIds(const QSet<QString>& ids)
{
    m_newIds = ids;
    if (rowCount() > 0) {
        emit dataChanged(index(0, 0),
                         index(rowCount() - 1, columnCount() - 1),
                         { NewRole });
    }
}

void HistoryTableModel::setHoverRow(int row)
{
    if (m_hoverRow == row) return;
    const int oldRow = m_hoverRow;
    m_hoverRow = row;
    if (oldRow >= 0 && oldRow < rowCount()) {
        emit dataChanged(index(oldRow, 0),
                         index(oldRow, columnCount() - 1),
                         { HoverRole });
    }
    if (m_hoverRow >= 0 && m_hoverRow < rowCount()) {
        emit dataChanged(index(m_hoverRow, 0),
                         index(m_hoverRow, columnCount() - 1),
                         { HoverRole });
    }
}

} // namespace bili::gui
