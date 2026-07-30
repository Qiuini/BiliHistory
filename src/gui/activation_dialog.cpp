#include "activation_dialog.h"

#include "theme.h"
#include "core/paths.h"
#include "licensing/i_license_manager.h"
#include "licensing/machine_id.h"

#include <QDialogButtonBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QPushButton>
#include <QVBoxLayout>

namespace bili::gui {

class ActivationDialog::Impl {
public:
    ILicenseManager* licenseManager = nullptr;
    QLabel* statusLabel = nullptr;
    QLabel* machineLabel = nullptr;
    QLineEdit* codeEdit = nullptr;
};

ActivationDialog::ActivationDialog(bili::ILicenseManager& licenseManager, QWidget* parent)
    : QDialog(parent)
    , d(std::make_unique<Impl>())
{
    d->licenseManager = &licenseManager;
    setWindowTitle(QStringLiteral("会员激活"));
    setMinimumWidth(480);
    setFixedWidth(480);
    buildUi();
    updateStatus();
}

ActivationDialog::~ActivationDialog() = default;

void ActivationDialog::buildUi()
{
    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(20, 20, 20, 20);
    layout->setSpacing(12);

    auto* title = new QLabel(QStringLiteral("激活 BiliHistory 会员"), this);
    title->setStyleSheet(QStringLiteral("color: %1; font-size: 16px; font-weight: 600;").arg(theme::TEXT));
    layout->addWidget(title);

    d->statusLabel = new QLabel(this);
    d->statusLabel->setStyleSheet(QStringLiteral("color: %1; font-size: 13px;").arg(theme::TEXT_2));
    layout->addWidget(d->statusLabel);

    d->machineLabel = new QLabel(QStringLiteral("机器码: %1").arg(bili::MachineId::current()), this);
    d->machineLabel->setStyleSheet(QStringLiteral("color: %1; font-size: 12px;").arg(theme::TEXT_3));
    d->machineLabel->setWordWrap(true);
    layout->addWidget(d->machineLabel);

    auto* codeHint = new QLabel(QStringLiteral("请输入激活码："), this);
    codeHint->setStyleSheet(QStringLiteral("color: %1; font-size: 12px;").arg(theme::TEXT_3));
    layout->addWidget(codeHint);

    d->codeEdit = new QLineEdit(this);
    d->codeEdit->setPlaceholderText(QStringLiteral("在此粘贴激活码..."));
    layout->addWidget(d->codeEdit);

    auto* buttons = new QDialogButtonBox(QDialogButtonBox::Cancel, this);
    auto* activateBtn = new QPushButton(QStringLiteral("立即激活"), this);
    activateBtn->setObjectName(QStringLiteral("primaryButton"));
    connect(activateBtn, &QPushButton::clicked, this, &ActivationDialog::onActivate);
    buttons->addButton(activateBtn, QDialogButtonBox::AcceptRole);
    buttons->button(QDialogButtonBox::Cancel)->setText(QStringLiteral("稍后再说"));
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
    layout->addWidget(buttons);
}

void ActivationDialog::updateStatus()
{
    if (d->licenseManager->isLicensed(bili::Paths::licensePath())) {
        d->statusLabel->setText(QStringLiteral("当前状态: 已激活"));
    } else {
        d->statusLabel->setText(QStringLiteral("当前状态: 未激活"));
    }
}

void ActivationDialog::onActivate()
{
    const QString code = d->codeEdit->text().trimmed();
    if (code.isEmpty()) {
        QMessageBox::warning(this, QStringLiteral("激活码为空"), QStringLiteral("请输入激活码。"));
        return;
    }

    const auto result = d->licenseManager->activate(code, bili::Paths::licensePath());
    if (result) {
        QMessageBox::information(this, QStringLiteral("激活成功"),
                                 QStringLiteral("会员激活成功，感谢您的支持！"));
        accept();
    } else {
        QMessageBox::critical(this, QStringLiteral("激活失败"),
                              QStringLiteral("激活码无效或已过期，请检查输入内容。"));
    }
}

} // namespace bili::gui
