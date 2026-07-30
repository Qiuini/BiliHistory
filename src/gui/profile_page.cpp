#include "profile_page.h"

#include "core/i_config.h"
#include "core/models.h"
#include "core/exceptions.h"
#include "image_loader.h"
#include "network/i_api_client.h"
#include "network/i_user_profile_fetcher.h"
#include "theme.h"

#include <QHBoxLayout>
#include <QLabel>
#include <QPainter>
#include <QPainterPath>
#include <QPushButton>
#include <QScrollArea>
#include <QSize>
#include <QUrl>
#include <QVBoxLayout>

namespace bili::gui {

namespace {

constexpr QSize AvatarSize(96, 96);

QPixmap roundedAvatar(const QPixmap& source, const QSize& size)
{
    QPixmap target(size);
    target.fill(Qt::transparent);

    QPainter painter(&target);
    painter.setRenderHint(QPainter::Antialiasing, true);
    painter.setRenderHint(QPainter::SmoothPixmapTransform, true);

    QPainterPath path;
    path.addEllipse(QRectF(QPointF(0, 0), QSizeF(size)));
    painter.setClipPath(path);
    painter.drawPixmap(target.rect(), source.scaled(size, Qt::KeepAspectRatioByExpanding, Qt::SmoothTransformation));

    return target;
}

QLabel* makeLabel(QWidget* parent, const QString& text, const QString& style)
{
    auto* label = new QLabel(text, parent);
    label->setStyleSheet(style);
    label->setWordWrap(true);
    return label;
}

} // namespace

class ProfilePage::Impl {
public:
    Impl(ProfilePage* q_, bili::IConfig* config_,
         bili::IUserProfileFetcher* fetcher_, ImageLoader* imageLoader_)
        : q(q_)
        , config(config_)
        , fetcher(fetcher_)
        , imageLoader(imageLoader_)
    {
        Q_ASSERT(config != nullptr);
        Q_ASSERT(fetcher != nullptr);
        Q_ASSERT(imageLoader != nullptr);
        buildUi();

        QObject::connect(fetcher, &bili::IUserProfileFetcher::finished,
                         q, [this](const bili::UserInfo& info) { updateProfile(info); });
        QObject::connect(fetcher, &bili::IUserProfileFetcher::error,
                         q, [this](const bili::NetworkException& e) {
                             statusLabel->setText(QStringLiteral("加载失败: %1").arg(e.message()));
                         });
    }

    void buildUi()
    {
        auto* mainLayout = new QVBoxLayout(q);
        mainLayout->setContentsMargins(0, 0, 0, 0);
        mainLayout->setSpacing(0);

        auto* scroll = new QScrollArea(q);
        scroll->setWidgetResizable(true);
        scroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
        scroll->setStyleSheet(QStringLiteral("QScrollArea { border: none; background-color: transparent; }"));

        auto* content = new QWidget(scroll);
        auto* contentLayout = new QVBoxLayout(content);
        contentLayout->setContentsMargins(20, 20, 20, 20);
        contentLayout->setSpacing(16);

        auto* card = new QWidget(content);
        card->setStyleSheet(QStringLiteral(
            "QWidget { background-color: %1; border: 1px solid %2; border-radius: 12px; }"
        ).arg(theme::CARD, theme::BORDER));
        auto* cardLayout = new QHBoxLayout(card);
        cardLayout->setContentsMargins(20, 20, 20, 20);
        cardLayout->setSpacing(20);

        avatarLabel = new QLabel(card);
        avatarLabel->setFixedSize(AvatarSize);
        avatarLabel->setStyleSheet(QStringLiteral(
            "QLabel { background-color: %1; border-radius: 48px; }"
        ).arg(theme::SURFACE_HOVER));
        avatarLabel->setAlignment(Qt::AlignCenter);
        avatarLabel->setText(QStringLiteral("头像"));
        cardLayout->addWidget(avatarLabel);

        auto* infoLayout = new QVBoxLayout();
        infoLayout->setSpacing(8);

        nameLabel = makeLabel(card, QStringLiteral("-"),
                              QStringLiteral("color: %1; font-size: 20px; font-weight: 700;").arg(theme::TEXT));
        infoLayout->addWidget(nameLabel);

        auto* metaLayout = new QHBoxLayout();
        metaLayout->setSpacing(16);
        uidLabel = makeLabel(card, QStringLiteral("UID: -"),
                             QStringLiteral("color: %1; font-size: 13px;").arg(theme::TEXT_2));
        levelLabel = makeLabel(card, QStringLiteral("等级: -"),
                               QStringLiteral("color: %1; font-size: 13px;").arg(theme::TEXT_2));
        regLabel = makeLabel(card, QStringLiteral("注册时间: -"),
                             QStringLiteral("color: %1; font-size: 13px;").arg(theme::TEXT_2));
        metaLayout->addWidget(uidLabel);
        metaLayout->addWidget(levelLabel);
        metaLayout->addWidget(regLabel);
        metaLayout->addStretch();
        infoLayout->addLayout(metaLayout);

        signLabel = makeLabel(card, QStringLiteral("签名: -"),
                              QStringLiteral("color: %1; font-size: 12px;").arg(theme::TEXT_3));
        infoLayout->addWidget(signLabel);

        cardLayout->addLayout(infoLayout, 1);

        contentLayout->addWidget(card);

        statusLabel = makeLabel(content, QStringLiteral("点击刷新加载个人资料"),
                                QStringLiteral("color: %1; font-size: 12px;").arg(theme::TEXT_3));
        contentLayout->addWidget(statusLabel);

        auto* refreshBtn = new QPushButton(QStringLiteral("刷新资料"), content);
        refreshBtn->setObjectName(QStringLiteral("primaryButton"));
        refreshBtn->setFixedWidth(120);
        QObject::connect(refreshBtn, &QPushButton::clicked, q, &ProfilePage::onRefreshClicked);
        contentLayout->addWidget(refreshBtn);

        contentLayout->addStretch();
        scroll->setWidget(content);
        mainLayout->addWidget(scroll);
    }

    void updateProfile(const bili::UserInfo& info)
    {
        if (info.mid == 0) {
            statusLabel->setText(QStringLiteral("未登录，请检查 Cookie 设置"));
            return;
        }

        nameLabel->setText(info.name.isEmpty() ? QStringLiteral("B站用户") : info.name);
        uidLabel->setText(QStringLiteral("UID: %1").arg(info.mid));
        levelLabel->setText(QStringLiteral("等级: LV%1").arg(info.level));
        regLabel->setText(info.registrationTime.isValid()
                              ? QStringLiteral("注册时间: %1").arg(info.registrationTimeText)
                              : QStringLiteral("注册时间: 未知"));
        signLabel->setText(info.sign.isEmpty()
                               ? QStringLiteral("签名: 暂无")
                               : QStringLiteral("签名: %1").arg(info.sign));

        if (!info.faceUrl.isEmpty()) {
            avatarLabel->setText(QString());
            imageLoader->load(QUrl(info.faceUrl), [this](QPixmap pixmap) {
                avatarLabel->setPixmap(roundedAvatar(pixmap, AvatarSize));
            }, AvatarSize * 2);
        }

        statusLabel->setText(QStringLiteral("资料加载完成"));
    }

    ProfilePage* q = nullptr;
    bili::IConfig* config = nullptr;
    bili::IUserProfileFetcher* fetcher = nullptr;
    ImageLoader* imageLoader = nullptr;

    QLabel* avatarLabel = nullptr;
    QLabel* nameLabel = nullptr;
    QLabel* uidLabel = nullptr;
    QLabel* levelLabel = nullptr;
    QLabel* regLabel = nullptr;
    QLabel* signLabel = nullptr;
    QLabel* statusLabel = nullptr;
};

ProfilePage::ProfilePage(bili::IConfig* config,
                         bili::IUserProfileFetcher* fetcher,
                         ImageLoader* imageLoader,
                         QWidget* parent)
    : QWidget(parent)
    , d(std::make_unique<Impl>(this, config, fetcher, imageLoader))
{
}

ProfilePage::~ProfilePage() = default;

void ProfilePage::refresh()
{
    const QString cookie = d->config->cookie();
    if (cookie.isEmpty()) {
        d->statusLabel->setText(QStringLiteral("请先设置 Cookie"));
        return;
    }
    d->statusLabel->setText(QStringLiteral("正在加载..."));
    d->fetcher->fetch(cookie);
}

void ProfilePage::onRefreshClicked()
{
    refresh();
}

} // namespace bili::gui
