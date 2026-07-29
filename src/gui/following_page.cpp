#include "following_page.h"

#include "animation_utils.h"
#include "theme.h"

#include <QHBoxLayout>
#include <QLabel>
#include <QPainter>
#include <QPainterPath>
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

FollowingPage::FollowingPage(QWidget* parent)
    : QWidget(parent)
    , m_loader(new ImageLoader(this))
{
    auto* mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(0, 0, 0, 0);

    m_scroll = new QScrollArea(this);
    m_scroll->setWidgetResizable(true);
    m_scroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);

    m_container = new QWidget(this);
    m_grid = new QGridLayout(m_container);
    m_grid->setContentsMargins(20, 20, 20, 20);
    m_grid->setSpacing(16);

    m_scroll->setWidget(m_container);
    mainLayout->addWidget(m_scroll);
}

void FollowingPage::loadData(const FollowingList& records)
{
    QLayoutItem* item = nullptr;
    while ((item = m_grid->takeAt(0)) != nullptr) {
        if (item->widget()) {
            item->widget()->deleteLater();
        }
        delete item;
    }

    constexpr int columns = 2;
    for (size_t i = 0; i < records.size(); ++i) {
        auto* card = new FollowingCard(records[i], m_loader, m_container);
        m_grid->addWidget(card, static_cast<int>(i) / columns, static_cast<int>(i) % columns);
    }

    m_grid->setRowStretch(static_cast<int>(records.size()) / columns + 1, 1);
}

} // namespace bili::gui
