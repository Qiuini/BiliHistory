"""临时截图脚本 - 离屏渲染主窗口/弹窗为 PNG（验证后删除）"""
import sys
import os

os.environ.setdefault("QT_QPA_PLATFORM", "offscreen")
os.environ.setdefault("QT_QPA_FONTDIR", r"C:\Windows\Fonts")
root = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, os.path.join(root, "src"))
sys.path.insert(0, root)

from PyQt6.QtCore import Qt
from PyQt6.QtWidgets import QApplication, QPushButton

from gui import theme

app = QApplication([])
app.setStyleSheet(theme.GLOBAL_QSS)

from gui.main_window import MainWindow
from gui.activation_dialog import ActivationDialog
from gui.settings_dialog import CookieSettingsDialog
from gui.dialogs import TrialExhaustedDialog, FetchCompleteDialog, AboutAuthorDialog

out = os.path.join(root, "_shots")
os.makedirs(out, exist_ok=True)

# 截图时使用全新试用记录，确保状态栏显示 30 天
import paths
paths.trial_file().unlink(missing_ok=True)

w = MainWindow()
w.resize(1000, 680)
w.show()

# 塞入演示数据展示 normal 态
demo_rows = [
    {"标题": "【4K修复】洛天依十周年演唱会全程回顾，泪目了家人们",
     "类型": "video", "UP主": "哔哩哔哩音乐", "分类": "音乐",
     "观看时间": "2026-07-27 21:30:15", "BV号": "BV1xx411c7mD",
     "总时长(秒)": 3600, "已观看(秒)": 3600,
     "视频链接": "https://www.bilibili.com/video/BV1xx411c7mD"},
    {"标题": "洛天依新曲 Reaction：这编曲绝了",
     "类型": "video", "UP主": "哔哩哔哩音乐", "分类": "音乐",
     "观看时间": "2026-07-27 19:20:00", "BV号": "BV1zz411c7mF",
     "总时长(秒)": 600, "已观看(秒)": 600,
     "视频链接": "https://www.bilibili.com/video/BV1zz411c7mF"},
    {"标题": "Python 从入门到放弃第42集：装饰器完全指南",
     "类型": "video", "UP主": "码农高天", "分类": "编程",
     "观看时间": "2026-07-27 20:11:02", "BV号": "BV1yy411c7mE",
     "总时长(秒)": 1800, "已观看(秒)": 820,
     "视频链接": "https://www.bilibili.com/video/BV1yy411c7mE"},
    {"标题": "Python 异步编程实战：asyncio 并发抓站",
     "类型": "video", "UP主": "码农高天", "分类": "编程",
     "观看时间": "2026-07-26 15:40:00", "BV号": "BV1aa411c7mG",
     "总时长(秒)": 1200, "已观看(秒)": 1200,
     "视频链接": "https://www.bilibili.com/video/BV1aa411c7mG"},
    {"标题": "深夜电台：一起听歌聊天", "类型": "live", "主播": "某某电台",
     "分类": "电台", "观看时间": "2026-07-26 23:45:00", "房间号": "9021",
     "直播链接": "https://live.bilibili.com/9021"},
    {"标题": "2026年显卡购买指南", "类型": "article", "作者": "硬件茶谈",
     "分类": "数码", "观看时间": "2026-07-25 12:00:00", "文章ID": "123456",
     "专栏链接": "https://www.bilibili.com/read/cv123456"},
    {"标题": "CPU选购避坑指南", "类型": "article", "作者": "硬件茶谈",
     "分类": "数码", "观看时间": "2026-07-24 12:00:00", "文章ID": "123457",
     "专栏链接": "https://www.bilibili.com/read/cv123457"},
]
w.storage._data = demo_rows
w.model.set_rows(demo_rows)
w.model.set_new_ids({"video_BV1xx411c7mD"})
w._show_state("normal")
w._last_fetch_text = "今天 14:35"
w._update_status_counts(len(demo_rows))
w.banner_label.setText(
    f"抓取成功！本次新增 <span style='color:#00B578;font-size:16px;font-weight:700;'>1</span> "
    f"条记录，总计 <span style='color:#00B578;font-size:16px;font-weight:700;'>7</span> 条，已自动备份快照。")
w.banner.setVisible(True)
w.snapshot_label.setText("快照: backups/snapshot_20260727.csv")
w.sep_snapshot.setVisible(True)
w.snapshot_icon.setVisible(True)
w.snapshot_label.setVisible(True)
w.log_status_icon.setText("✓")
w.log_status_label.setText("抓取完成")
w.grab().save(os.path.join(out, "main_normal.png"))

# 切换到数据统计页面截图
w._on_nav_stats()
w.grab().save(os.path.join(out, "main_stats.png"))

# 切换到关注页面截图
w._on_nav_following()
w.grab().save(os.path.join(out, "main_following.png"))

# 切换到收藏夹页面截图
w._on_nav_favorites()
w.grab().save(os.path.join(out, "main_favorites.png"))

# 切回历史页继续后续截图
w._on_nav_history()

# 按 UP主 排序（同名按观看时间倒序）验证表头排序
w.table.sortByColumn(2, Qt.SortOrder.AscendingOrder)
w._sync_log_hint()
w.grab().save(os.path.join(out, "main_sorted_author.png"))

w.banner.setVisible(False)
w.storage._data = []
w.model.set_rows([])
w.model.set_new_ids(set())
w._show_state("empty")
w._update_status_counts(0)
w.sep_snapshot.setVisible(False)
w.snapshot_icon.setVisible(False)
w.snapshot_label.setVisible(False)
w.log_status_icon.setText("")
w.log_status_label.setText("")
w._sync_log_hint()
# debug empty page button
for child in w.page_empty.findChildren(QPushButton):
    print("EMPTY BTN:", repr(child.text()), child.sizeHint(), child.geometry())
w.grab().save(os.path.join(out, "main_empty.png"))

w._start_loading_ui()
w.stat_records.setText("90")
w.stat_pages.setText("3")
w.stat_elapsed.setText("00:42")
w.grab().save(os.path.join(out, "main_loading.png"))

# 取消态
w._show_state("normal")
w._last_fetch_text = "今天 14:35"
w.storage._data = demo_rows
w.model.set_rows(demo_rows)
w.model.set_new_ids(set())
w._update_status_counts(len(demo_rows))

w.banner.setProperty("variant", "warning")
w.banner_icon.setText("⏹")
MainWindow._repolish(w.banner)
w.banner_label.setText(
    "抓取已取消，已保留 <span style='color:#FF9B29;font-size:16px;font-weight:700;'>"
    "28</span> 条记录（本次新增 0 条）")
w.banner.setVisible(True)
w.btn_fetch.setText("⏹ 已取消")
w.btn_fetch.setProperty("cls", "warning")
from PyQt6.QtWidgets import QWidget
MainWindow._repolish(w.btn_fetch)
w.log_status_icon.setText("⏹")
w.log_status_label.setText("已取消")
w._sync_log_hint()
w.grab().save(os.path.join(out, "main_cancelled.png"))

for name, dlg in [
    ("modal_activation", ActivationDialog(w)),
    ("modal_cookie", CookieSettingsDialog(w)),
    ("modal_trial", TrialExhaustedDialog(w)),
    ("modal_complete", FetchCompleteDialog(w, added=12, total=345, pages=8,
                                           snapshot="backups/snapshot_20260727.csv",
                                           elapsed="01:23")),
    ("modal_about_author", AboutAuthorDialog(w)),
]:
    dlg.show()
    dlg.adjustSize()
    dlg.grab().save(os.path.join(out, f"{name}.png"))
    dlg.close()

print("SHOTS OK ->", out)
