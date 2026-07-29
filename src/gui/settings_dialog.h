#pragma once

#include <QDialog>

class QTextEdit;

namespace bili::gui {

class SettingsDialog : public QDialog {
    Q_OBJECT
public:
    explicit SettingsDialog(QWidget* parent = nullptr);

private slots:
    void onSave();

private:
    void buildUi();

    QTextEdit* m_cookieEdit = nullptr;
};

} // namespace bili::gui
