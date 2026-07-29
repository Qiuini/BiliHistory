#pragma once

#include <QDialog>

namespace bili::gui {

class TrialExpiredDialog : public QDialog {
    Q_OBJECT
public:
    explicit TrialExpiredDialog(QWidget* parent = nullptr);

private slots:
    void onActivate();

private:
    void buildUi();
};

} // namespace bili::gui
