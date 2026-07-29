#pragma once

#include <QDialog>

class QLabel;
class QLineEdit;

namespace bili::gui {

class ActivationDialog : public QDialog {
    Q_OBJECT
public:
    explicit ActivationDialog(QWidget* parent = nullptr);

private slots:
    void onActivate();

private:
    void buildUi();
    void updateStatus();

    QLabel* m_statusLabel = nullptr;
    QLabel* m_machineLabel = nullptr;
    QLineEdit* m_codeEdit = nullptr;
};

} // namespace bili::gui
