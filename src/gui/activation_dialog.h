#pragma once

#include <QDialog>
#include <memory>

namespace bili {
class ILicenseManager;
} // namespace bili

namespace bili::gui {

class ActivationDialog : public QDialog {
    Q_OBJECT
public:
    explicit ActivationDialog(bili::ILicenseManager& licenseManager,
                              QWidget* parent = nullptr);
    ~ActivationDialog() override;

private slots:
    void onActivate();

private:
    void buildUi();
    void updateStatus();

    class Impl;
    std::unique_ptr<Impl> d;
};

} // namespace bili::gui
