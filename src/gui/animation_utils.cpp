#include "animation_utils.h"

#include <QGraphicsOpacityEffect>
#include <QPropertyAnimation>
#include <QVariantAnimation>

namespace bili::gui::animation {

HoverEventFilter::HoverEventFilter(QObject* parent)
    : QObject(parent)
{
}

bool HoverEventFilter::eventFilter(QObject* watched, QEvent* event)
{
    auto* widget = qobject_cast<QWidget*>(watched);
    if (widget) {
        switch (event->type()) {
        case QEvent::Enter:
        case QEvent::HoverEnter:
            emit hoverEntered(widget);
            break;
        case QEvent::Leave:
        case QEvent::HoverLeave:
            emit hoverLeft(widget);
            break;
        default:
            break;
        }
    }
    return QObject::eventFilter(watched, event);
}

namespace {

class ScaleController : public HoverEventFilter {
    Q_OBJECT
public:
    ScaleController(QWidget* widget, qreal scale, int duration)
        : HoverEventFilter(widget)
        , m_widget(widget)
        , m_scale(scale)
        , m_duration(duration)
    {
        m_baseGeometry = widget->geometry();
    }

protected:
    bool eventFilter(QObject* watched, QEvent* event) override
    {
        if (!m_animating && event->type() == QEvent::Resize) {
            m_baseGeometry = m_widget->geometry();
        }
        return HoverEventFilter::eventFilter(watched, event);
    }

public slots:
    void onEntered(QWidget*)
    {
        animate(m_widget->geometry(), scaled(m_baseGeometry));
    }

    void onLeft(QWidget*)
    {
        animate(m_widget->geometry(), m_baseGeometry);
    }

private:
    QRect scaled(const QRect& rect) const
    {
        const int w = qRound(rect.width() * m_scale);
        const int h = qRound(rect.height() * m_scale);
        const int x = rect.x() + (rect.width() - w) / 2;
        const int y = rect.y() + (rect.height() - h) / 2;
        return QRect(x, y, w, h);
    }

    void animate(const QRect& from, const QRect& to)
    {
        auto* anim = new QPropertyAnimation(m_widget, "geometry", m_widget);
        anim->setDuration(m_duration);
        anim->setStartValue(from);
        anim->setEndValue(to);
        anim->setEasingCurve(QEasingCurve::OutCubic);
        m_animating = true;
        QObject::connect(anim, &QPropertyAnimation::finished, this, [this]() { m_animating = false; });
        anim->start(QAbstractAnimation::DeleteWhenStopped);
    }

    QWidget* m_widget;
    qreal m_scale;
    int m_duration;
    QRect m_baseGeometry;
    bool m_animating = false;
};

class CardHoverController : public HoverEventFilter {
    Q_OBJECT
public:
    CardHoverController(QWidget* widget, const QColor& hoverBg, int duration)
        : HoverEventFilter(widget)
        , m_widget(widget)
        , m_hoverBg(hoverBg)
        , m_duration(duration)
    {
        m_baseSheet = widget->styleSheet();
        m_baseBg = widget->palette().color(QPalette::Window);
    }

public slots:
    void onEntered(QWidget*)
    {
        animate(m_baseBg, m_hoverBg);
    }

    void onLeft(QWidget*)
    {
        animate(m_hoverBg, m_baseBg);
    }

private:
    void animate(const QColor& from, const QColor& to)
    {
        auto* anim = new QVariantAnimation(this);
        anim->setDuration(m_duration);
        anim->setStartValue(from);
        anim->setEndValue(to);
        anim->setEasingCurve(QEasingCurve::OutCubic);
        QObject::connect(anim, &QVariantAnimation::valueChanged, this, [this](const QVariant& value) {
            setBackground(value.value<QColor>());
        });
        anim->start(QAbstractAnimation::DeleteWhenStopped);
    }

    void setBackground(const QColor& color)
    {
        const QString obj = m_widget->objectName();
        const QString selector = obj.isEmpty()
            ? QStringLiteral("QWidget")
            : QStringLiteral("%1#%2").arg(QString::fromLatin1(m_widget->metaObject()->className()), obj);
        const QString rule = QStringLiteral("\n%1 { background-color: %2; }").arg(selector, color.name());
        m_widget->setStyleSheet(m_baseSheet + rule);
    }

    QWidget* m_widget;
    QColor m_hoverBg;
    QColor m_baseBg;
    QString m_baseSheet;
    int m_duration;
};

} // namespace

void installButtonScaleAnimation(QPushButton* button, qreal scale, int duration)
{
    auto* controller = new ScaleController(button, scale, duration);
    button->installEventFilter(controller);
    QObject::connect(controller, &HoverEventFilter::hoverEntered, controller, &ScaleController::onEntered);
    QObject::connect(controller, &HoverEventFilter::hoverLeft, controller, &ScaleController::onLeft);
}

void installCardScaleAnimation(QWidget* widget, qreal scale, int duration)
{
    auto* controller = new ScaleController(widget, scale, duration);
    widget->installEventFilter(controller);
    QObject::connect(controller, &HoverEventFilter::hoverEntered, controller, &ScaleController::onEntered);
    QObject::connect(controller, &HoverEventFilter::hoverLeft, controller, &ScaleController::onLeft);
}

void installCardHoverAnimation(QWidget* widget, const QColor& hoverBg, int duration)
{
    auto* controller = new CardHoverController(widget, hoverBg, duration);
    widget->installEventFilter(controller);
    QObject::connect(controller, &HoverEventFilter::hoverEntered, controller, &CardHoverController::onEntered);
    QObject::connect(controller, &HoverEventFilter::hoverLeft, controller, &CardHoverController::onLeft);
}

void fadeIn(QWidget* widget, int duration)
{
    if (!widget) return;

    auto* effect = qobject_cast<QGraphicsOpacityEffect*>(widget->graphicsEffect());
    if (!effect) {
        effect = new QGraphicsOpacityEffect(widget);
        widget->setGraphicsEffect(effect);
    }
    effect->setOpacity(0.0);
    widget->show();

    auto* anim = new QPropertyAnimation(effect, "opacity", effect);
    anim->setDuration(duration);
    anim->setStartValue(0.0);
    anim->setEndValue(1.0);
    anim->setEasingCurve(QEasingCurve::OutCubic);
    anim->start(QAbstractAnimation::DeleteWhenStopped);
}

void fadeOut(QWidget* widget, int duration)
{
    if (!widget) return;

    auto* effect = qobject_cast<QGraphicsOpacityEffect*>(widget->graphicsEffect());
    if (!effect) {
        effect = new QGraphicsOpacityEffect(widget);
        widget->setGraphicsEffect(effect);
    }

    auto* anim = new QPropertyAnimation(effect, "opacity", effect);
    anim->setDuration(duration);
    anim->setStartValue(effect->opacity());
    anim->setEndValue(0.0);
    anim->setEasingCurve(QEasingCurve::OutCubic);
    QObject::connect(anim, &QPropertyAnimation::finished, widget, &QWidget::hide);
    anim->start(QAbstractAnimation::DeleteWhenStopped);
}

void animateWidgetGeometry(QWidget* widget, const QRect& target, int duration)
{
    if (!widget) return;

    auto* anim = new QPropertyAnimation(widget, "geometry", widget);
    anim->setDuration(duration);
    anim->setStartValue(widget->geometry());
    anim->setEndValue(target);
    anim->setEasingCurve(QEasingCurve::OutCubic);
    anim->start(QAbstractAnimation::DeleteWhenStopped);
}

} // namespace bili::gui::animation

#include "animation_utils.moc"
