#include "settings_dialog.h"

#include "theme.h"
#include "core/config.h"

#include <QDialogButtonBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QMessageBox>
#include <QPushButton>
#include <QTextEdit>
#include <QVBoxLayout>

namespace bili::gui {

SettingsDialog::SettingsDialog(QWidget* parent)
    : QDialog(parent)
{
    setWindowTitle(QStringLiteral("Cookie 设置"));
    setMinimumWidth(480);
    setFixedWidth(480);
    buildUi();
}

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

    m_cookieEdit = new QTextEdit(this);
    m_cookieEdit->setPlaceholderText(QStringLiteral("在此粘贴 Cookie..."));
    m_cookieEdit->setPlainText(bili::Config::instance().cookie());
    m_cookieEdit->setMinimumHeight(120);
    layout->addWidget(m_cookieEdit);

    auto* buttons = new QDialogButtonBox(QDialogButtonBox::Cancel | QDialogButtonBox::Save, this);
    buttons->button(QDialogButtonBox::Cancel)->setText(QStringLiteral("取消"));
    buttons->button(QDialogButtonBox::Save)->setText(QStringLiteral("保存"));
    connect(buttons, &QDialogButtonBox::accepted, this, &SettingsDialog::onSave);
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
    layout->addWidget(buttons);
}

void SettingsDialog::onSave()
{
    const QString cookie = m_cookieEdit->toPlainText().trimmed();
    if (cookie.isEmpty()) {
        QMessageBox::warning(this, QStringLiteral("Cookie 为空"), QStringLiteral("请输入有效的 Cookie 字符串。"));
        return;
    }

    bili::Config::instance().saveCookie(cookie);
    accept();
}

} // namespace bili::gui
