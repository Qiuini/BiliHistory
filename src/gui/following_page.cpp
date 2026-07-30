#include "following_page.h"

#include "animation_utils.h"
#include "image_loader.h"
#include "theme.h"

#include <QGridLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QPainter>
#include <QPainterPath>
#include <QScrollArea>
#include <QVBoxLayout>

namespace bili::gui {

namespace {

class AvatarLabel : public QLabel {
public:
    explicit AvatarLabel(QWidget* parent = nullptr)
        : QLabel(parent)
    {
        setFixedSize(56, 56);
        setAlignment(Qt::AlignCenter);
    }

    void setPixmap(const QPixmap& pix)
    {
        m_pixmap = pix;
        update();
    }

    void setPlaceholder(const QString& text, const QColor& color)
    {
        m_text = text;
        m_color = color;
        m_pixmap = QPixmap();
        update();
    }

protected:
    void paintEvent(QPaintEvent*) override
    {
        QPainter painter(this);
        painter.setRenderHint(QPainter::Antialiasing);
        const QRect r = rect();

        if (!m_pixmap.isNull()) {
            QPainterPath path;
            path.addEllipse(r);
            painter.setClipPath(path);
            painter.drawPixmap(r, m_pixmap);
        } else {
            painter.setPen(Qt::NoPen);
            painter.setBrush(m_color);
            painter.drawEllipse(r);

            painter.setPen(Qt::white);
            QFont f = font();
            f.setPixelSize(20);
            f.setBold(true);
            painter.setFont(f);
            painter.drawText(r, Qt::AlignCenter, m_text);
        }
    }

private:
    QPixmap m_pixmap;
    QString m_text;
    QColor m_color;
};

QColor colorForMid(qint64 mid)
{
    const int hue = static_cast<int>((mid * 137) % 360);
    return QColor::fromHsv(hue, 180, 240);
}

} // namespace

FollowingCard::FollowingCard(const FollowingUser& user, ImageLoader* loader, QWidget* parent)
    : QFrame(parent)
{
    setObjectName(QStringLiteral("FollowingCard"));
    setFrameShape(QFrame::NoFrame);
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    setFixedHeight(88);
    setStyleSheet(QStringLiteral(
        "QFrame#FollowingCard { background-color: %1; border: 1px solid %2; border-radius: 12px; }"
    ).arg(theme::CARD, theme::BORDER));

    auto* layout = new QHBoxLayout(this);
    layout->setContentsMargins(16, 16, 16, 16);
    layout->setSpacing(12);

    auto* avatar = new AvatarLabel(this);
    const QString firstLetter = user.name.isEmpty()
        ? QStringLiteral("?")
        : user.name.at(0).toUpper();
    avatar->setPlaceholder(firstLetter, colorForMid(user.mid));

    if (!user.faceUrl.isEmpty() && loader) {
        loader->load(QUrl(user.faceUrl), [avatar](const QPixmap& pix) {
            avatar->setPixmap(pix);
        }, QSize(56, 56));
    }

    auto* textLayout = new QVBoxLayout();
    textLayout->setSpacing(4);
    textLayout->setContentsMargins(0, 0, 0, 0);

    auto* nameLayout = new QHBoxLayout();
    nameLayout->setSpacing(6);

    auto* nameLabel = new QLabel(user.name, this);
    nameLabel->setStyleSheet(QStringLiteral("color: %1; font-size: 14px; font-weight: 600;").arg(theme::TEXT));
    nameLayout->addWidget(nameLabel);

    if (user.officialVerify != 0) {
        auto* badge = new QLabel(QStringLiteral("官方认证"), this);
        badge->setStyleSheet(QStringLiteral(
            "color: #FFFFFF; background-color: %1; border-radius: 4px; padding: 1px 4px; font-size: 10px;"
        ).arg(theme::PINK));
        badge->setFixedHeight(16);
        nameLayout->addWidget(badge);
    }
    nameLayout->addStretch();

    auto* signLabel = new QLabel(user.sign, this);
    signLabel->setStyleSheet(QStringLiteral("color: %1; font-size: 12px;").arg(theme::TEXT_3));
    signLabel->setWordWrap(true);

    textLayout->addLayout(nameLayout);
    textLayout->addWidget(signLabel);
    textLayout->addStretch();

    layout->addWidget(avatar);
    layout->addLayout(textLayout, 1);

    animation::installCardScaleAnimation(this);
    animation::installCardHoverAnimation(this, QColor(theme::SURFACE_HOVER));
}

class FollowingPage::Impl {
public:
    QScrollArea* scroll = nullptr;
    QWidget* container = nullptr;
    QGridLayout* grid = nullptr;
    ImageLoader* loader = nullptr;
};

FollowingPage::FollowingPage(ImageLoader* loader, QWidget* parent)
    : QWidget(parent)
    , d(std::make_unique<Impl>())
{
    d->loader = loader;

    auto* mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(0, 0, 0, 0);

    d->scroll = new QScrollArea(this);
    d->scroll->setWidgetResizable(true);
    d->scroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);

    d->container = new QWidget(this);
    d->grid = new QGridLayout(d->container);
    d->grid->setContentsMargins(20, 20, 20, 20);
    d->grid->setSpacing(16);

    d->scroll->setWidget(d->container);
    mainLayout->addWidget(d->scroll);
}

FollowingPage::~FollowingPage() = default;

void FollowingPage::loadData(const FollowingList& records)
{
    QLayoutItem* item = nullptr;
    while ((item = d->grid->takeAt(0)) != nullptr) {
        if (item->widget()) {
            item->widget()->deleteLater();
        }
        delete item;
    }

    constexpr int columns = 2;
    for (size_t i = 0; i < records.size(); ++i) {
        auto* card = new FollowingCard(records[i], d->loader, d->container);
        d->grid->addWidget(card, static_cast<int>(i) / columns, static_cast<int>(i) % columns);
    }

    d->grid->setRowStretch(static_cast<int>(records.size()) / columns + 1, 1);
}

} // namespace bili::gui
