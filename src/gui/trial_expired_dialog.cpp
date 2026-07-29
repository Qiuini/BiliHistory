#include "trial_expired_dialog.h"

#include "activation_dialog.h"
#include "theme.h"

#include <QDialogButtonBox>
#include <QLabel>
#include <QPushButton>
#include <QVBoxLayout>

namespace bili::gui {

TrialExpiredDialog::TrialExpiredDialog(QWidget* parent)
    : QDialog(parent)
{
    setWindowTitle(QStringLiteral("试用已到期"));
    setMinimumWidth(420);
    setFixedWidth(420);
    buildUi();
}

void TrialExpiredDialog::buildUi()
{
    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(24, 24, 24, 24);
    layout->setSpacing(12);

    auto* iconLabel = new QLabel(QStringLiteral("⏰"), this);
    iconLabel->setStyleSheet(QStringLiteral("font-size: 32px;"));
    iconLabel->setAlignment(Qt::AlignCenter);
    layout->addWidget(iconLabel);

    auto* title = new QLabel(QStringLiteral("免费试用已到期"), this);
    title->setStyleSheet(QStringLiteral("color: %1; font-size: 16px; font-weight: 600;").arg(theme::TEXT));
    title->setAlignment(Qt::AlignCenter);
    layout->addWidget(title);

    auto* message = new QLabel(
        QStringLiteral("感谢体验 BiliHistory！\n试用已结束，激活会员后可继续使用全部功能。"),
        this);
    message->setStyleSheet(QStringLiteral("color: %1; font-size: 13px;").arg(theme::TEXT_2));
    message->setAlignment(Qt::AlignCenter);
    message->setWordWrap(true);
    layout->addWidget(message);

    auto* guide = new QLabel(
        QStringLiteral("购买方式：访问项目 Release 页面获取激活码，或使用工具自助签发。"),
        this);
    guide->setStyleSheet(QStringLiteral("color: %1; font-size: 12px;").arg(theme::TEXT_3));
    guide->setAlignment(Qt::AlignCenter);
    guide->setWordWrap(true);
    layout->addWidget(guide);

    auto* buttons = new QDialogButtonBox(this);
    auto* quitBtn = new QPushButton(QStringLiteral("退出"), this);
    auto* activateBtn = new QPushButton(QStringLiteral("激活会员"), this);
    activateBtn->setObjectName(QStringLiteral("primaryButton"));
    connect(quitBtn, &QPushButton::clicked, this, &QDialog::reject);
    connect(activateBtn, &QPushButton::clicked, this, &TrialExpiredDialog::onActivate);
    buttons->addButton(quitBtn, QDialogButtonBox::RejectRole);
    buttons->addButton(activateBtn, QDialogButtonBox::AcceptRole);
    layout->addWidget(buttons);
}

void TrialExpiredDialog::onActivate()
{
    ActivationDialog dialog(this);
    if (dialog.exec() == QDialog::Accepted) {
        accept();
    }
}

} // namespace bili::gui
