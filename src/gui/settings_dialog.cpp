#include "settings_dialog.h"

#include "theme.h"
#include "core/i_config.h"
#include "core/i_feature_access.h"

#include <QCheckBox>
#include <QDialogButtonBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QMessageBox>
#include <QPushButton>
#include <QTextEdit>
#include <QVBoxLayout>
#include <QtGlobal>

namespace bili::gui {

class SettingsDialog::Impl {
public:
    bili::IConfig* config = nullptr;
    bili::IFeatureAccess* featureAccess = nullptr;
    QTextEdit* cookieEdit = nullptr;
    QCheckBox* fetchAllCheck = nullptr;
    QLabel* proHintLabel = nullptr;
};

SettingsDialog::SettingsDialog(bili::IConfig* config,
                               bili::IFeatureAccess* featureAccess,
                               QWidget* parent)
    : QDialog(parent)
    , d(std::make_unique<Impl>())
{
    d->config = config;
    d->featureAccess = featureAccess;
    Q_ASSERT(d->config != nullptr);
    setWindowTitle(QStringLiteral("Cookie 设置"));
    setMinimumWidth(480);
    setFixedWidth(480);
    buildUi();
}

SettingsDialog::~SettingsDialog() = default;

void SettingsDialog::buildUi()
{
    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(20, 20, 20, 20);
    layout->setSpacing(12);

    auto* title = new QLabel(QStringLiteral("设置 Bilibili Cookie"), this);
    title->setStyleSheet(QStringLiteral("color: %1; font-size: 16px; font-weight: 600;").arg(theme::TEXT));
    layout->addWidget(title);

    auto* hint = new QLabel(QStringLiteral("请将完整的 Cookie 字符串粘贴到下方："), this);
    hint->setStyleSheet(QStringLiteral("color: %1; font-size: 12px;").arg(theme::TEXT_3));
    layout->addWidget(hint);

    d->cookieEdit = new QTextEdit(this);
    d->cookieEdit->setPlaceholderText(QStringLiteral("在此粘贴 Cookie..."));
    d->cookieEdit->setPlainText(d->config->cookie());
    d->cookieEdit->setMinimumHeight(120);
    layout->addWidget(d->cookieEdit);

    auto* syncTitle = new QLabel(QStringLiteral("同步方式"), this);
    syncTitle->setStyleSheet(QStringLiteral("color: %1; font-size: 13px; font-weight: 600;").arg(theme::TEXT));
    layout->addWidget(syncTitle);

    d->fetchAllCheck = new QCheckBox(QStringLiteral("完整同步所有历史记录（关闭则只同步最近记录）"), this);
    d->fetchAllCheck->setChecked(d->config->fetchAll());
    layout->addWidget(d->fetchAllCheck);

    auto* syncHint = new QLabel(QStringLiteral("提示：完整同步会抓取全部历史记录，耗时较长；增量同步只获取最新部分。"), this);
    syncHint->setStyleSheet(QStringLiteral("color: %1; font-size: 11px;").arg(theme::TEXT_3));
    syncHint->setWordWrap(true);
    layout->addWidget(syncHint);

    d->proHintLabel = new QLabel(this);
    d->proHintLabel->setStyleSheet(QStringLiteral("color: %1; font-size: 11px;").arg(theme::PINK));
    d->proHintLabel->setWordWrap(true);
    layout->addWidget(d->proHintLabel);

    const bool proUnlocked = d->featureAccess && d->featureAccess->isProUnlocked();
    if (!proUnlocked) {
        d->fetchAllCheck->setChecked(true);
        d->fetchAllCheck->setEnabled(false);
        d->proHintLabel->setText(QStringLiteral("增量同步为专业版功能，试用或激活后可使用。"));
    } else {
        d->proHintLabel->hide();
    }

    auto* buttons = new QDialogButtonBox(QDialogButtonBox::Cancel | QDialogButtonBox::Save, this);
    buttons->button(QDialogButtonBox::Cancel)->setText(QStringLiteral("取消"));
    buttons->button(QDialogButtonBox::Save)->setText(QStringLiteral("保存"));
    connect(buttons, &QDialogButtonBox::accepted, this, &SettingsDialog::onSave);
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
    layout->addWidget(buttons);
}

void SettingsDialog::onSave()
{
    const QString cookie = d->cookieEdit->toPlainText().trimmed();
    if (cookie.isEmpty()) {
        QMessageBox::warning(this, QStringLiteral("Cookie 为空"), QStringLiteral("请输入有效的 Cookie 字符串。"));
        return;
    }

    d->config->saveCookie(cookie);
    if (d->fetchAllCheck) {
        d->config->setFetchAll(d->fetchAllCheck->isChecked());
    }
    accept();
}

} // namespace bili::gui