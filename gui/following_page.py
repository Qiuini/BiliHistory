"""
关注列表页面 - 以卡片/头像网格展示已关注 UP 主
"""
import webbrowser
from typing import List

from PyQt6.QtCore import Qt
from PyQt6.QtGui import QPixmap
from PyQt6.QtWidgets import (
    QWidget, QVBoxLayout, QHBoxLayout, QLabel, QGridLayout, QScrollArea,
    QSizePolicy, QFrame
)

from gui import theme
from gui.image_loader import ImageLoader
from gui.animations import install_card_scale_animation
from models import FollowingRecord


class FollowingCard(QFrame):
    """单个 UP 主卡片"""

    def __init__(self, record: FollowingRecord, parent=None):
        super().__init__(parent)
        self.record = record
        self.setObjectName("FollowingCard")
        self.setCursor(Qt.CursorShape.PointingHandCursor)
        self.setFrameShape(QFrame.Shape.NoFrame)
        self.setSizePolicy(QSizePolicy.Policy.Expanding, QSizePolicy.Policy.Fixed)
        self.setMinimumWidth(220)
        self.setFixedHeight(96)

        lay = QHBoxLayout(self)
        lay.setContentsMargins(16, 12, 16, 12)
        lay.setSpacing(14)

        # 头像占位
        self.avatar = QLabel(record.name[:1].upper() if record.name else "?")
        self.avatar.setObjectName("FollowingAvatar")
        self.avatar.setFixedSize(56, 56)
        self.avatar.setAlignment(Qt.AlignmentFlag.AlignCenter)
        self.avatar.setStyleSheet(self._avatar_style(record.name[:1] if record.name else "?"))

        info = QVBoxLayout()
        info.setSpacing(4)
        info.setAlignment(Qt.AlignmentFlag.AlignVCenter)

        name_row = QHBoxLayout()
        name_row.setSpacing(6)
        self.name_label = QLabel(record.name)
        self.name_label.setObjectName("FollowingName")
        self.name_label.setStyleSheet(f"font-size:14px;font-weight:700;color:{theme.TEXT};")
        name_row.addWidget(self.name_label)
        if record.official:
            badge = QLabel("✓")
            badge.setObjectName("FollowingBadge")
            badge.setToolTip(record.official)
            badge.setStyleSheet(f"""
                font-size:10px;
                color:{theme.BLUE};
                background:{theme.SURFACE_PRESSED};
                border-radius:6px;
                padding:0 4px;
            """)
            name_row.addWidget(badge)
        name_row.addStretch(1)
        info.addLayout(name_row)

        sign = record.sign or "这个人很懒，什么都没有写~"
        self.sign_label = QLabel(sign)
        self.sign_label.setObjectName("FollowingSign")
        self.sign_label.setStyleSheet(f"font-size:11px;color:{theme.TEXT_2};")
        self.sign_label.setWordWrap(True)
        self.sign_label.setToolTip(sign)
        info.addWidget(self.sign_label)

        lay.addWidget(self.avatar)
        lay.addLayout(info, stretch=1)

        # 卡片 hover 轻微放大，提升触感
        install_card_scale_animation(self, scale=1.015, duration=120)

    def _avatar_style(self, letter: str) -> str:
        idx = sum(ord(c) for c in self.record.mid) % len(theme.ACCENT_PALETTE)
        color = theme.ACCENT_PALETTE[idx]
        return f"""
            QLabel#FollowingAvatar {{
                background: {color}20;
                color: {color};
                border-radius: 28px;
                font-size:20px;
                font-weight:700;
            }}
        """

    def _set_avatar(self, pixmap: QPixmap):
        self.avatar.setPixmap(pixmap)
        self.avatar.setText("")
        self.avatar.setStyleSheet("border-radius:28px;")

    def mousePressEvent(self, event):
        if event.button() == Qt.MouseButton.LeftButton and self.record.link:
            webbrowser.open(self.record.link)
        super().mousePressEvent(event)


class FollowingPage(QWidget):
    """关注列表主页面"""

    def __init__(self, parent=None):
        super().__init__(parent)
        self.setObjectName("FollowingPage")
        self._records: List[FollowingRecord] = []
        self._image_loader = ImageLoader(parent=self)
        self._build_ui()

    def _build_ui(self):
        root = QVBoxLayout(self)
        root.setContentsMargins(24, 20, 24, 20)
        root.setSpacing(16)

        header = QLabel("我的关注")
        header.setObjectName("FollowingPageTitle")
        header.setStyleSheet(f"font-size:20px;font-weight:800;color:{theme.TEXT};")
        root.addWidget(header)

        self.subtitle = QLabel("加载中...")
        self.subtitle.setObjectName("FollowingPageSubtitle")
        self.subtitle.setStyleSheet(f"font-size:12px;color:{theme.TEXT_3};")
        root.addWidget(self.subtitle)

        # 滚动区
        scroll = QScrollArea()
        scroll.setObjectName("FollowingScroll")
        scroll.setWidgetResizable(True)
        scroll.setFrameShape(QFrame.Shape.NoFrame)
        scroll.setHorizontalScrollBarPolicy(Qt.ScrollBarPolicy.ScrollBarAlwaysOff)
        scroll.setStyleSheet("background:transparent;border:none;")

        self.cards_widget = QWidget()
        self.cards_widget.setStyleSheet("background:transparent;")
        self.grid = QGridLayout(self.cards_widget)
        self.grid.setContentsMargins(0, 0, 0, 0)
        self.grid.setSpacing(12)
        self.grid.setAlignment(Qt.AlignmentFlag.AlignTop | Qt.AlignmentFlag.AlignLeft)

        scroll.setWidget(self.cards_widget)
        root.addWidget(scroll, stretch=1)

    def load_data(self, records: List[FollowingRecord]):
        """加载并渲染关注列表"""
        self._records = records
        self._clear_grid()
        self.subtitle.setText(f"共 {len(records)} 位 UP 主")

        for i, record in enumerate(records):
            card = FollowingCard(record)
            row = i // 3
            col = i % 3
            self.grid.addWidget(card, row, col)

            # 异步加载头像（带缓存）
            if record.face:
                self._image_loader.load(
                    record.face,
                    callback=lambda pixmap, c=card: c._set_avatar(pixmap),
                    size=(56, 56),
                )

    def _clear_grid(self):
        while self.grid.count():
            item = self.grid.takeAt(0)
            if item.widget():
                item.widget().deleteLater()
        self._image_loader.clear_memory()
