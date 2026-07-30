#pragma once

#include "business/filter.h"
#include "core/models.h"

#include <QDialog>
#include <memory>

namespace bili::gui {

// 批量筛选 / 高级导出对话框。
// 根据历史记录提取分类与 UP 主列表，支持时间、类型、分类、UP 主、
// 完成度范围和关键词筛选。
class FilterDialog : public QDialog {
    Q_OBJECT
public:
    explicit FilterDialog(const RecordList& records, QWidget* parent = nullptr);
    ~FilterDialog() override;

    bili::business::FilterCriteria criteria() const;

private slots:
    void onAccepted();
    void onReset();

private:
    class Impl;
    std::unique_ptr<Impl> d;
};

} // namespace bili::gui
