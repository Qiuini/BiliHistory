#pragma once

#include <QDialog>
#include <memory>

namespace bili {
class ILicenseManager;
} // namespace bili

namespace bili::gui {

class TrialExpiredDialog : public QDialog {
    Q_OBJECT
public:
    explicit TrialExpiredDialog(bili::ILicenseManager& licenseManager,
                                QWidget* parent = nullptr);
    ~TrialExpiredDialog() override;

private slots:
    void onActivate();

private:
    void buildUi();

    class Impl;
    std::unique_ptr<Impl> d;
};

} // namespace bili::gui
