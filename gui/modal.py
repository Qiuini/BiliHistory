"""
弹窗基类 - 复刻设计稿弹窗骨架: header(图标+标题+关闭钮) / body(24px) / footer(#FAFAFD)

所有业务弹窗（Cookie设置/会员激活/结果反馈等）继承 ModalDialog，
调用 add_header / body_layout / add_footer 组装内容。
"""
from PyQt6.QtCore import Qt, QTimer
from PyQt6.QtWidgets import (
    QDialog, QVBoxLayout, QHBoxLayout, QLabel, QPushButton, QWidget,
    QFrame, QScrollArea
)

from gui.animations import fade_in, slide_in


class ModalDialog(QDialog):
    """设计稿风格弹窗骨架"""

    def __init__(self, parent=None, width: int = 480):
        super().__init__(parent)
        self.setModal(True)
        self.setFixedWidth(width)
        self._root = QVBoxLayout(self)
        self._root.setContentsMargins(0, 0, 0, 0)
        self._root.setSpacing(0)
        self._body_widget: QWidget | None = None
        self._shown = False

    def showEvent(self, event):
        super().showEvent(event)
        if self._shown:
            return
        self._shown = True
        # 首次显示时做淡入 + 轻微上滑，提升弹窗出现质感
        QTimer.singleShot(0, lambda: fade_in(self, duration=160))
        QTimer.singleShot(0, lambda: slide_in(self, direction="up", distance=16, duration=180))

    # ---------------- header ----------------
    def add_header(self, title: str, subtitle: str = "",
                   icon: str = "", icon_style: str = "pink"):
        """头部: 渐变小图标 + 标题(副题) + 关闭钮"""
        header = QWidget()
        header.setObjectName("ModalHeader")
        lay = QHBoxLayout(header)
        lay.setContentsMargins(24, 18, 16, 16)
        lay.setSpacing(12)

        if icon:
            icon_label = QLabel(icon)
            icon_name = {"pink": "ModalHeadIcon",
                         "green": "ModalHeadIconGreen",
                         "red": "ModalHeadIconRed"}.get(icon_style, "ModalHeadIcon")
            icon_label.setObjectName(icon_name)
            icon_label.setFixedSize(34, 34)
            icon_label.setAlignment(Qt.AlignmentFlag.AlignCenter)
            lay.addWidget(icon_label)

        text_col = QVBoxLayout()
        text_col.setSpacing(2)
        title_label = QLabel(title)
        title_label.setObjectName("ModalTitle")
        text_col.addWidget(title_label)
        if subtitle:
            sub_label = QLabel(subtitle)
            sub_label.setObjectName("ModalSub")
            text_col.addWidget(sub_label)
        lay.addLayout(text_col, stretch=1)

        close_btn = QPushButton("✕")
        close_btn.setObjectName("CloseBtn")
        close_btn.setFixedSize(32, 32)
        close_btn.setCursor(Qt.CursorShape.PointingHandCursor)
        close_btn.clicked.connect(self.reject)
        lay.addWidget(close_btn, alignment=Qt.AlignmentFlag.AlignTop)

        self._root.addWidget(header)

    # ---------------- body ----------------
    def body_layout(self, scrollable: bool = False,
                    max_height: int = 480) -> QVBoxLayout:
        """创建 body 容器（padding 24），返回其布局供子类填充"""
        self._body_widget = QWidget()
        lay = QVBoxLayout(self._body_widget)
        lay.setContentsMargins(24, 20, 24, 20)
        lay.setSpacing(14)

        if scrollable:
            scroll = QScrollArea()
            scroll.setWidgetResizable(True)
            scroll.setFrameShape(QFrame.Shape.NoFrame)
            scroll.setMaximumHeight(max_height)
            scroll.setWidget(self._body_widget)
            scroll.setStyleSheet("QScrollArea{background:transparent;}")
            self._body_widget.setStyleSheet("background:transparent;")
            self._root.addWidget(scroll, stretch=1)
        else:
            self._root.addWidget(self._body_widget, stretch=1)
        return lay

    # ---------------- footer ----------------
    def add_footer(self, buttons: list[QPushButton],
                   align: str = "right") -> None:
        """底部按钮区。align: right / between / center"""
        footer = QWidget()
        footer.setObjectName("ModalFooter")
        lay = QHBoxLayout(footer)
        lay.setContentsMargins(24, 14, 24, 14)
        lay.setSpacing(10)

        if align == "between" and len(buttons) >= 2:
            lay.addWidget(buttons[0])
            lay.addStretch(1)
            for b in buttons[1:]:
                lay.addWidget(b)
        elif align == "center":
            lay.addStretch(1)
            for b in buttons:
                lay.addWidget(b)
            lay.addStretch(1)
        else:
            lay.addStretch(1)
            for b in buttons:
                lay.addWidget(b)

        for b in buttons:
            b.setCursor(Qt.CursorShape.PointingHandCursor)
        self._root.addWidget(footer)


def make_button(text: str, cls: str = "secondary", size: str = "") -> QPushButton:
    """创建带设计系统样式类的按钮"""
    btn = QPushButton(text)
    btn.setProperty("cls", cls)
    if size:
        btn.setProperty("size", size)
    return btn


def make_alert(text: str, kind: str = "info") -> QLabel:
    """创建 info/error/success 提示条"""
    label = QLabel(text)
    label.setProperty("alert", kind)
    label.setWordWrap(True)
    return label
