"""
通用 UI 动画工具

为按钮、卡片、弹窗、横幅等提供一致的 QPropertyAnimation 封装，
减少样板代码并保证动画曲线/时长统一。
"""
from typing import Callable, Optional

from PyQt6.QtCore import (
    QPropertyAnimation, QAbstractAnimation, QEasingCurve, QPoint, QSize, Qt,
    pyqtSignal, QObject, QVariantAnimation
)
from PyQt6.QtGui import QColor
from PyQt6.QtWidgets import (
    QWidget, QGraphicsOpacityEffect, QPushButton, QFrame, QApplication,
    QTableView
)


# 统一动画时长（ms）
DURATION_FAST = 120
DURATION_NORMAL = 180
DURATION_SLOW = 250


def _ensure_opacity_effect(widget: QWidget) -> QGraphicsOpacityEffect:
    effect = widget.graphicsEffect()
    if isinstance(effect, QGraphicsOpacityEffect):
        return effect
    effect = QGraphicsOpacityEffect(widget)
    widget.setGraphicsEffect(effect)
    return effect


def fade_in(widget: QWidget, duration: int = DURATION_NORMAL,
            on_finished: Optional[Callable] = None) -> QPropertyAnimation:
    """淡入显示 widget（先确保 visible）"""
    effect = _ensure_opacity_effect(widget)
    effect.setOpacity(0.0)
    widget.setVisible(True)

    anim = QPropertyAnimation(effect, b"opacity", widget)
    anim.setDuration(duration)
    anim.setStartValue(0.0)
    anim.setEndValue(1.0)
    anim.setEasingCurve(QEasingCurve.Type.InOutQuad)
    if on_finished:
        anim.finished.connect(on_finished)
    anim.start(QPropertyAnimation.DeletionPolicy.DeleteWhenStopped)
    return anim


def fade_out(widget: QWidget, duration: int = DURATION_NORMAL,
             on_finished: Optional[Callable] = None) -> QPropertyAnimation:
    """淡出隐藏 widget"""
    effect = _ensure_opacity_effect(widget)

    anim = QPropertyAnimation(effect, b"opacity", widget)
    anim.setDuration(duration)
    anim.setStartValue(effect.opacity() if effect.opacity() > 0 else 1.0)
    anim.setEndValue(0.0)
    anim.setEasingCurve(QEasingCurve.Type.InOutQuad)

    def _finish():
        widget.setVisible(False)
        effect.setOpacity(1.0)
        if on_finished:
            on_finished()

    anim.finished.connect(_finish)
    anim.start(QPropertyAnimation.DeletionPolicy.DeleteWhenStopped)
    return anim


def slide_in(widget: QWidget, direction: str = "down",
             distance: int = 24, duration: int = DURATION_NORMAL,
             on_finished: Optional[Callable] = None) -> QPropertyAnimation:
    """位移动画进入：从指定方向滑入（down/up/left/right）"""
    widget.setVisible(True)
    original = widget.pos()

    offsets = {
        "down": QPoint(0, -distance),
        "up": QPoint(0, distance),
        "left": QPoint(distance, 0),
        "right": QPoint(-distance, 0),
    }
    start_pos = original + offsets.get(direction, QPoint(0, -distance))

    widget.move(start_pos)
    anim = QPropertyAnimation(widget, b"pos", widget)
    anim.setDuration(duration)
    anim.setStartValue(start_pos)
    anim.setEndValue(original)
    anim.setEasingCurve(QEasingCurve.Type.OutCubic)
    if on_finished:
        anim.finished.connect(on_finished)
    anim.start(QPropertyAnimation.DeletionPolicy.DeleteWhenStopped)
    return anim


def install_button_hover_animation(btn: QPushButton, scale: float = 1.03,
                                   duration: int = DURATION_FAST) -> None:
    """为按钮安装 hover 轻微缩放动画"""
    btn.setAttribute(Qt.WidgetAttribute.WA_Hover, True)
    original_size: Optional[QSize] = None

    def _enter(_):
        nonlocal original_size
        if original_size is None:
            original_size = btn.size()
        target = QSize(int(original_size.width() * scale),
                       int(original_size.height() * scale))
        anim = QPropertyAnimation(btn, b"size", btn)
        anim.setDuration(duration)
        anim.setStartValue(btn.size())
        anim.setEndValue(target)
        anim.setEasingCurve(QEasingCurve.Type.OutCubic)
        anim.start(QPropertyAnimation.DeletionPolicy.DeleteWhenStopped)

    def _leave(_):
        if original_size is None:
            return
        anim = QPropertyAnimation(btn, b"size", btn)
        anim.setDuration(duration)
        anim.setStartValue(btn.size())
        anim.setEndValue(original_size)
        anim.setEasingCurve(QEasingCurve.Type.OutCubic)
        anim.start(QPropertyAnimation.DeletionPolicy.DeleteWhenStopped)

    btn.installEventFilter(_HoverEventFilter(btn, _enter, _leave))


class _HoverEventFilter(QObject):
    """监听 widget 的 hover 进入/离开事件"""

    def __init__(self, parent: QWidget, on_enter, on_leave):
        super().__init__(parent)
        self.on_enter = on_enter
        self.on_leave = on_leave

    def eventFilter(self, obj, event):
        if event.type() == event.Type.HoverEnter:
            self.on_enter(event)
        elif event.type() == event.Type.HoverLeave:
            self.on_leave(event)
        return super().eventFilter(obj, event)


def install_card_hover_animation(card: QFrame, bg_hover: str = "#FAFAFD",
                                  duration: int = DURATION_FAST) -> None:
    """为卡片安装 hover 背景色渐变动画"""
    card.setAttribute(Qt.WidgetAttribute.WA_Hover, True)
    base_sheet = card.styleSheet()

    def _enter(_):
        anim = QPropertyAnimation(card, b"styleSheet", card)
        anim.setDuration(duration)
        anim.setStartValue(base_sheet)
        anim.setEndValue(f"{base_sheet}; background-color: {bg_hover};")
        anim.setEasingCurve(QEasingCurve.Type.InOutQuad)
        anim.start(QPropertyAnimation.DeletionPolicy.DeleteWhenStopped)

    def _leave(_):
        anim = QPropertyAnimation(card, b"styleSheet", card)
        anim.setDuration(duration)
        anim.setStartValue(card.styleSheet())
        anim.setEndValue(base_sheet)
        anim.setEasingCurve(QEasingCurve.Type.InOutQuad)
        anim.start(QPropertyAnimation.DeletionPolicy.DeleteWhenStopped)

    card.installEventFilter(_HoverEventFilter(card, _enter, _leave))


def animate_geometry(widget: QWidget, target_rect,
                     duration: int = DURATION_NORMAL) -> QPropertyAnimation:
    """几何动画（用于弹窗展开等）"""
    anim = QPropertyAnimation(widget, b"geometry", widget)
    anim.setDuration(duration)
    anim.setStartValue(widget.geometry())
    anim.setEndValue(target_rect)
    anim.setEasingCurve(QEasingCurve.Type.OutCubic)
    anim.start(QPropertyAnimation.DeletionPolicy.DeleteWhenStopped)
    return anim


class HoverRowTracker(QObject):
    """为 QTableView 追踪当前 hover 行，并驱动 model 的 hover 动画

    用法：
        HoverRowTracker(table_view, model)
    model 需要实现 set_hover_row(row: int) 接口。
    """

    def __init__(self, table: QTableView, model, parent=None):
        super().__init__(parent or table)
        self._table = table
        self._model = model
        self._current_row = -1
        table.viewport().installEventFilter(self)
        table.setMouseTracking(True)

    def eventFilter(self, obj, event):
        if obj is not self._table.viewport():
            return super().eventFilter(obj, event)

        et = event.type()
        if et in (event.Type.HoverMove, event.Type.HoverEnter):
            idx = self._table.indexAt(event.position().toPoint())
            row = idx.row()
            if row != self._current_row:
                self._current_row = row
                self._model.set_hover_row(row)
        elif et == event.Type.HoverLeave:
            if self._current_row != -1:
                self._current_row = -1
                self._model.set_hover_row(-1)
        return super().eventFilter(obj, event)


def install_card_scale_animation(card: QFrame, scale: float = 1.015,
                                  duration: int = DURATION_FAST) -> None:
    """为卡片安装 hover 缩放 + 抬升感动画（比背景色渐变更直观）"""
    card.setAttribute(Qt.WidgetAttribute.WA_Hover, True)
    original_size: Optional[QSize] = None
    original_pos: Optional[QPoint] = None

    def _enter(_):
        nonlocal original_size, original_pos
        if original_size is None:
            original_size = card.size()
            original_pos = card.pos()
        target = QSize(int(original_size.width() * scale),
                       int(original_size.height() * scale))
        # 中心放大：向四周扩展
        offset = QPoint((target.width() - original_size.width()) // 2,
                        (target.height() - original_size.height()) // 2)

        size_anim = QPropertyAnimation(card, b"size", card)
        size_anim.setDuration(duration)
        size_anim.setStartValue(card.size())
        size_anim.setEndValue(target)
        size_anim.setEasingCurve(QEasingCurve.Type.OutCubic)
        size_anim.start(QPropertyAnimation.DeletionPolicy.DeleteWhenStopped)

        pos_anim = QPropertyAnimation(card, b"pos", card)
        pos_anim.setDuration(duration)
        pos_anim.setStartValue(card.pos())
        pos_anim.setEndValue(original_pos - offset)
        pos_anim.setEasingCurve(QEasingCurve.Type.OutCubic)
        pos_anim.start(QPropertyAnimation.DeletionPolicy.DeleteWhenStopped)

    def _leave(_):
        if original_size is None or original_pos is None:
            return
        size_anim = QPropertyAnimation(card, b"size", card)
        size_anim.setDuration(duration)
        size_anim.setStartValue(card.size())
        size_anim.setEndValue(original_size)
        size_anim.setEasingCurve(QEasingCurve.Type.OutCubic)
        size_anim.start(QPropertyAnimation.DeletionPolicy.DeleteWhenStopped)

        pos_anim = QPropertyAnimation(card, b"pos", card)
        pos_anim.setDuration(duration)
        pos_anim.setStartValue(card.pos())
        pos_anim.setEndValue(original_pos)
        pos_anim.setEasingCurve(QEasingCurve.Type.OutCubic)
        pos_anim.start(QPropertyAnimation.DeletionPolicy.DeleteWhenStopped)

    card.installEventFilter(_HoverEventFilter(card, _enter, _leave))
