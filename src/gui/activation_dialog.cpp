#include "activation_dialog.h"

#include "theme.h"
#include "core/paths.h"
#include "licensing/i_license_manager.h"
#include "licensing/machine_id.h"

#include <QApplication>
#include <QClipboard>
#include <QDialogButtonBox>
#include <QFrame>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QPushButton>
#include <QTimer>
#include <QVBoxLayout>

namespace bili::gui {

class ActivationDialog::Impl {
public:
    ILicenseManager* licenseManager = nullptr;
    QLabel* statusLabel = nullptr;
    QLabel* machineLabel = nullptr;
    QPushButton* copyMachineBtn = nullptr;
    QLineEdit* codeEdit = nullptr;
};

ActivationDialog::ActivationDialog(bili::ILicenseManager& licenseManager, QWidget* parent)
    : QDialog(parent)
    , d(std::make_unique<Impl>())
{
    d->licenseManager = &licenseManager;
    setWindowTitle(QStringLiteral("会员中心"));
    setMinimumWidth(580);
    setFixedWidth(580);
    buildUi();
    updateStatus();
}

ActivationDialog::~ActivationDialog() = default;

void ActivationDialog::buildUi()
{
    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(20, 20, 20, 20);
    layout->setSpacing(14);

    // 标题
    auto* title = new QLabel(QStringLiteral("BiliHistory 会员"), this);
    title->setStyleSheet(QStringLiteral("color: %1; font-size: 18px; font-weight: 700;").arg(theme::TEXT));
    layout->addWidget(title);

    // 状态
    d->statusLabel = new QLabel(this);
    d->statusLabel->setStyleSheet(QStringLiteral("color: %1; font-size: 13px;").arg(theme::TEXT_2));
    layout->addWidget(d->statusLabel);

    // 机器码 + 复制按钮
    auto* machineFrame = new QFrame(this);
    machineFrame->setStyleSheet(QStringLiteral(
        "QFrame { background-color: %1; border: 1px dashed %2; border-radius: 6px; }"
    ).arg(theme::BG, theme::BORDER));
    auto* machineLayout = new QHBoxLayout(machineFrame);
    machineLayout->setContentsMargins(12, 8, 12, 8);
    auto* machineIcon = new QLabel(QStringLiteral("\xF0\x9F\x92\xBB"), machineFrame);
    machineIcon->setStyleSheet(QStringLiteral("font-size: 14px;"));
    auto* machineTitle = new QLabel(QStringLiteral("机器码"), machineFrame);
    machineTitle->setStyleSheet(QStringLiteral("color: %1; font-size: 12px; font-weight: 600;").arg(theme::TEXT_2));
    d->machineLabel = new QLabel(bili::MachineId::current(), machineFrame);
    d->machineLabel->setStyleSheet(QStringLiteral("color: %1; font-size: 12px; font-family: monospace;").arg(theme::TEXT_3));
    d->machineLabel->setWordWrap(true);
    d->machineLabel->setMinimumWidth(200);
    d->copyMachineBtn = new QPushButton(QStringLiteral("复制"), machineFrame);
    d->copyMachineBtn->setStyleSheet(QStringLiteral(
        "QPushButton { background-color: %1; color: white; border: none; border-radius: 4px; padding: 4px 12px; font-size: 11px; }"
        "QPushButton:hover { background-color: %2; }"
    ).arg(theme::PINK, theme::PINK_HOVER));
    machineLayout->addWidget(machineIcon);
    machineLayout->addWidget(machineTitle);
    machineLayout->addWidget(d->machineLabel, 1);
    machineLayout->addWidget(d->copyMachineBtn);
    layout->addWidget(machineFrame);
    connect(d->copyMachineBtn, &QPushButton::clicked, this, [this]() {
        QApplication::clipboard()->setText(bili::MachineId::current());
        d->copyMachineBtn->setText(QStringLiteral("已复制"));
        QTimer::singleShot(2000, this, [this]() { d->copyMachineBtn->setText(QStringLiteral("复制")); });
    });

    auto* machineHint = new QLabel(QStringLiteral("购买时请将此机器码发送给客服，激活码将绑定本机。"), this);
    machineHint->setStyleSheet(QStringLiteral("color: %1; font-size: 11px;").arg(theme::TEXT_3));
    layout->addWidget(machineHint);

    // 分隔线
    auto* sep1 = new QFrame(this);
    sep1->setFrameShape(QFrame::HLine);
    sep1->setStyleSheet(QStringLiteral("color: %1;").arg(theme::BORDER));
    layout->addWidget(sep1);

    // 定价卡片标题
    auto* pricingTitle = new QLabel(QStringLiteral("选择会员方案"), this);
    pricingTitle->setStyleSheet(QStringLiteral("color: %1; font-size: 14px; font-weight: 600;").arg(theme::TEXT));
    layout->addWidget(pricingTitle);

    // 三栏定价卡片
    auto* pricingLayout = new QHBoxLayout();
    pricingLayout->setSpacing(10);

    // 卡片 1: 免费试用
    auto* card1 = new QFrame(this);
    card1->setStyleSheet(QStringLiteral(
        "QFrame { background-color: %1; border: 1px solid %2; border-radius: 8px; }"
    ).arg(theme::CARD, theme::BORDER));
    auto* card1Layout = new QVBoxLayout(card1);
    card1Layout->setContentsMargins(14, 14, 14, 14);
    card1Layout->setSpacing(6);
    auto* card1Name = new QLabel(QStringLiteral("免费试用"), card1);
    card1Name->setStyleSheet(QStringLiteral("color: %1; font-size: 13px; font-weight: 600;").arg(theme::TEXT_2));
    auto* card1Price = new QLabel(QStringLiteral("\xE2\x82\xB0"), card1);
    card1Price->setStyleSheet(QStringLiteral("color: %1; font-size: 28px; font-weight: 700;").arg(theme::TEXT_3));
    auto* card1PriceSub = new QLabel(QStringLiteral("30 天免费体验"), card1);
    card1PriceSub->setStyleSheet(QStringLiteral("color: %1; font-size: 11px;").arg(theme::TEXT_3));
    const QStringList card1Features = {
        QStringLiteral("全部核心功能"),
        QStringLiteral("完整历史记录抓取"),
        QStringLiteral("搜索与筛选"),
        QStringLiteral("数据导出")
    };
    for (const auto& f : card1Features) {
        auto* feat = new QLabel(QStringLiteral("\xE2\x9C\x93  ") + f, card1);
        feat->setStyleSheet(QStringLiteral("color: %1; font-size: 11px;").arg(theme::TEXT_3));
        card1Layout->addWidget(feat);
    }
    card1Layout->addWidget(card1Name);
    card1Layout->addWidget(card1Price);
    card1Layout->addWidget(card1PriceSub);
    card1Layout->addStretch();
    pricingLayout->addWidget(card1);

    // 卡片 2: 年度会员 (推荐)
    auto* card2 = new QFrame(this);
    card2->setStyleSheet(QStringLiteral(
        "QFrame { background-color: %1; border: 2px solid %2; border-radius: 8px; }"
    ).arg(theme::CARD, theme::PINK));
    auto* card2Layout = new QVBoxLayout(card2);
    card2Layout->setContentsMargins(14, 14, 14, 14);
    card2Layout->setSpacing(6);
    auto* card2Badge = new QLabel(QStringLiteral("  \xE6\x8E\xA8\xE8\x8D\x90  "), card2);
    card2Badge->setStyleSheet(QStringLiteral(
        "background-color: %1; color: white; border-radius: 3px; font-size: 10px; padding: 2px 8px; font-weight: 600;"
    ).arg(theme::PINK));
    card2Badge->setAlignment(Qt::AlignCenter);
    auto* card2Name = new QLabel(QStringLiteral("年度会员"), card2);
    card2Name->setStyleSheet(QStringLiteral("color: %1; font-size: 13px; font-weight: 600;").arg(theme::TEXT));
    auto* card2Price = new QLabel(QStringLiteral("\xC2\xA568"), card2);
    card2Price->setStyleSheet(QStringLiteral("color: %1; font-size: 28px; font-weight: 700;").arg(theme::PINK));
    auto* card2PriceSub = new QLabel(QStringLiteral("/ 年  \xE2\x89\x88 5.7 \xE5\x85\x83/\xE6\x9C\x88"), card2);
    card2PriceSub->setStyleSheet(QStringLiteral("color: %1; font-size: 11px;").arg(theme::TEXT_3));
    const QStringList card2Features = {
        QStringLiteral("免费试用全部功能"),
        QStringLiteral("无限次历史记录抓取"),
        QStringLiteral("高级数据导出 (Excel/HTML)"),
        QStringLiteral("优先技术支持"),
        QStringLiteral("永久免费更新")
    };
    for (const auto& f : card2Features) {
        auto* feat = new QLabel(QStringLiteral("\xE2\x9C\x93  ") + f, card2);
        feat->setStyleSheet(QStringLiteral("color: %1; font-size: 11px;").arg(theme::TEXT_2));
        card2Layout->addWidget(feat);
    }
    card2Layout->addWidget(card2Badge);
    card2Layout->addWidget(card2Name);
    card2Layout->addWidget(card2Price);
    card2Layout->addWidget(card2PriceSub);
    card2Layout->addStretch();
    pricingLayout->addWidget(card2);

    // 卡片 3: 永久买断 (首发特惠)
    auto* card3 = new QFrame(this);
    card3->setStyleSheet(QStringLiteral(
        "QFrame { background-color: %1; border: 2px solid %2; border-radius: 8px; }"
    ).arg(theme::CARD, theme::WARNING));
    auto* card3Layout = new QVBoxLayout(card3);
    card3Layout->setContentsMargins(14, 14, 14, 14);
    card3Layout->setSpacing(6);
    auto* card3Badge = new QLabel(QStringLiteral("  \xE9\xA6\x96\xE5\x8F\x91\xE7\x89\xB9\xE6\x83\xA0  "), card3);
    card3Badge->setStyleSheet(QStringLiteral(
        "background-color: %1; color: white; border-radius: 3px; font-size: 10px; padding: 2px 8px; font-weight: 600;"
    ).arg(theme::WARNING));
    card3Badge->setAlignment(Qt::AlignCenter);
    auto* card3Name = new QLabel(QStringLiteral("永久买断"), card3);
    card3Name->setStyleSheet(QStringLiteral("color: %1; font-size: 13px; font-weight: 600;").arg(theme::TEXT));
    auto* card3Price = new QLabel(QStringLiteral("\xC2\xA538"), card3);
    card3Price->setStyleSheet(QStringLiteral("color: %1; font-size: 28px; font-weight: 700;").arg(theme::WARNING));
    auto* card3PriceSub = new QLabel(QStringLiteral("\xE4\xB8\x80\xE6\xAC\xA1\xE6\x80\xA7\xE4\xBB\x98\xE8\xB4\xB9\xEF\xBC\x8C\xE6\xB0\xB8\xE4\xB9\x85\xE4\xBD\xBF\xE7\x94\xA8"), card3);
    card3PriceSub->setStyleSheet(QStringLiteral("color: %1; font-size: 11px;").arg(theme::TEXT_3));
    const QStringList card3Features = {
        QStringLiteral("全部年度会员功能"),
        QStringLiteral("永久授权，无续费"),
        QStringLiteral("所有未来新功能"),
        QStringLiteral("专属客服通道"),
        QStringLiteral("首发买一送一活动")
    };
    for (const auto& f : card3Features) {
        auto* feat = new QLabel(QStringLiteral("\xE2\x9C\x93  ") + f, card3);
        feat->setStyleSheet(QStringLiteral("color: %1; font-size: 11px;").arg(theme::TEXT_2));
        card3Layout->addWidget(feat);
    }
    card3Layout->addWidget(card3Badge);
    card3Layout->addWidget(card3Name);
    card3Layout->addWidget(card3Price);
    card3Layout->addWidget(card3PriceSub);
    card3Layout->addStretch();
    pricingLayout->addWidget(card3);

    layout->addLayout(pricingLayout);

    // 购买方式
    auto* purchaseFrame = new QFrame(this);
    purchaseFrame->setStyleSheet(QStringLiteral(
        "QFrame { background-color: %1; border-radius: 6px; }"
    ).arg(theme::PINK_LIGHT));
    auto* purchaseLayout = new QVBoxLayout(purchaseFrame);
    purchaseLayout->setContentsMargins(12, 10, 12, 10);
    auto* purchaseTitle = new QLabel(QStringLiteral("\xF0\x9F\x9B\x92  购买方式"), purchaseFrame);
    purchaseTitle->setStyleSheet(QStringLiteral("color: %1; font-size: 12px; font-weight: 600;").arg(theme::TEXT));
    auto* purchaseText = new QLabel(QStringLiteral(
        "添加客服微信 <b>BiliHistoryCS</b>，发送机器码并选择会员方案，"
        "支付后即可获取专属激活码。"
    ), purchaseFrame);
    purchaseText->setStyleSheet(QStringLiteral("color: %1; font-size: 11px;").arg(theme::TEXT_2));
    purchaseText->setWordWrap(true);
    purchaseText->setTextFormat(Qt::RichText);
    auto* promoText = new QLabel(QStringLiteral(
        "\xF0\x9F\x8E\x81  首发优惠：首月购买年度会员即送永久会员！"
    ), purchaseFrame);
    promoText->setStyleSheet(QStringLiteral("color: %1; font-size: 11px; font-weight: 600;").arg(theme::PINK));
    purchaseLayout->addWidget(purchaseTitle);
    purchaseLayout->addWidget(purchaseText);
    purchaseLayout->addWidget(promoText);
    layout->addWidget(purchaseFrame);

    // 分隔线
    auto* sep2 = new QFrame(this);
    sep2->setFrameShape(QFrame::HLine);
    sep2->setStyleSheet(QStringLiteral("color: %1;").arg(theme::BORDER));
    layout->addWidget(sep2);

    // 激活码输入
    auto* codeHint = new QLabel(QStringLiteral("已有激活码？请输入："), this);
    codeHint->setStyleSheet(QStringLiteral("color: %1; font-size: 12px; font-weight: 600;").arg(theme::TEXT_2));
    layout->addWidget(codeHint);

    d->codeEdit = new QLineEdit(this);
    d->codeEdit->setPlaceholderText(QStringLiteral("在此粘贴激活码..."));
    d->codeEdit->setStyleSheet(QStringLiteral(
        "QLineEdit { border: 1px solid %1; border-radius: 6px; padding: 8px; font-size: 12px; }"
        "QLineEdit:focus { border-color: %2; }"
    ).arg(theme::BORDER, theme::PINK));
    layout->addWidget(d->codeEdit);

    // 按钮
    auto* buttons = new QDialogButtonBox(QDialogButtonBox::Cancel, this);
    auto* activateBtn = new QPushButton(QStringLiteral("立即激活"), this);
    activateBtn->setObjectName(QStringLiteral("primaryButton"));
    connect(activateBtn, &QPushButton::clicked, this, &ActivationDialog::onActivate);
    buttons->addButton(activateBtn, QDialogButtonBox::AcceptRole);
    buttons->button(QDialogButtonBox::Cancel)->setText(QStringLiteral("关闭"));
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
    layout->addWidget(buttons);
}

void ActivationDialog::updateStatus()
{
    if (d->licenseManager->isLicensed(bili::Paths::licensePath())) {
        d->statusLabel->setText(QStringLiteral("\xE2\x9C\x85  当前状态: 已激活会员"));
        d->statusLabel->setStyleSheet(QStringLiteral("color: %1; font-size: 13px; font-weight: 600;").arg(theme::SUCCESS));
    } else {
        d->statusLabel->setText(QStringLiteral("\xE2\x8F\xB0  当前状态: 未激活（试用中）"));
        d->statusLabel->setStyleSheet(QStringLiteral("color: %1; font-size: 13px;").arg(theme::TEXT_2));
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
