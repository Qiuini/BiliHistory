#include "activation_dialog.h"

#include "theme.h"
#include "core/paths.h"
#include "licensing/license_manager.h"
#include "licensing/machine_id.h"

#include <QDialogButtonBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QPushButton>
#include <QVBoxLayout>

namespace bili::gui {

ActivationDialog::ActivationDialog(QWidget* parent)
    : QDialog(parent)
{
    setWindowTitle(QStringLiteral("会员激活"));
    setMinimumWidth(480);
    setFixedWidth(480);
    buildUi();
    updateStatus();
}

void ActivationDialog::buildUi()
{
    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(20, 20, 20, 20);
    layout->setSpacing(12);

    auto* title = new QLabel(QStringLiteral("激活 BiliHistory 会员"), this);
    title->setStyleSheet(QStringLiteral("color: %1; font-size: 16px; font-weight: 600;").arg(theme::TEXT));
    layout->addWidget(title);

    m_statusLabel = new QLabel(this);
    m_statusLabel->setStyleSheet(QStringLiteral("color: %1; font-size: 13px;").arg(theme::TEXT_2));
    layout->addWidget(m_statusLabel);

    m_machineLabel = new QLabel(QStringLiteral("机器码: %1").arg(bili::MachineId::current()), this);
    m_machineLabel->setStyleSheet(QStringLiteral("color: %1; font-size: 12px;").arg(theme::TEXT_3));
    m_machineLabel->setWordWrap(true);
    layout->addWidget(m_machineLabel);

    auto* codeHint = new QLabel(QStringLiteral("请输入激活码："), this);
    codeHint->setStyleSheet(QStringLiteral("color: %1; font-size: 12px;").arg(theme::TEXT_3));
    layout->addWidget(codeHint);

    m_codeEdit = new QLineEdit(this);
    m_codeEdit->setPlaceholderText(QStringLiteral("在此粘贴激活码..."));
    layout->addWidget(m_codeEdit);

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
    if (bili::LicenseManager::isLicensed(bili::Paths::licensePath())) {
        m_statusLabel->setText(QStringLiteral("当前状态: 已激活"));
    } else {
        m_statusLabel->setText(QStringLiteral("当前状态: 未激活"));
    }
}

void ActivationDialog::onActivate()
{
    const QString code = m_codeEdit->text().trimmed();
    if (code.isEmpty()) {
        QMessageBox::warning(this, QStringLiteral("激活码为空"), QStringLiteral("请输入激活码。"));
        return;
    }

    const auto result = bili::LicenseManager::activate(code, bili::Paths::licensePath());
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
