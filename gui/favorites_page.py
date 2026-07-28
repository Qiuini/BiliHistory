"""
收藏夹页面 - 树形/分组展示收藏文件夹及其内容
"""
import webbrowser
from typing import List, Dict

from PyQt6.QtCore import Qt
from PyQt6.QtWidgets import (
    QWidget, QVBoxLayout, QHBoxLayout, QLabel, QTreeWidget, QTreeWidgetItem,
    QSizePolicy, QFrame
)

from gui import theme
from models import FavFolderRecord, FavResourceRecord


class FavoritesPage(QWidget):
    """收藏夹主页面"""

    def __init__(self, parent=None):
        super().__init__(parent)
        self.setObjectName("FavoritesPage")
        self._folders: List[FavFolderRecord] = []
        self._resources: Dict[str, List[FavResourceRecord]] = {}
        self._build_ui()

    def _build_ui(self):
        root = QVBoxLayout(self)
        root.setContentsMargins(24, 20, 24, 20)
        root.setSpacing(16)

        header = QLabel("我的收藏")
        header.setObjectName("FavoritesPageTitle")
        header.setStyleSheet(f"font-size:20px;font-weight:800;color:{theme.TEXT};")
        root.addWidget(header)

        self.subtitle = QLabel("加载中...")
        self.subtitle.setObjectName("FavoritesPageSubtitle")
        self.subtitle.setStyleSheet(f"font-size:12px;color:{theme.TEXT_3};")
        root.addWidget(self.subtitle)

        # 树形控件
        self.tree = QTreeWidget()
        self.tree.setObjectName("FavoritesTree")
        self.tree.setHeaderHidden(True)
        self.tree.setColumnCount(2)
        self.tree.setColumnWidth(0, 360)
        self.tree.setColumnWidth(1, 160)
        self.tree.setIndentation(20)
        self.tree.setSizePolicy(QSizePolicy.Policy.Expanding, QSizePolicy.Policy.Expanding)
        self.tree.setFrameShape(QFrame.Shape.NoFrame)
        self.tree.setStyleSheet(f"""
            QTreeWidget#FavoritesTree {{
                background: {theme.CARD};
                border: 1px solid {theme.BORDER};
                border-radius: 12px;
                outline: none;
            }}
            QTreeWidget::item {{
                padding: 10px 16px;
                border-bottom: 1px solid {theme.BORDER_LIGHT};
            }}
            QTreeWidget::item:selected {{
                background: {theme.PINK_LIGHT};
                color: {theme.TEXT};
            }}
            QTreeWidget::item:hover {{
                background: {theme.SURFACE_HOVER};
            }}
        """)
        self.tree.itemDoubleClicked.connect(self._on_item_double_clicked)
        self.tree.itemExpanded.connect(self._on_item_expanded)
        root.addWidget(self.tree, stretch=1)

    def load_data(self, data):
        """加载收藏夹数据

        Args:
            data: (folders, resources) 元组
        """
        self._folders, self._resources = data
        self.subtitle.setText(f"共 {len(self._folders)} 个收藏夹，{sum(len(v) for v in self._resources.values())} 条内容")
        self._render_tree()

    def _render_tree(self):
        self.tree.clear()
        for folder in self._folders:
            folder_item = QTreeWidgetItem(self.tree)
            folder_item.setText(0, f"📁 {folder.title}")
            folder_item.setText(1, f"{folder.media_count} 条")
            folder_item.setData(0, Qt.ItemDataRole.UserRole, {"type": "folder", "link": folder.link})
            folder_item.setFlags(folder_item.flags() | Qt.ItemFlag.ItemIsAutoTristate)
            folder_item.setExpanded(True)

            items = self._resources.get(folder.folder_id, [])
            for resource in items:
                child = QTreeWidgetItem(folder_item)
                child.setText(0, f"  {resource.title}")
                child.setText(1, resource.author or "")
                child.setData(0, Qt.ItemDataRole.UserRole,
                              {"type": "resource", "link": resource.link, "bvid": resource.bvid})

    def _on_item_double_clicked(self, item: QTreeWidgetItem, column: int):
        data = item.data(0, Qt.ItemDataRole.UserRole)
        if isinstance(data, dict) and data.get("link"):
            webbrowser.open(data["link"])

    def _on_item_expanded(self, item: QTreeWidgetItem):
        data = item.data(0, Qt.ItemDataRole.UserRole)
        if isinstance(data, dict) and data.get("type") == "folder":
            item.setText(0, item.text(0).replace("📁", "📂"))
