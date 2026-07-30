#pragma once

#include <QDialog>

#include <memory>

class QTextEdit;
class QCheckBox;
class QLabel;

namespace bili {
class IConfig;
class IFeatureAccess;
}

namespace bili::gui {

class SettingsDialog : public QDialog {
    Q_OBJECT
public:
    explicit SettingsDialog(bili::IConfig* config,
                            bili::IFeatureAccess* featureAccess,
                            QWidget* parent = nullptr);
    ~SettingsDialog() override;

private slots:
    void onSave();

private:
    void buildUi();

    class Impl;
    std::unique_ptr<Impl> d;
};

} // namespace bili::gui
