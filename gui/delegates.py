"""
表格自绘委托 - 复刻设计稿的类型 tag 胶囊、观看进度迷你条、标题 NEW 徽标、操作按钮
"""
from PyQt6.QtCore import Qt, QRectF, QModelIndex
from PyQt6.QtGui import QColor, QFont, QPainter, QPen
from PyQt6.QtWidgets import QStyledItemDelegate, QStyle, QStyleOptionViewItem

from gui import theme
from gui.table_model import TypeRole, ProgressRole, ProgressTextRole, NewRole, HoverRole


def _paint_row_bg(painter: QPainter, option: QStyleOptionViewItem, index: QModelIndex):
    """统一绘制行背景（hover 粉底 / 新增行淡绿底 / 底部分隔线）"""
    rect = option.rect
    bg = index.data(Qt.ItemDataRole.BackgroundRole)
    selected = bool(option.state & QStyle.StateFlag.State_Selected)
    hover_progress = index.data(HoverRole) or 0.0
    mouse_over = bool(option.state & QStyle.StateFlag.State_MouseOver)

    if selected:
        painter.fillRect(rect, QColor(theme.PINK_LIGHT))
    elif hover_progress > 0 and isinstance(bg, QColor):
        # 使用 model 已经做过颜色过渡计算的 hover 背景
        painter.fillRect(rect, bg)
    elif mouse_over:
        painter.fillRect(rect, QColor(theme.PINK_LIGHT))
    elif isinstance(bg, QColor):
        painter.fillRect(rect, bg)
    else:
        painter.fillRect(rect, QColor(theme.CARD))
    # 底部分隔线
    painter.setPen(QPen(QColor(theme.BORDER_LIGHT), 1))
    painter.drawLine(rect.left(), rect.bottom(), rect.right(), rect.bottom())


class TagDelegate(QStyledItemDelegate):
    """类型列 - 胶囊 tag（3px 10px 11px/600）"""

    def paint(self, painter, option, index):
        painter.save()
        painter.setRenderHint(QPainter.RenderHint.Antialiasing)
        _paint_row_bg(painter, option, index)

        text = index.data(TypeRole) or ""
        if text:
            fg, bg = theme.TAG_COLORS.get(text, theme.TAG_DEFAULT)
            font = QFont(option.font)
            font.setPixelSize(11)
            font.setWeight(QFont.Weight.DemiBold)
            painter.setFont(font)
            fm = painter.fontMetrics()
            w = fm.horizontalAdvance(text) + 20
            h = fm.height() + 6
            pill = QRectF(option.rect.left() + 14,
                          option.rect.center().y() - h / 2, w, h)
            painter.setPen(Qt.PenStyle.NoPen)
            painter.setBrush(QColor(bg))
            painter.drawRoundedRect(pill, h / 2, h / 2)
            painter.setPen(QColor(fg))
            painter.drawText(pill, Qt.AlignmentFlag.AlignCenter, text)
        painter.restore()


class TitleDelegate(QStyledItemDelegate):
    """标题列 - 14px/500 省略号，新增行标题后附粉色 NEW 小徽标"""

    def paint(self, painter, option, index):
        painter.save()
        painter.setRenderHint(QPainter.RenderHint.Antialiasing)
        _paint_row_bg(painter, option, index)

        rect = option.rect.adjusted(14, 0, -10, 0)
        is_new = bool(index.data(NewRole))
        font = QFont(option.font)
        font.setPixelSize(14)
        font.setWeight(QFont.Weight.Medium)
        painter.setFont(font)
        fm = painter.fontMetrics()

        badge_w = 0
        if is_new:
            badge_w = 40  # NEW 徽标预留宽度
        text = index.data(Qt.ItemDataRole.DisplayRole) or ""
        elided = fm.elidedText(text, Qt.TextElideMode.ElideRight,
                               rect.width() - badge_w)
        painter.setPen(QColor(theme.TEXT))
        painter.drawText(rect, Qt.AlignmentFlag.AlignVCenter |
                         Qt.AlignmentFlag.AlignLeft, elided)

        if is_new:
            bf = QFont(option.font)
            bf.setPixelSize(10)
            bf.setWeight(QFont.Weight.Bold)
            painter.setFont(bf)
            bw = painter.fontMetrics().horizontalAdvance("NEW") + 12
            bh = painter.fontMetrics().height() + 4
            bx = rect.left() + fm.horizontalAdvance(elided) + 8
            pill = QRectF(min(bx, rect.right() - bw),
                          rect.center().y() - bh / 2, bw, bh)
            painter.setPen(Qt.PenStyle.NoPen)
            painter.setBrush(QColor(theme.PINK_LIGHT_HOVER))
            painter.drawRoundedRect(pill, bh / 2, bh / 2)
            painter.setPen(QColor(theme.PINK))
            painter.drawText(pill, Qt.AlignmentFlag.AlignCenter, "NEW")
        painter.restore()


class ProgressDelegate(QStyledItemDelegate):
    """观看进度列 - 90×6 迷你渐变条 + 11px 灰字百分比/已看完"""

    def paint(self, painter, option, index):
        painter.save()
        painter.setRenderHint(QPainter.RenderHint.Antialiasing)
        _paint_row_bg(painter, option, index)

        ratio = index.data(ProgressRole)
        text = index.data(ProgressTextRole) or ""
        rect = option.rect

        font = QFont(option.font)
        font.setPixelSize(11)
        painter.setFont(font)

        if ratio is None or ratio < 0:
            painter.setPen(QColor(theme.TEXT_DISABLED))
            painter.drawText(rect.adjusted(14, 0, 0, 0),
                             Qt.AlignmentFlag.AlignVCenter, "—")
            painter.restore()
            return

        bar_w = min(64, rect.width() - 50)
        bar = QRectF(rect.left() + 14, rect.center().y() - 3, bar_w, 6)
        # 底槽
        painter.setPen(Qt.PenStyle.NoPen)
        painter.setBrush(QColor(theme.SURFACE_PRESSED))
        painter.drawRoundedRect(bar, 3, 3)
        # 已看完为绿色，其余粉色
        fill_color = QColor(theme.SUCCESS if ratio >= 0.999 else theme.PINK)
        fill = QRectF(bar.left(), bar.top(), max(4.0, bar.width() * ratio), 6)
        painter.setBrush(fill_color)
        painter.drawRoundedRect(fill, 3, 3)
        # 百分比文字
        painter.setPen(QColor(theme.TEXT_3))
        painter.drawText(QRectF(bar.right() + 6, rect.top(),
                                rect.right() - bar.right() - 6, rect.height()),
                         Qt.AlignmentFlag.AlignVCenter, text)
        painter.restore()


class MonoDelegate(QStyledItemDelegate):
    """BV号列 - 11px 等宽 #B0B0BE"""

    def paint(self, painter, option, index):
        painter.save()
        _paint_row_bg(painter, option, index)
        font = QFont("Consolas")
        font.setPixelSize(11)
        painter.setFont(font)
        painter.setPen(QColor(theme.TEXT_DISABLED))
        painter.drawText(option.rect.adjusted(14, 0, -6, 0),
                         Qt.AlignmentFlag.AlignVCenter, str(
                             index.data(Qt.ItemDataRole.DisplayRole) or ""))
        painter.restore()


class ActionDelegate(QStyledItemDelegate):
    """操作列 - 打开链接图标（hover 蓝色），点击逻辑由视图 clicked 信号处理"""

    def paint(self, painter, option, index):
        painter.save()
        painter.setRenderHint(QPainter.RenderHint.Antialiasing)
        _paint_row_bg(painter, option, index)
        hovering = bool(option.state & QStyle.StateFlag.State_MouseOver)
        painter.setPen(QColor(theme.BLUE if hovering else theme.TEXT_3))
        font = QFont(option.font)
        font.setPixelSize(13)
        painter.setFont(font)
        painter.drawText(option.rect, Qt.AlignmentFlag.AlignCenter, "↗")
        painter.restore()


class PlainDelegate(QStyledItemDelegate):
    """普通文本列（UP主/观看时间）- 统一行背景与分隔线"""

    def paint(self, painter, option, index):
        painter.save()
        _paint_row_bg(painter, option, index)
        fg = index.data(Qt.ItemDataRole.ForegroundRole)
        painter.setPen(fg if isinstance(fg, QColor) else QColor(theme.TEXT_2))
        font = QFont(option.font)
        font.setPixelSize(13)
        is_new = bool(index.data(NewRole))
        if is_new and isinstance(fg, QColor) and fg == QColor(theme.SUCCESS):
            font.setWeight(QFont.Weight.DemiBold)
        painter.setFont(font)
        fm = painter.fontMetrics()
        rect = option.rect.adjusted(14, 0, -6, 0)
        text = fm.elidedText(str(index.data(Qt.ItemDataRole.DisplayRole) or ""),
                             Qt.TextElideMode.ElideRight, rect.width())
        painter.drawText(rect, Qt.AlignmentFlag.AlignVCenter, text)
        painter.restore()


class CategoryDelegate(QStyledItemDelegate):
    """分类列 - 11px 灰字小标签，匹配设计稿"""

    def paint(self, painter, option, index):
        painter.save()
        painter.setRenderHint(QPainter.RenderHint.Antialiasing)
        _paint_row_bg(painter, option, index)

        text = str(index.data(Qt.ItemDataRole.DisplayRole) or "")
        font = QFont(option.font)
        font.setPixelSize(11)
        painter.setFont(font)
        painter.setPen(QColor(theme.TEXT_3))
        painter.drawText(option.rect.adjusted(14, 0, -6, 0),
                         Qt.AlignmentFlag.AlignVCenter, text)
        painter.restore()
