"""
主窗口 - 按 BiliHistory-UI 设计稿复刻:
左侧边栏(216px) + 工具栏 + 内容区(空态引导/表格卡片/抓取进度卡) + 状态栏 + 日志折叠条

四个界面状态: empty(无数据引导) / normal(数据浏览) / loading(抓取中) / complete(抓取完成)
业务链路沿用: FetchWorker 抓取线程、licensing 授权门禁、trial 试用计数、日志桥接。
"""
import logging
import os
import re
import webbrowser
from datetime import datetime, timedelta

from PyQt6.QtCore import Qt, QTimer, QPoint, QSize, QSignalBlocker, QThread, pyqtSignal
from PyQt6.QtGui import QColor, QFont, QPainter, QPen
from PyQt6.QtWidgets import (
    QMainWindow, QWidget, QVBoxLayout, QHBoxLayout, QPushButton,
    QComboBox, QLineEdit, QLabel, QTableView, QPlainTextEdit,
    QProgressBar, QHeaderView, QStackedWidget, QFrame, QSizePolicy,
    QApplication, QButtonGroup
)

# 轻量图标映射（无 Lucide 依赖，使用系统常见符号/emoji 近似）
_ICONS = {
    "play": "▶", "history": "↺", "bookmark": "🔖", "clock": "🕐",
    "chart": "📊", "database": "🗄", "settings": "⚙", "crown": "👑",
    "user": "👤", "cloud": "☁", "search": "🔍", "filter": "▽",
    "calendar": "📅", "sort": "⇅", "refresh": "↻", "refresh_cw": "↻",
    "upload": "↑", "download": "↓", "download_cloud": "☁↓",
    "more": "⋮", "more_vertical": "⋮", "stop": "■", "stop_circle": "⏹",
    "check": "✓", "check_circle": "✓", "check_circle_2": "✓",
    "link": "↗", "external_link": "↗", "terminal": "⌘",
    "chevron_down": "▼", "chevron_right": "›", "chevron_up": "▲",
    "help": "?", "minimize": "─", "maximize": "□",
    "restore": "❐", "close": "✕", "file_check": "✓",
    "spinner": "◐",
}

from config import get_config
from storage import HistoryStorage
from models import ContentType
from social_fetcher import UserInfoFetcher

from gui import theme
from gui.table_model import HistoryTableModel, LinkRole
from gui.delegates import (
    TagDelegate, TitleDelegate, ProgressDelegate, MonoDelegate,
    ActionDelegate, PlainDelegate, CategoryDelegate
)
from gui.stats_page import StatsPage
from gui.following_page import FollowingPage
from gui.favorites_page import FavoritesPage
from gui.workers import FetchWorker
from gui.settings_dialog import CookieSettingsDialog
from gui.activation_dialog import ActivationDialog
from gui.dialogs import TrialExhaustedDialog, FetchCompleteDialog, FetchErrorDialog
from gui.log_bridge import install_qt_log_handler

from licensing import license_manager as lm
from licensing import trial

# 界面筛选下拉项 -> 类型英文值（与 to_dict 中存储的 '类型' 一致）
_FILTER_MAP = {
    "全部类型": None,
    "视频": ContentType.VIDEO.value,
    "直播": ContentType.LIVE.value,
    "专栏": ContentType.ARTICLE.value,
}
_TIME_OPTIONS = ["全部时间", "今天", "本周", "本月"]
_SORT_OPTIONS = ["最新优先", "最早优先", "排序方式"]

# fetcher 日志中的翻页进度行
_PAGE_RE = re.compile(r"第 (\d+) 页获取 (\d+) 条记录（累计 (\d+) 条）")


class _RegTimeWorker(QThread):
    """后台查询注册时间"""
    ready = pyqtSignal(str)

    def __init__(self, mid: str, parent=None):
        super().__init__(parent)
        self.mid = mid

    def run(self):
        try:
            fetcher = UserInfoFetcher()
            dt = fetcher.fetch_registration_time(self.mid)
            fetcher.close()
            if dt:
                self.ready.emit(dt.strftime("%Y-%m-%d"))
            else:
                self.ready.emit("")
        except Exception:
            self.ready.emit("")


class MainWindow(QMainWindow):
    """B站历史记录管理主窗口"""

    def __init__(self):
        super().__init__()
        self.setWindowTitle("BiliHistory - B站历史记录管理工具")
        self.resize(1000, 680)
        self.setMinimumSize(760, 520)
        # Win11 Fluent 风格无边框窗口（设计稿含自定义标题栏）
        self.setWindowFlags(Qt.WindowType.FramelessWindowHint | Qt.WindowType.Window)
        self.setAttribute(Qt.WidgetAttribute.WA_TranslucentBackground, False)

        self.config = get_config()
        self.storage = HistoryStorage(self.config.csv_file)
        self.model = HistoryTableModel()
        self.worker: FetchWorker | None = None

        self._log_count = 0
        self._fetch_pages = 0
        self._fetch_records = 0
        self._fetch_started_at: datetime | None = None
        self._last_fetch_text = ""
        self._ids_before_fetch: set[str] = set()
        self._drag_pos: QPoint | None = None
        self._is_maximized = False

        self._elapsed_timer = QTimer(self)
        self._elapsed_timer.setInterval(1000)
        self._elapsed_timer.timeout.connect(self._tick_elapsed)

        self._spinner_timer = QTimer(self)
        self._spinner_timer.setInterval(120)
        self._spinner_chars = ["◐", "◓", "◑", "◒"]
        self._spinner_idx = 0
        self._spinner_timer.timeout.connect(self._tick_spinner)

        self._build_ui()
        self._install_log_bridge()

        # 启动即尝试加载已有总表
        self._load_master()
        self._refresh_license_status()
        self._load_registration_time()

    def _load_registration_time(self):
        """从 Cookie 中解析 DedeUserID 并后台查询注册时间"""
        mid = self._extract_mid_from_cookie()
        if mid:
            self._reg_worker = _RegTimeWorker(mid, self)
            self._reg_worker.ready.connect(self._on_reg_time_ready)
            self._reg_worker.start()
        else:
            self.reg_time_label.setText("注册时间 --")

    def _extract_mid_from_cookie(self) -> str:
        """从 Cookie 字符串中提取 DedeUserID"""
        cookie = self.config.cookie or ""
        match = re.search(r"DedeUserID=(\d+)", cookie)
        return match.group(1) if match else ""

    def _on_reg_time_ready(self, date_str: str):
        if date_str:
            self.reg_time_label.setText(f"注册于 {date_str}")
        else:
            self.reg_time_label.setText("注册时间 --")

    # ================= UI 骨架 =================
    def _build_ui(self):
        central = QWidget()
        central.setObjectName("CentralArea")
        self.setCentralWidget(central)
        root = QVBoxLayout(central)
        root.setContentsMargins(0, 0, 0, 0)
        root.setSpacing(0)

        root.addWidget(self._build_titlebar())

        body = QHBoxLayout()
        body.setContentsMargins(0, 0, 0, 0)
        body.setSpacing(0)
        body.addWidget(self._build_sidebar())
        body.addWidget(self._build_content(), stretch=1)
        root.addLayout(body, stretch=1)

    # ---------------- 标题栏 ----------------
    def _build_titlebar(self) -> QWidget:
        bar = QWidget()
        bar.setObjectName("TitleBar")
        bar.setFixedHeight(36)
        lay = QHBoxLayout(bar)
        lay.setContentsMargins(16, 0, 0, 0)
        lay.setSpacing(12)

        icon = QLabel("B")
        icon.setObjectName("TitleBarIcon")
        icon.setFixedSize(22, 22)
        icon.setAlignment(Qt.AlignmentFlag.AlignCenter)
        title = QLabel("BiliHistory - B站历史记录管理工具")
        title.setObjectName("TitleBarTitle")
        lay.addWidget(icon)
        lay.addWidget(title)
        lay.addStretch(1)

        controls = QHBoxLayout()
        controls.setSpacing(0)
        controls.setContentsMargins(0, 0, 0, 0)
        for symbol, name, handler in [
            ("─", "Minimize", self.showMinimized),
            ("□", "Maximize", self._toggle_maximized),
            ("✕", "Close", self.close),
        ]:
            btn = QPushButton(symbol)
            btn.setObjectName("TitleBarBtn" if name != "Close" else "TitleBarBtnClose")
            btn.setProperty("titlebar", "true")
            btn.setFixedSize(46, 36)
            btn.setCursor(Qt.CursorShape.PointingHandCursor)
            btn.clicked.connect(handler)
            controls.addWidget(btn)
        lay.addLayout(controls)
        return bar

    def _toggle_maximized(self):
        if self.isMaximized():
            self.showNormal()
        else:
            self.showMaximized()

    def _resize_edge_at(self, pos: QPoint) -> Qt.Edge:
        """根据鼠标位置判断当前处于窗口哪条边/角，用于无边框窗口缩放"""
        margin = 8
        x, y = pos.x(), pos.y()
        w, h = self.width(), self.height()

        on_left = x < margin
        on_right = x >= w - margin
        on_top = y < margin
        on_bottom = y >= h - margin

        if on_top and on_left:
            return Qt.Edge.TopEdge | Qt.Edge.LeftEdge
        if on_top and on_right:
            return Qt.Edge.TopEdge | Qt.Edge.RightEdge
        if on_bottom and on_left:
            return Qt.Edge.BottomEdge | Qt.Edge.LeftEdge
        if on_bottom and on_right:
            return Qt.Edge.BottomEdge | Qt.Edge.RightEdge
        if on_top:
            return Qt.Edge.TopEdge
        if on_bottom:
            return Qt.Edge.BottomEdge
        if on_left:
            return Qt.Edge.LeftEdge
        if on_right:
            return Qt.Edge.RightEdge
        return Qt.Edge(0)

    def _cursor_for_edge(self, edge: Qt.Edge):
        if edge == (Qt.Edge.TopEdge | Qt.Edge.LeftEdge) or edge == (Qt.Edge.BottomEdge | Qt.Edge.RightEdge):
            return Qt.CursorShape.SizeFDiagCursor
        if edge == (Qt.Edge.TopEdge | Qt.Edge.RightEdge) or edge == (Qt.Edge.BottomEdge | Qt.Edge.LeftEdge):
            return Qt.CursorShape.SizeBDiagCursor
        if edge & Qt.Edge.TopEdge or edge & Qt.Edge.BottomEdge:
            return Qt.CursorShape.SizeVerCursor
        if edge & Qt.Edge.LeftEdge or edge & Qt.Edge.RightEdge:
            return Qt.CursorShape.SizeHorCursor
        return Qt.CursorShape.ArrowCursor

    def mousePressEvent(self, event):
        if event.button() == Qt.MouseButton.LeftButton:
            edge = self._resize_edge_at(event.position().toPoint())
            if edge and self.windowHandle() and not self.isMaximized():
                self.windowHandle().startSystemResize(edge)
                return
            # 仅在标题栏区域触发拖动
            if event.position().y() <= 36:
                self._drag_pos = event.globalPosition().toPoint()
        super().mousePressEvent(event)

    def mouseMoveEvent(self, event):
        if self._drag_pos is not None and event.buttons() == Qt.MouseButton.LeftButton:
            if not self.isMaximized():
                self.move(self.pos() + event.globalPosition().toPoint() - self._drag_pos)
            self._drag_pos = event.globalPosition().toPoint()
        else:
            edge = self._resize_edge_at(event.position().toPoint())
            self.setCursor(self._cursor_for_edge(edge))
        super().mouseMoveEvent(event)

    def mouseReleaseEvent(self, event):
        self._drag_pos = None
        super().mouseReleaseEvent(event)

    def _build_nav_item(self, icon_key: str, text: str, enabled: bool,
                        handler, badge: int, checkable: bool = False,
                        checked: bool = False) -> QPushButton:
        """构建带图标、文本、徽标的侧边栏导航项"""
        btn = QPushButton()
        btn.setObjectName("NavBtn")
        btn.setEnabled(enabled)
        btn.setCursor(Qt.CursorShape.PointingHandCursor)
        btn.setCheckable(checkable)
        if checked:
            btn.setChecked(True)
        if handler:
            btn.clicked.connect(handler)
        if not enabled:
            btn.setToolTip("即将推出")

        lay = QHBoxLayout(btn)
        lay.setContentsMargins(16, 0, 12, 0)
        lay.setSpacing(10)

        icon = QLabel(_ICONS.get(icon_key, "•"))
        icon.setObjectName("NavIcon")
        icon.setFixedSize(18, 18)
        icon.setAlignment(Qt.AlignmentFlag.AlignCenter)
        lay.addWidget(icon)

        label = QLabel(text)
        label.setObjectName("NavText")
        lay.addWidget(label, stretch=1)

        if badge > 0:
            badge_lbl = QLabel(str(badge))
            badge_lbl.setObjectName("NavBadge")
            badge_lbl.setAlignment(Qt.AlignmentFlag.AlignCenter)
            lay.addWidget(badge_lbl)
        return btn

    # ---------------- 侧边栏 ----------------
    def _build_sidebar(self) -> QWidget:
        sidebar = QWidget()
        sidebar.setObjectName("Sidebar")
        sidebar.setFixedWidth(216)
        lay = QVBoxLayout(sidebar)
        lay.setContentsMargins(12, 16, 12, 14)
        lay.setSpacing(4)

        # 品牌区
        brand = QHBoxLayout()
        brand.setSpacing(10)
        logo = QLabel("B")
        logo.setObjectName("BrandIcon")
        logo.setFixedSize(36, 36)
        logo.setAlignment(Qt.AlignmentFlag.AlignCenter)
        title = QLabel("BiliHistory")
        title.setObjectName("BrandTitle")
        brand.addWidget(logo)
        brand.addWidget(title)
        brand.addStretch(1)
        lay.addLayout(brand)
        lay.addSpacing(16)

        # 导航（未实现的功能置灰禁用，避免虚假入口）
        self._nav_group = QButtonGroup(self)
        self._nav_group.setExclusive(True)
        nav_items = [
            ("history", "观看历史", True, self._on_nav_history, 0, True, True),
            ("bookmark", "我的关注", True, self._on_nav_following, 0, True, False),
            ("clock", "我的收藏", True, self._on_nav_favorites, 0, True, False),
            ("chart", "数据统计", True, self._on_nav_stats, 0, True, False),
            ("database", "备份管理", False, None, 0, False, False),
            ("settings", "Cookie设置", True, self._on_settings, 0, False, False),
            ("crown", "会员中心", True, self._on_activate_dialog, 0, False, False),
        ]
        for item_args in nav_items:
            icon_key, text, enabled, handler, badge = item_args[:5]
            checkable = item_args[5] if len(item_args) > 5 else False
            checked = item_args[6] if len(item_args) > 6 else False
            item = self._build_nav_item(icon_key, text, enabled, handler, badge,
                                        checkable, checked)
            if checkable:
                self._nav_group.addButton(item)
            lay.addWidget(item)

        lay.addStretch(1)

        # 用户卡
        card = QWidget()
        card.setObjectName("UserCard")
        card_lay = QHBoxLayout(card)
        card_lay.setContentsMargins(12, 10, 12, 10)
        card_lay.setSpacing(10)
        avatar = QLabel("B")
        avatar.setObjectName("UserAvatar")
        avatar.setFixedSize(32, 32)
        avatar.setAlignment(Qt.AlignmentFlag.AlignCenter)
        card_lay.addWidget(avatar)
        user_col = QVBoxLayout()
        user_col.setSpacing(0)
        name = QLabel("本地用户")
        name.setObjectName("UserName")
        self.plan_label = QLabel("免费版")
        self.plan_label.setObjectName("UserPlan")
        self.reg_time_label = QLabel("注册时间 --")
        self.reg_time_label.setObjectName("UserRegTime")
        user_col.addWidget(name)
        user_col.addWidget(self.plan_label)
        user_col.addWidget(self.reg_time_label)
        card_lay.addLayout(user_col, stretch=1)
        lay.addWidget(card)

        version = QLabel("BiliHistory v1.0")
        version.setObjectName("VersionLabel")
        version.setAlignment(Qt.AlignmentFlag.AlignCenter)
        lay.addWidget(version)
        return sidebar

    # ---------------- 内容列 ----------------
    def _build_content(self) -> QWidget:
        content = QWidget()
        col = QVBoxLayout(content)
        col.setContentsMargins(0, 0, 0, 0)
        col.setSpacing(0)

        col.addWidget(self._build_toolbar())

        # 主区：空态 / 表格 / 进度卡 / 数据统计 四页切换
        self.stack = QStackedWidget()
        self.stack.setStyleSheet(f"background:{theme.BG};")
        self.page_table = self._build_table_page()
        self.page_empty = self._build_empty_page()
        self.page_loading = self._build_loading_page()
        self.page_stats = StatsPage()
        self.page_following = FollowingPage()
        self.page_favorites = FavoritesPage()
        self.stack.addWidget(self.page_empty)
        self.stack.addWidget(self.page_table)
        self.stack.addWidget(self.page_loading)
        self.stack.addWidget(self.page_stats)
        self.stack.addWidget(self.page_following)
        self.stack.addWidget(self.page_favorites)
        col.addWidget(self.stack, stretch=1)

        col.addWidget(self._build_status_bar())
        col.addWidget(self._build_log_area())
        return content

    def _build_toolbar(self) -> QWidget:
        self.toolbar = QWidget()
        self.toolbar.setObjectName("Toolbar")
        lay = QHBoxLayout(self.toolbar)
        lay.setContentsMargins(24, 14, 24, 14)
        lay.setSpacing(12)

        # 左侧：主 CTA + 搜索 + 筛选下拉
        left = QHBoxLayout()
        left.setSpacing(12)

        self.btn_fetch = QPushButton(f"{_ICONS['download_cloud']} 一键抓取历史")
        self.btn_fetch.setProperty("cls", "primary")
        self.btn_fetch.setProperty("size", "lg")
        self.btn_fetch.setCursor(Qt.CursorShape.PointingHandCursor)
        left.addWidget(self.btn_fetch)

        # 搜索框（带内嵌图标）
        search_wrap = QWidget()
        search_wrap.setObjectName("SearchBox")
        search_lay = QHBoxLayout(search_wrap)
        search_lay.setContentsMargins(12, 0, 14, 0)
        search_lay.setSpacing(6)
        search_icon = QLabel(_ICONS["search"])
        search_icon.setObjectName("SearchBoxIcon")
        search_icon.setFixedSize(16, 16)
        search_icon.setAlignment(Qt.AlignmentFlag.AlignCenter)
        self.search_edit = QLineEdit()
        self.search_edit.setObjectName("SearchEditInner")
        self.search_edit.setPlaceholderText("搜索视频标题、UP主、BV号...")
        search_lay.addWidget(search_icon)
        search_lay.addWidget(self.search_edit, stretch=1)
        search_wrap.setMinimumSize(140, 36)
        search_wrap.setSizePolicy(QSizePolicy.Policy.Expanding, QSizePolicy.Policy.Fixed)
        left.addWidget(search_wrap)

        self.type_combo = self._build_select("filter", list(_FILTER_MAP.keys())[0],
                                             list(_FILTER_MAP.keys()))
        self.time_combo = self._build_select("calendar", _TIME_OPTIONS[0], _TIME_OPTIONS)
        self.sort_combo = self._build_select("sort", _SORT_OPTIONS[0], _SORT_OPTIONS)
        self.sort_combo.combo.currentIndexChanged.disconnect(self._refresh_view)
        self.sort_combo.combo.currentIndexChanged.connect(self._on_sort_changed)
        left.addWidget(self.type_combo)
        left.addWidget(self.time_combo)
        left.addWidget(self.sort_combo)

        lay.addLayout(left)
        lay.addStretch(1)

        # 右侧：图标按钮组（刷新 / 导入 / 导出 / 更多）
        right = QHBoxLayout()
        right.setSpacing(8)
        self.btn_refresh = self._build_icon_btn("refresh_cw", "刷新", self._load_master)
        self.btn_import = self._build_icon_btn("upload", "导入", self._on_placeholder)
        self.btn_export = self._build_icon_btn("download", "导出", self._on_placeholder)
        self.btn_more = self._build_icon_btn("more_vertical", "更多", self._on_dedup)
        right.addWidget(self.btn_refresh)
        right.addWidget(self.btn_import)
        right.addWidget(self.btn_export)
        right.addWidget(self.btn_more)
        lay.addLayout(right)

        # 事件绑定（下拉框内部 QComboBox 已连接）
        self.btn_fetch.clicked.connect(self._on_fetch)
        self.search_edit.textChanged.connect(self._refresh_view)
        return self.toolbar

    def _build_select(self, icon_key: str, initial: str, options: list[str]) -> QWidget:
        """设计稿风格的带图标下拉选择框"""
        wrap = QWidget()
        wrap.setObjectName("SelectBox")
        wrap.setMinimumSize(96, 36)
        wrap.setSizePolicy(QSizePolicy.Policy.Expanding, QSizePolicy.Policy.Fixed)
        lay = QHBoxLayout(wrap)
        lay.setContentsMargins(10, 0, 6, 0)
        lay.setSpacing(6)

        icon = QLabel(_ICONS.get(icon_key, ""))
        icon.setObjectName("SelectIcon")
        icon.setFixedSize(16, 16)
        icon.setAlignment(Qt.AlignmentFlag.AlignCenter)

        combo = QComboBox()
        combo.setObjectName("SelectCombo")
        combo.addItems(options)
        combo.setCurrentText(initial)
        combo.currentIndexChanged.connect(self._refresh_view)

        lay.addWidget(icon)
        lay.addWidget(combo, stretch=1)
        wrap.combo = combo  # 便于业务层读取当前选项
        return wrap

    def _build_icon_btn(self, icon_key: str, tooltip: str, handler) -> QPushButton:
        btn = QPushButton(_ICONS.get(icon_key, "•"))
        btn.setObjectName("IconBtn")
        btn.setToolTip(tooltip)
        btn.setCursor(Qt.CursorShape.PointingHandCursor)
        btn.setFixedSize(36, 36)
        if handler:
            btn.clicked.connect(handler)
        return btn

    def _on_placeholder(self):
        """占位按钮：记录即将推出的提示"""
        self._append_log("该功能即将推出，敬请期待", logging.INFO)

    def _build_table_page(self) -> QWidget:
        page = QWidget()
        lay = QVBoxLayout(page)
        lay.setContentsMargins(24, 16, 24, 16)
        lay.setSpacing(12)

        # 完成横幅（默认隐藏）
        self.banner = QWidget()
        self.banner.setObjectName("SuccessBanner")
        banner_lay = QHBoxLayout(self.banner)
        banner_lay.setContentsMargins(24, 14, 24, 14)
        banner_lay.setSpacing(12)
        self.banner_icon = QLabel(_ICONS["check_circle"])
        self.banner_icon.setObjectName("SuccessBannerIcon")
        self.banner_icon.setFixedSize(20, 20)
        self.banner_icon.setAlignment(Qt.AlignmentFlag.AlignCenter)
        self.banner_label = QLabel("")
        self.banner_label.setObjectName("SuccessBannerText")
        self.banner_label.setWordWrap(True)
        banner_close = QPushButton(_ICONS["close"])
        banner_close.setObjectName("SuccessBannerClose")
        banner_close.setFixedSize(24, 24)
        banner_close.setCursor(Qt.CursorShape.PointingHandCursor)
        banner_close.clicked.connect(lambda: self.banner.setVisible(False))
        banner_lay.addWidget(self.banner_icon)
        banner_lay.addWidget(self.banner_label, stretch=1)
        banner_lay.addWidget(banner_close)
        self.banner.setVisible(False)
        lay.addWidget(self.banner)

        # 表格白卡
        card = QWidget()
        card.setObjectName("TableCard")
        card_lay = QVBoxLayout(card)
        card_lay.setContentsMargins(1, 1, 1, 1)

        self.table = QTableView()
        self.table.setModel(self.model)
        self.table.setSelectionBehavior(QTableView.SelectionBehavior.SelectRows)
        self.table.setEditTriggers(QTableView.EditTrigger.NoEditTriggers)
        self.table.setShowGrid(False)
        self.table.setMouseTracking(True)
        self.table.verticalHeader().setVisible(False)
        self.table.verticalHeader().setDefaultSectionSize(44)
        self.table.setFrameShape(QFrame.Shape.NoFrame)

        # 列委托与列宽（类型/标题/UP主/进度/分类/时间/BV号/操作）
        self.table.setItemDelegateForColumn(0, TagDelegate(self.table))
        self.table.setItemDelegateForColumn(1, TitleDelegate(self.table))
        self.table.setItemDelegateForColumn(2, PlainDelegate(self.table))
        self.table.setItemDelegateForColumn(3, ProgressDelegate(self.table))
        self.table.setItemDelegateForColumn(4, CategoryDelegate(self.table))
        self.table.setItemDelegateForColumn(5, PlainDelegate(self.table))
        self.table.setItemDelegateForColumn(6, MonoDelegate(self.table))
        self.table.setItemDelegateForColumn(7, ActionDelegate(self.table))
        header = self.table.horizontalHeader()
        widths = [90, 0, 130, 120, 80, 120, 100, 50]
        for i, w in enumerate(widths):
            if w:
                self.table.setColumnWidth(i, w)
        header.setSectionResizeMode(1, QHeaderView.ResizeMode.Stretch)
        header.setHighlightSections(False)
        header.setSortIndicatorShown(True)
        self.table.setSortingEnabled(True)
        self.table.sortByColumn(5, Qt.SortOrder.DescendingOrder)
        header.sortIndicatorChanged.connect(self._on_header_sort_changed)
        self.table.clicked.connect(self._on_table_clicked)
        self.table.doubleClicked.connect(self._open_row_link)

        card_lay.addWidget(self.table)
        lay.addWidget(card, stretch=1)
        return page

    def _build_empty_page(self) -> QWidget:
        page = QWidget()
        lay = QVBoxLayout(page)
        lay.setContentsMargins(24, 16, 24, 16)
        lay.addStretch(3)

        circle = QLabel("📦")
        circle.setObjectName("EmptyIconCircle")
        circle.setFixedSize(120, 120)
        circle.setAlignment(Qt.AlignmentFlag.AlignCenter)
        lay.addWidget(circle, alignment=Qt.AlignmentFlag.AlignHCenter)
        lay.addSpacing(20)

        title = QLabel("还没有观看历史记录")
        title.setObjectName("EmptyTitle")
        title.setAlignment(Qt.AlignmentFlag.AlignCenter)
        lay.addWidget(title)
        lay.addSpacing(8)

        desc = QLabel("点击下方按钮一键抓取你的B站观看历史，"
                      "开始管理和囤积你的视频回忆吧~")
        desc.setObjectName("EmptyDesc")
        desc.setAlignment(Qt.AlignmentFlag.AlignCenter)
        lay.addWidget(desc)
        lay.addSpacing(24)

        # 三步 chips
        steps_row = QHBoxLayout()
        steps_row.addStretch(1)
        for i, text in enumerate(["设置Cookie", "一键抓取", "本地管理"], start=1):
            if i > 1:
                arrow = QLabel("›")
                arrow.setObjectName("StepArrow")
                steps_row.addWidget(arrow)
            chip = QWidget()
            chip.setObjectName("StepChip")
            chip_lay = QHBoxLayout(chip)
            chip_lay.setContentsMargins(12, 10, 18, 10)
            chip_lay.setSpacing(10)
            num = QLabel(str(i))
            num.setObjectName("StepNum")
            num.setFixedSize(24, 24)
            num.setAlignment(Qt.AlignmentFlag.AlignCenter)
            label = QLabel(text)
            label.setObjectName("StepText")
            chip_lay.addWidget(num)
            chip_lay.addWidget(label)
            steps_row.addWidget(chip)
        steps_row.addStretch(1)
        lay.addLayout(steps_row)
        lay.addSpacing(28)

        cta = QPushButton("立即抓取历史记录")
        cta.setProperty("cls", "primary")
        cta.setCursor(Qt.CursorShape.PointingHandCursor)
        cta.setFixedSize(200, 44)
        cta.clicked.connect(self._on_fetch)
        lay.addWidget(cta, alignment=Qt.AlignmentFlag.AlignCenter)
        lay.addSpacing(12)

        link = QPushButton("如何获取Cookie？")
        link.setObjectName("LinkBtn")
        link.setCursor(Qt.CursorShape.PointingHandCursor)
        link.clicked.connect(self._on_settings)
        lay.addWidget(link, alignment=Qt.AlignmentFlag.AlignHCenter)

        lay.addStretch(4)
        return page

    def _build_loading_page(self) -> QWidget:
        page = QWidget()
        outer = QVBoxLayout(page)
        outer.addStretch(2)

        card = QWidget()
        card.setObjectName("ProgressCard")
        card.setFixedWidth(460)
        lay = QVBoxLayout(card)
        lay.setContentsMargins(40, 32, 40, 32)
        lay.setSpacing(12)

        title_row = QHBoxLayout()
        title_row.addStretch(1)
        self.spinner = QLabel("◐")
        self.spinner.setObjectName("ProgressSpinner")
        self.spinner.setFixedSize(22, 22)
        self.spinner.setAlignment(Qt.AlignmentFlag.AlignCenter)
        title = QLabel("正在抓取观看历史")
        title.setObjectName("ProgressTitle")
        title_row.addWidget(self.spinner)
        title_row.addWidget(title)
        title_row.addStretch(1)
        lay.addLayout(title_row)

        hint = QLabel("请耐心等待，抓取过程中请勿关闭窗口或断开网络")
        hint.setObjectName("ProgressHint")
        hint.setAlignment(Qt.AlignmentFlag.AlignCenter)
        hint.setWordWrap(True)
        lay.addWidget(hint)
        lay.addSpacing(8)

        self.main_progress_label = QLabel("总进度")
        self.main_progress_label.setObjectName("ProgressBarLabel")
        self.main_progress_value = QLabel("抓取中...")
        self.main_progress_value.setObjectName("ProgressBarValue")
        prog_label_row = QHBoxLayout()
        prog_label_row.addWidget(self.main_progress_label)
        prog_label_row.addStretch(1)
        prog_label_row.addWidget(self.main_progress_value)
        lay.addLayout(prog_label_row)

        self.main_progress = QProgressBar()
        self.main_progress.setObjectName("MainProgress")
        self.main_progress.setRange(0, 0)  # 游标分页无法预知总量，忙碌态
        self.main_progress.setTextVisible(False)
        lay.addWidget(self.main_progress)
        lay.addSpacing(10)

        # 三列统计（虚线上边框）
        divider = QFrame()
        divider.setObjectName("StatDivider")
        divider.setFixedHeight(2)
        lay.addWidget(divider)

        stats_row = QHBoxLayout()
        stats_row.setSpacing(8)
        self.stat_records = QLabel("0")
        self.stat_records.setObjectName("StatValue")
        self.stat_pages = QLabel("0")
        self.stat_pages.setObjectName("StatValuePlain")
        self.stat_elapsed = QLabel("00:00")
        self.stat_elapsed.setObjectName("StatValuePlain")
        for value_label, text in [(self.stat_records, "已抓取记录"),
                                  (self.stat_pages, "已抓取页数"),
                                  (self.stat_elapsed, "已用时间")]:
            cell = QVBoxLayout()
            cell.setSpacing(2)
            value_label.setAlignment(Qt.AlignmentFlag.AlignCenter)
            label = QLabel(text)
            label.setObjectName("StatLabel")
            label.setAlignment(Qt.AlignmentFlag.AlignCenter)
            cell.addWidget(value_label)
            cell.addWidget(label)
            stats_row.addLayout(cell, stretch=1)
        lay.addLayout(stats_row)
        lay.addSpacing(8)

        # 当前页进度
        page_row = QHBoxLayout()
        page_label = QLabel("当前页进度")
        page_label.setObjectName("StatLabel")
        self.page_indicator = QLabel("第 1 页")
        self.page_indicator.setObjectName("StatLabel")
        page_row.addWidget(page_label)
        page_row.addStretch(1)
        page_row.addWidget(self.page_indicator)
        lay.addLayout(page_row)

        self.page_progress = QProgressBar()
        self.page_progress.setObjectName("PageProgress")
        self.page_progress.setRange(0, 0)
        self.page_progress.setTextVisible(False)
        lay.addWidget(self.page_progress)

        outer.addWidget(card, alignment=Qt.AlignmentFlag.AlignHCenter)
        outer.addStretch(3)
        return page

    def _build_status_bar(self) -> QWidget:
        bar = QWidget()
        bar.setObjectName("StatusBar")
        bar.setFixedHeight(32)
        lay = QHBoxLayout(bar)
        lay.setContentsMargins(24, 0, 24, 0)
        lay.setSpacing(0)

        # 左侧状态组
        left = QHBoxLayout()
        left.setSpacing(12)
        left.setAlignment(Qt.AlignmentFlag.AlignLeft | Qt.AlignmentFlag.AlignVCenter)

        self.status_dot = QLabel()
        self.status_dot.setObjectName("StatusDot")
        self.status_dot.setFixedSize(8, 8)
        self.count_label = QLabel("暂无数据")
        self.count_label.setObjectName("StatusStrong")
        count_row = QHBoxLayout()
        count_row.setSpacing(8)
        count_row.addWidget(self.status_dot)
        count_row.addWidget(self.count_label)
        left.addLayout(count_row)

        self.sep_trial = self._build_status_sep()
        self.trial_label = QLabel("")
        self.trial_label.setObjectName("TrialLabel")
        left.addWidget(self.sep_trial)
        left.addWidget(self.trial_label)

        self.last_fetch_label = QLabel("")
        self.last_fetch_label.setObjectName("StatusLight")
        left.addWidget(self.last_fetch_label)

        self.sep_snapshot = self._build_status_sep()
        self.sep_snapshot.setVisible(False)
        self.snapshot_label = QLabel("")
        self.snapshot_label.setObjectName("SnapshotLabel")
        self.snapshot_label.setVisible(False)
        self.snapshot_icon = QLabel(_ICONS["file_check"])
        self.snapshot_icon.setObjectName("SnapshotIcon")
        self.snapshot_icon.setVisible(False)
        snapshot_row = QHBoxLayout()
        snapshot_row.setSpacing(6)
        snapshot_row.addWidget(self.snapshot_icon)
        snapshot_row.addWidget(self.snapshot_label)
        left.addWidget(self.sep_snapshot)
        left.addLayout(snapshot_row)

        # 右侧信息组
        right = QHBoxLayout()
        right.setSpacing(12)
        right.setAlignment(Qt.AlignmentFlag.AlignRight | Qt.AlignmentFlag.AlignVCenter)

        self.db_label = QLabel("")
        self.db_label.setObjectName("StatusDb")
        self.plan_badge = QLabel("")
        self.plan_badge.setObjectName("PlanBadge")
        right.addWidget(self.db_label)
        right.addWidget(self.plan_badge)

        lay.addLayout(left)
        lay.addStretch(1)
        lay.addLayout(right)
        return bar

    @staticmethod
    def _build_status_sep() -> QFrame:
        sep = QFrame()
        sep.setObjectName("StatusSep")
        sep.setFrameShape(QFrame.Shape.VLine)
        sep.setFixedSize(1, 14)
        return sep

    def _build_log_area(self) -> QWidget:
        wrap = QWidget()
        col = QVBoxLayout(wrap)
        col.setContentsMargins(0, 0, 0, 0)
        col.setSpacing(0)

        # 折叠条
        self.log_bar = QPushButton()
        self.log_bar.setObjectName("LogBar")
        self.log_bar.setFixedHeight(30)
        self.log_bar.setCursor(Qt.CursorShape.PointingHandCursor)
        bar_lay = QHBoxLayout(self.log_bar)
        bar_lay.setContentsMargins(24, 0, 24, 0)
        bar_lay.setSpacing(8)

        self.log_chevron = QLabel(_ICONS["chevron_down"])
        self.log_chevron.setObjectName("LogBarChevron")
        self.log_chevron.setFixedSize(14, 14)
        self.log_chevron.setAlignment(Qt.AlignmentFlag.AlignCenter)
        self.log_count_label = QLabel("日志 (0)")
        self.log_count_label.setObjectName("LogBarTitle")
        left = QHBoxLayout()
        left.setSpacing(8)
        left.addWidget(self.log_chevron)
        left.addWidget(self.log_count_label)

        self.log_status_label = QLabel("")
        self.log_status_label.setObjectName("LogBarStatus")
        self.log_status_icon = QLabel("")
        self.log_status_icon.setObjectName("LogBarStatusIcon")
        self.log_status_icon.setFixedSize(12, 12)
        self.log_status_icon.setAlignment(Qt.AlignmentFlag.AlignCenter)
        status_row = QHBoxLayout()
        status_row.setSpacing(6)
        status_row.addWidget(self.log_status_icon)
        status_row.addWidget(self.log_status_label)

        self.log_hint_label = QLabel("点击展开")
        self.log_hint_label.setObjectName("LogBarHint")

        bar_lay.addLayout(left)
        bar_lay.addStretch(1)
        bar_lay.addWidget(self.log_hint_label)
        bar_lay.addLayout(status_row)
        self.log_bar.clicked.connect(self._toggle_log)
        col.addWidget(self.log_bar)

        # 日志面板（默认折叠）
        self.log_view = QPlainTextEdit()
        self.log_view.setObjectName("LogPanel")
        self.log_view.setReadOnly(True)
        self.log_view.setMaximumBlockCount(2000)
        self.log_view.setFixedHeight(150)
        self.log_view.setVisible(False)
        self._sync_log_hint()
        col.addWidget(self.log_view)
        return wrap

    def _install_log_bridge(self):
        """挂载日志桥接，把 logger 输出显示到日志面板"""
        handler = install_qt_log_handler(logging.INFO)
        handler.emitter.message.connect(self._append_log)

    # ================= 导航切换 =================
    def _on_nav_history(self):
        """切换到观看历史"""
        self.toolbar.setVisible(True)
        self._show_state("normal" if self.storage.count else "empty")

    def _on_nav_following(self):
        """切换到关注列表"""
        self.toolbar.setVisible(False)
        if not self.page_following._records:
            self.page_following.load_data(self._demo_followings())
        self.stack.setCurrentWidget(self.page_following)
        self._spinner_timer.stop()

    def _on_nav_favorites(self):
        """切换到收藏夹"""
        self.toolbar.setVisible(False)
        if not self.page_favorites._folders:
            self.page_favorites.load_data(self._demo_favorites())
        self.stack.setCurrentWidget(self.page_favorites)
        self._spinner_timer.stop()

    def _on_nav_stats(self):
        """切换到数据统计"""
        self.toolbar.setVisible(False)
        self.page_stats.load_data(list(self.storage.records))
        self.stack.setCurrentWidget(self.page_stats)
        self._spinner_timer.stop()

    def _demo_followings(self):
        from models import FollowingRecord
        return [
            FollowingRecord(mid="208259", name="哔哩哔哩弹幕网", sign="B站官方账号", official="bilibili官方账号", level=6, face=""),
            FollowingRecord(mid="327280337", name="老师好我叫何同学", sign="科技区 UP 主", official="bilibili 2022百大UP主", level=6, face=""),
            FollowingRecord(mid="508963009", name="硬件茶谈", sign="电脑硬件科普", level=6, face=""),
            FollowingRecord(mid="163637592", name="罗翔说刑法", sign="中国政法大学刑事司法学院教授", official="bilibili 2022百大UP主", level=6, face=""),
            FollowingRecord(mid="25876945", name="局座张召忠", sign=" retired ", level=6, face=""),
            FollowingRecord(mid="8047632", name="精致的男孩富贵", sign="生活区 UP 主", level=5, face=""),
        ]

    def _demo_favorites(self):
        from models import FavFolderRecord, FavResourceRecord
        folders = [
            FavFolderRecord(folder_id="1", title="默认收藏夹", media_count=3),
            FavFolderRecord(folder_id="2", title="技术教程", media_count=2),
        ]
        resources = {
            "1": [
                FavResourceRecord(title="2026 显卡购买指南", bvid="BV1xx411c7mD", author="硬件茶谈"),
                FavResourceRecord(title="Python 异步编程实战", bvid="BV1yy411c7mE", author="码农高天"),
                FavResourceRecord(title="罗翔：我们为什么要读书", bvid="BV1zz411c7mF", author="罗翔说刑法"),
            ],
            "2": [
                FavResourceRecord(title="PyQt6 完整教程", bvid="BV1aa411c7mG", author="码农高天"),
                FavResourceRecord(title="LLM 原理入门", bvid="BV1bb411c7mH", author="李沐"),
            ],
        }
        return folders, resources

    # ================= 状态切换 =================
    def _show_state(self, state: str):
        """empty / normal / loading 三页切换（complete 复用 normal 页 + 横幅）"""
        if state == "loading":
            self.stack.setCurrentWidget(self.page_loading)
            self._spinner_timer.start()
        elif state == "empty":
            self.stack.setCurrentWidget(self.page_empty)
            self._spinner_timer.stop()
        else:
            self.stack.setCurrentWidget(self.page_table)
            self._spinner_timer.stop()

        # 空态/抓取中禁用搜索筛选
        interactive = state == "normal"
        for w in (self.search_edit, self.type_combo,
                  self.time_combo, self.sort_combo):
            w.setEnabled(interactive)
            if isinstance(w, QWidget) and hasattr(w, "combo"):
                w.combo.setEnabled(interactive)
        if state == "empty":
            self.search_edit.setPlaceholderText("还没有数据，先抓取吧~")
        else:
            self.search_edit.setPlaceholderText("搜索视频标题、UP主、BV号...")

    # ================= 数据加载与展示 =================
    def _load_master(self):
        """从磁盘加载总表并刷新表格"""
        self.storage.load()
        self._refresh_view()

    def _refresh_view(self):
        """按类型/时间筛选 + 搜索关键字 + 排序，刷新表格（非破坏性）"""
        rows = list(self.storage.records)

        type_value = _FILTER_MAP.get(self.type_combo.combo.currentText())
        if type_value:
            rows = [r for r in rows if r.get("类型") == type_value]

        rows = self._filter_by_time(rows, self.time_combo.combo.currentText())

        keyword = self.search_edit.text().strip().lower()
        if keyword:
            def match(r):
                title = (r.get("标题") or "").lower()
                author = (r.get("UP主") or r.get("主播") or r.get("作者") or "").lower()
                rid = (r.get("BV号") or "").lower()
                return keyword in title or keyword in author or keyword in rid
            rows = [r for r in rows if match(r)]

        header = self.table.horizontalHeader()
        sort_col = header.sortIndicatorSection()
        sort_order = header.sortIndicatorOrder()

        self.model.set_rows(rows)
        if sort_col >= 0:
            self.table.sortByColumn(sort_col, sort_order)

        self._update_status_counts(len(rows))

        # 抓取中不切换主区页面
        if not (self.worker and self.worker.isRunning()):
            self._show_state("normal" if self.storage.count else "empty")

    def _on_sort_changed(self):
        """排序下拉切换：同步到表头时间列排序"""
        text = self.sort_combo.combo.currentText()
        if text == "排序方式":
            return
        order = (Qt.SortOrder.DescendingOrder if text == "最新优先"
                 else Qt.SortOrder.AscendingOrder)
        self.table.sortByColumn(5, order)

    def _on_header_sort_changed(self, section: int, order: Qt.SortOrder):
        """表头点击排序后，同步排序下拉框文案"""
        with QSignalBlocker(self.sort_combo.combo):
            if section == 5:
                text = "最新优先" if order == Qt.SortOrder.DescendingOrder else "最早优先"
                self.sort_combo.combo.setCurrentText(text)
            else:
                self.sort_combo.combo.setCurrentText("排序方式")

    @staticmethod
    def _filter_by_time(rows: list, option: str) -> list:
        if option == "全部时间":
            return rows
        now = datetime.now()
        if option == "今天":
            start = now.replace(hour=0, minute=0, second=0, microsecond=0)
        elif option == "本周":
            start = (now - timedelta(days=now.weekday())).replace(
                hour=0, minute=0, second=0, microsecond=0)
        else:  # 本月
            start = now.replace(day=1, hour=0, minute=0,
                                second=0, microsecond=0)
        threshold = start.strftime("%Y-%m-%d %H:%M:%S")
        return [r for r in rows if (r.get("观看时间") or "") >= threshold]

    def _update_status_counts(self, shown: int):
        total = self.storage.count
        added = len(self.model._new_ids) if total else 0
        if total == 0:
            self.count_label.setText("暂无数据")
            self._set_status_dot("gray")
            self.last_fetch_label.setVisible(False)
        else:
            text = f"共 <span style='color:{theme.TEXT};font-weight:700;'>{total}</span> 条记录"
            if added > 0:
                text += f" <span style='color:{theme.SUCCESS};font-weight:600;'>(+{added} 新增)</span>"
            elif shown != total:
                text += f"（筛选后 {shown} 条）"
            self.count_label.setText(text)
            self._set_status_dot("success")
            self.last_fetch_label.setVisible(True)
        self.last_fetch_label.setText(
            f"最近抓取: {self._last_fetch_text}" if self._last_fetch_text else "")
        self._update_db_label()

    def _update_db_label(self):
        path = self.config.csv_file
        name = os.path.basename(path) if path else "未配置"
        size_text = ""
        try:
            if path and os.path.isfile(path):
                size_mb = os.path.getsize(path) / (1024 * 1024)
                size_text = f" · {size_mb:.1f}MB"
        except Exception:
            pass
        self.db_label.setText(f"数据库: {name}{size_text}")

    @staticmethod
    def _format_fetch_time(when: datetime) -> str:
        now = datetime.now()
        if when.date() == now.date():
            return f"今天 {when.strftime('%H:%M')}"
        return when.strftime('%m-%d %H:%M')

    def _set_status_dot(self, state: str):
        """状态指示点：success / warn / error / gray"""
        colors = {
            "success": (theme.SUCCESS, "0 0 6px rgba(0,181,120,0.4)"),
            "warn": (theme.WARN, "0 0 6px rgba(255,155,41,0.4)"),
            "error": (theme.DANGER, "0 0 6px rgba(240,72,56,0.4)"),
            "gray": (theme.TEXT_DISABLED, "none"),
        }
        color, shadow = colors.get(state, colors["gray"])
        self.status_dot.setStyleSheet(
            f"background:{color};border-radius:4px;box-shadow:{shadow};")

    # ================= 动作 =================
    def _on_settings(self):
        dlg = CookieSettingsDialog(self)
        if dlg.exec():
            # 保存后 save_cookie 已刷新全局配置单例，这里同步本地引用
            self.config = get_config()

    def _on_activate_dialog(self):
        dlg = ActivationDialog(self)
        dlg.exec()
        self._refresh_license_status()

    def _refresh_license_status(self):
        """刷新侧边栏用户卡 + 状态栏授权显示"""
        info = lm.current_license()
        if info is not None:
            plan = "永久会员" if info.is_buyout else "月度会员"
            self.plan_label.setText(plan)
            self.plan_badge.setText(f"{_ICONS['crown']} {plan}")
            self.trial_label.setVisible(False)
            self.sep_trial.setVisible(False)
        else:
            left = trial.remaining_days()
            if left > 0:
                self.plan_label.setText(f"免费试用中 · 剩余 {left} 天")
                self.plan_badge.setText(
                    f"<span style='color:{theme.GOLD};'>{_ICONS['crown']}</span> "
                    f"<span style='color:{theme.TEXT_2};font-weight:500;'>免费试用</span>")
                self.trial_label.setText(
                    f"剩余试用: <span style='color:{theme.PINK};font-weight:700;'>{left}天</span>")
            else:
                self.plan_label.setText("免费试用已到期")
                self.plan_badge.setText(
                    f"<span style='color:{theme.TEXT_DISABLED};'>{_ICONS['crown']}</span> "
                    f"<span style='color:{theme.TEXT_DISABLED};font-weight:500;'>未激活</span>")
                self.trial_label.setText(
                    f"剩余试用: <span style='color:{theme.TEXT_DISABLED};font-weight:700;'>已到期</span>")
            self.trial_label.setVisible(True)
            self.sep_trial.setVisible(True)

    def _on_fetch(self):
        # 抓取中再次点击 -> 取消抓取
        if self.worker and self.worker.isRunning():
            self.worker.cancel()
            return

        # ---- 授权门禁：付费授权放行；否则消耗试用；试用耗尽引导激活 ----
        if not lm.is_licensed() and not trial.is_active():
            dlg = TrialExhaustedDialog(self)
            if dlg.exec():
                self._on_activate_dialog()
            return

        # 记录抓取前的 ID 集合，用于标记 NEW 行
        self._ids_before_fetch = {
            self.model._record_key(r) for r in self.storage.records}
        self._start_loading_ui()

        self.worker = FetchWorker()
        self.worker.progress.connect(
            lambda msg: self._append_log(msg, logging.INFO))
        self.worker.finished_ok.connect(self._on_fetch_done)
        self.worker.cancelled.connect(self._on_fetch_cancelled)
        self.worker.failed.connect(self._on_fetch_failed)
        self.worker.start()

    def _start_loading_ui(self):
        self._fetch_pages = 0
        self._fetch_records = 0
        self._fetch_started_at = datetime.now()
        self.stat_records.setText("0")
        self.stat_pages.setText("0")
        self.stat_elapsed.setText("00:00")
        self.page_indicator.setText("第 1 页")
        self._elapsed_timer.start()

        self.btn_fetch.setEnabled(True)
        self.btn_fetch.setText(f"{_ICONS['stop_circle']} 停止抓取")
        self.btn_fetch.setProperty("cls", "destructive")
        self._repolish(self.btn_fetch)
        self.btn_refresh.setEnabled(False)
        self.btn_import.setEnabled(False)
        self.btn_export.setEnabled(False)
        self.btn_more.setEnabled(False)
        self.banner.setVisible(False)
        self._show_state("loading")

        self.count_label.setText("正在抓取中...")
        self._set_status_dot("warn")
        self.log_status_icon.setText(_ICONS["spinner"])
        self.log_status_label.setText("抓取中...")
        self._sync_log_hint()
        if not self.log_view.isVisible():
            self._toggle_log()

    def _finish_loading_ui(self):
        self._elapsed_timer.stop()
        self.btn_fetch.setEnabled(True)
        self.btn_refresh.setEnabled(True)
        self.btn_import.setEnabled(True)
        self.btn_export.setEnabled(True)
        self.btn_more.setEnabled(True)

    def _elapsed_text(self) -> str:
        if not self._fetch_started_at:
            return "00:00"
        sec = int((datetime.now() - self._fetch_started_at).total_seconds())
        return f"{sec // 60:02d}:{sec % 60:02d}"

    def _tick_elapsed(self):
        self.stat_elapsed.setText(self._elapsed_text())

    def _tick_spinner(self):
        if hasattr(self, "spinner"):
            self._spinner_idx = (self._spinner_idx + 1) % len(self._spinner_chars)
            self.spinner.setText(self._spinner_chars[self._spinner_idx])

    def _on_fetch_done(self, added: int, total: int, snapshot: str):
        elapsed = self._elapsed_text()
        self._finish_loading_ui()
        self._refresh_license_status()
        self._last_fetch_text = self._format_fetch_time(datetime.now())

        # 标记本次新增行（complete 态 NEW 徽标 + 淡绿底）
        self.storage.load()
        new_ids = {self.model._record_key(r) for r in self.storage.records
                   } - self._ids_before_fetch
        self.model.set_new_ids(new_ids)
        self._refresh_view()

        # complete 态：绿色按钮 + 成功横幅 + 快照信息
        self.btn_fetch.setText(f"{_ICONS['check_circle']} 抓取完成！")
        self.btn_fetch.setProperty("cls", "success")
        self._repolish(self.btn_fetch)
        QTimer.singleShot(4000, self._reset_fetch_button)

        self.banner.setProperty("variant", "success")
        self.banner_icon.setText(_ICONS["check_circle"])
        self._repolish(self.banner)
        self.banner_label.setText(
            f"抓取成功！本次新增 <span style='color:{theme.SUCCESS};font-size:16px;font-weight:700;'>"
            f"{added}</span> 条记录，"
            f"总计 <span style='color:{theme.SUCCESS};font-size:16px;font-weight:700;'>"
            f"{total}</span> 条，已自动备份快照。")
        self.banner.setVisible(True)
        if snapshot:
            self.snapshot_label.setText(f"快照: {snapshot}")
            self.sep_snapshot.setVisible(True)
            self.snapshot_icon.setVisible(True)
            self.snapshot_label.setVisible(True)
        else:
            self.sep_snapshot.setVisible(False)
            self.snapshot_icon.setVisible(False)
            self.snapshot_label.setVisible(False)
        self.log_status_icon.setText(_ICONS["check_circle_2"])
        self.log_status_label.setText("抓取完成")
        self._sync_log_hint()

        dlg = FetchCompleteDialog(self, added=added, total=total,
                                  pages=self._fetch_pages,
                                  snapshot=snapshot, elapsed=elapsed)
        dlg.exec()

    def _on_fetch_cancelled(self, added: int, total: int, snapshot: str):
        elapsed = self._elapsed_text()
        self._finish_loading_ui()
        self._refresh_license_status()
        self._last_fetch_text = self._format_fetch_time(datetime.now())

        # 标记本次新增行
        self.storage.load()
        new_ids = {self.model._record_key(r) for r in self.storage.records
                   } - self._ids_before_fetch
        self.model.set_new_ids(new_ids)
        self._refresh_view()

        # 取消态：黄色警告按钮 + 提示横幅
        self.btn_fetch.setText(f"{_ICONS['stop_circle']} 已取消")
        self.btn_fetch.setProperty("cls", "warning")
        self._repolish(self.btn_fetch)
        QTimer.singleShot(3000, self._reset_fetch_button)

        self.banner.setProperty("variant", "warning")
        self.banner_icon.setText(_ICONS["stop_circle"])
        self._repolish(self.banner)
        self.banner_label.setText(
            f"抓取已取消，已保留 <span style='color:{theme.WARN};font-size:16px;font-weight:700;'>"
            f"{total}</span> 条记录（本次新增 {added} 条）")
        self.banner.setVisible(True)
        if snapshot:
            self.snapshot_label.setText(f"快照: {snapshot}")
            self.sep_snapshot.setVisible(True)
            self.snapshot_icon.setVisible(True)
            self.snapshot_label.setVisible(True)
        else:
            self.sep_snapshot.setVisible(False)
            self.snapshot_icon.setVisible(False)
            self.snapshot_label.setVisible(False)
        self.log_status_icon.setText(_ICONS["stop_circle"])
        self.log_status_label.setText("已取消")
        self._sync_log_hint()

    def _reset_fetch_button(self):
        self.btn_fetch.setText(f"{_ICONS['download_cloud']} 一键抓取历史")
        self.btn_fetch.setProperty("cls", "primary")
        self._repolish(self.btn_fetch)

    def _on_fetch_failed(self, error: str):
        self._finish_loading_ui()
        self._refresh_view()
        self.log_status_icon.setText("")
        self.log_status_label.setText("")
        self._sync_log_hint()
        cookie_expired = "Cookie" in error or "cookie" in error
        dlg = FetchErrorDialog(self, error=error, cookie_expired=cookie_expired)
        if dlg.exec():
            if cookie_expired:
                self._on_settings()
            else:
                self._on_fetch()

    def _on_dedup(self):
        removed = self.storage.remove_duplicates_by_id()
        self.storage.save()
        self._refresh_view()
        self._append_log(f"去重完成，移除了 {removed} 条重复记录", logging.INFO)

    def _on_table_clicked(self, index):
        # 操作列点击 -> 打开链接
        if index.column() == len(HistoryTableModel.COLUMNS) - 1:
            self._open_row_link(index)

    def _open_row_link(self, index):
        link = index.data(LinkRole)
        if link:
            webbrowser.open(link)

    # ================= 辅助 =================
    @staticmethod
    def _repolish(widget):
        """动态属性变化后强制重刷 QSS"""
        widget.style().unpolish(widget)
        widget.style().polish(widget)

    def _append_log(self, text: str, level: int = logging.INFO):
        self._log_count += 1
        self.log_count_label.setText(f"日志 ({self._log_count})")

        # 解析翻页进度行，驱动 loading 卡统计
        m = _PAGE_RE.search(text)
        if m:
            self._fetch_pages = int(m.group(1))
            self._fetch_records = int(m.group(3))
            self.stat_pages.setText(str(self._fetch_pages))
            self.stat_records.setText(str(self._fetch_records))
            self.page_indicator.setText(f"第 {self._fetch_pages + 1} 页")

        # 设计稿日志着色：时间绿 / INFO蓝 / WARN黄 / ERROR红
        if level >= logging.ERROR:
            color = theme.LOG_ERROR
        elif level >= logging.WARNING:
            color = theme.LOG_WARN
        else:
            color = "#C8C8D8"
        parts = text.split(" ", 1)
        if len(parts) == 2 and ":" in parts[0]:
            html = (f'<span style="color:{theme.LOG_TIME}">{parts[0]}</span> '
                    f'<span style="color:{color}">{parts[1]}</span>')
        else:
            html = f'<span style="color:{color}">{text}</span>'
        self.log_view.appendHtml(html)

    def _toggle_log(self):
        visible = not self.log_view.isVisible()
        self.log_view.setVisible(visible)
        self.log_chevron.setText(_ICONS["chevron_up"] if visible else _ICONS["chevron_down"])
        self._sync_log_hint()

    def _sync_log_hint(self):
        has_status = bool(self.log_status_label.text())
        self.log_hint_label.setVisible(
            not self.log_view.isVisible() and not has_status)

    def closeEvent(self, event):
        # 等待抓取线程安全退出
        if self.worker and self.worker.isRunning():
            self.worker.wait(3000)
        super().closeEvent(event)
