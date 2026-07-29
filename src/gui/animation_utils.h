#pragma once

#include <QColor>
#include <QEvent>
#include <QObject>
#include <QPushButton>
#include <QRect>
#include <QWidget>

namespace bili::gui::animation {

class HoverEventFilter : public QObject {
    Q_OBJECT
public:
    explicit HoverEventFilter(QObject* parent = nullptr);

signals:
    void hoverEntered(QWidget* widget);
    void hoverLeft(QWidget* widget);

protected:
    bool eventFilter(QObject* watched, QEvent* event) override;
};

void installButtonScaleAnimation(QPushButton* button, qreal scale = 1.03, int duration = 120);
void installCardScaleAnimation(QWidget* widget, qreal scale = 1.015, int duration = 120);
void installCardHoverAnimation(QWidget* widget, const QColor& hoverBg, int duration = 120);
void fadeIn(QWidget* widget, int duration = 180);
void fadeOut(QWidget* widget, int duration = 180);
void animateWidgetGeometry(QWidget* widget, const QRect& target, int duration = 180);

} // namespace bili::gui::animation
