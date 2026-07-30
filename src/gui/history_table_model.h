#pragma once

#include <QAbstractTableModel>
#include <QSet>

#include <memory>

#include "core/models.h"

namespace bili::gui {

class HistoryTableModel : public QAbstractTableModel {
    Q_OBJECT
public:
    enum Roles {
        TypeRole = Qt::UserRole + 1,
        ProgressRole,
        ProgressTextRole,
        LinkRole,
        NewRole,
        HoverRole
    };
    Q_ENUM(Roles)

    explicit HistoryTableModel(QObject* parent = nullptr);
    ~HistoryTableModel() override;

    int rowCount(const QModelIndex& parent = QModelIndex()) const override;
    int columnCount(const QModelIndex& parent = QModelIndex()) const override;
    QVariant data(const QModelIndex& index, int role = Qt::DisplayRole) const override;
    QVariant headerData(int section, Qt::Orientation orientation, int role = Qt::DisplayRole) const override;

    void setRows(const RecordList& records);
    void updateRows(const RecordList& records);
    void setNewIds(const QSet<QString>& ids);
    void setHoverRow(int row);

private:
    class Impl;
    std::unique_ptr<Impl> d;
};

} // namespace bili::gui
