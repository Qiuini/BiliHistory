"""
表格模型 - 将 HistoryStorage 的记录（异构中文字段的 dict）统一展示到 QTableView

列结构按设计稿: 类型(tag) / 标题 / UP主 / 观看进度(迷你条) / 观看时间 / BV号 / 操作
自定义角色向委托暴露: 类型中文、进度比例、链接、NEW 标记。
"""
from datetime import datetime
from typing import List, Set

from PyQt6.QtCore import QAbstractTableModel, Qt, QModelIndex
from PyQt6.QtGui import QColor

# 类型英文值 -> 中文显示
_TYPE_CN = {"video": "视频", "live": "直播", "article": "专栏"}

# 自定义数据角色
TypeRole = Qt.ItemDataRole.UserRole + 1        # 类型中文（tag 用）
ProgressRole = Qt.ItemDataRole.UserRole + 2    # 观看进度 0.0~1.0，-1 表示无
ProgressTextRole = Qt.ItemDataRole.UserRole + 3  # "45%" / "已看完" / ""
LinkRole = Qt.ItemDataRole.UserRole + 4        # 打开链接
NewRole = Qt.ItemDataRole.UserRole + 5         # 本次新增记录标记


class HistoryTableModel(QAbstractTableModel):
    """历史记录表格模型"""

    COLUMNS = ["类型", "视频标题", "UP主", "观看进度", "分类", "观看时间", "BV号", "操作"]

    def __init__(self, rows: List[dict] = None, parent=None):
        super().__init__(parent)
        self._rows: List[dict] = rows or []
        self._new_ids: Set[str] = set()

    # ---- 数据设置 ----
    def set_rows(self, rows: List[dict]):
        """整体刷新数据"""
        self.beginResetModel()
        self._rows = rows or []
        self.endResetModel()

    def set_new_ids(self, ids: Set[str]):
        """标记本次抓取新增的记录 ID（complete 态 NEW 标签 + 淡绿底）"""
        self.beginResetModel()
        self._new_ids = ids or set()
        self.endResetModel()

    # ---- 内部取值 ----
    @staticmethod
    def _record_type(item: dict) -> str:
        return item.get("类型", "")

    @staticmethod
    def _record_id(item: dict) -> str:
        return str(item.get("BV号") or item.get("房间号") or item.get("文章ID") or "")

    @staticmethod
    def _record_key(item: dict) -> str:
        """类型感知的唯一键，用于 NEW 标记/去重（避免跨类型 ID 冲突）"""
        t = HistoryTableModel._record_type(item)
        rid = HistoryTableModel._record_id(item)
        return f"{t}_{rid}"

    @staticmethod
    def _progress_ratio(item: dict) -> float:
        """观看进度比例；直播/专栏或缺数据返回 -1"""
        try:
            dur = int(item.get("总时长(秒)", 0))
            pos = int(item.get("已观看(秒)", 0))
        except (TypeError, ValueError):
            return -1.0
        if dur <= 0:
            return -1.0
        return max(0.0, min(1.0, pos / dur))

    # ---- Qt 必需接口 ----
    def rowCount(self, parent=QModelIndex()) -> int:
        return 0 if parent.isValid() else len(self._rows)

    def columnCount(self, parent=QModelIndex()) -> int:
        return len(self.COLUMNS)

    def headerData(self, section, orientation, role=Qt.ItemDataRole.DisplayRole):
        if role != Qt.ItemDataRole.DisplayRole:
            return None
        if orientation == Qt.Orientation.Horizontal:
            return self.COLUMNS[section]
        return section + 1  # 行号

    def data(self, index: QModelIndex, role=Qt.ItemDataRole.DisplayRole):
        if not index.isValid():
            return None

        item = self._rows[index.row()]
        col = self.COLUMNS[index.column()]
        is_new = self._record_key(item) in self._new_ids

        # 自定义角色（任意列均可取，供委托使用）
        if role == TypeRole:
            return _TYPE_CN.get(item.get("类型", ""), item.get("类型", "") or "其他")
        if role == ProgressRole:
            return self._progress_ratio(item)
        if role == ProgressTextRole:
            ratio = self._progress_ratio(item)
            if ratio < 0:
                return ""
            return "已看完" if ratio >= 0.999 else f"{ratio * 100:.0f}%"
        if role == LinkRole:
            return (item.get("视频链接") or item.get("直播链接")
                    or item.get("专栏链接") or "")
        if role == NewRole:
            return is_new
        if role == Qt.ItemDataRole.BackgroundRole and is_new:
            return QColor("#EFFBF5")  # 新增行淡绿底
        if role == Qt.ItemDataRole.ForegroundRole:
            if col == "BV号":
                return QColor("#B0B0BE")
            if col == "观看时间" and is_new:
                return QColor("#00B578")
            if col in ("UP主", "观看时间"):
                return QColor("#4A4A5A")

        if role not in (Qt.ItemDataRole.DisplayRole, Qt.ItemDataRole.ToolTipRole):
            return None

        if col == "类型":
            # 显示文本由 TagDelegate 绘制，这里仅供 ToolTip / 无委托兜底
            return _TYPE_CN.get(item.get("类型", ""), item.get("类型", ""))
        if col == "视频标题":
            return item.get("标题", "")
        if col == "UP主":
            return item.get("UP主") or item.get("主播") or item.get("作者") or ""
        if col == "分类":
            return item.get("分类") or "-"
        if col == "观看进度":
            if role == Qt.ItemDataRole.ToolTipRole:
                return self.data(index, ProgressTextRole)
            return None if role == Qt.ItemDataRole.DisplayRole else ""
        if col == "观看时间":
            return item.get("观看时间", "")
        if col == "BV号":
            return self._record_id(item)
        if col == "操作":
            if role == Qt.ItemDataRole.ToolTipRole:
                return "在浏览器中打开"
            return None
        return None

    def sort(self, column: int, order: Qt.SortOrder):
        """表头点击排序：UP主/类型/分类相同值按观看时间倒序二次排序"""
        if not self._rows:
            return
        col = self.COLUMNS[column]
        reverse = order == Qt.SortOrder.DescendingOrder

        def _time_ts(r: dict) -> float:
            try:
                return datetime.strptime(r.get("观看时间", ""), "%Y-%m-%d %H:%M:%S").timestamp()
            except Exception:
                return 0.0

        # 先按观看时间倒序排（作为二次排序）
        self._rows.sort(key=_time_ts, reverse=True)

        if col == "UP主":
            self._rows.sort(
                key=lambda r: (r.get("UP主") or r.get("主播") or r.get("作者") or "").lower(),
                reverse=reverse,
            )
        elif col == "类型":
            self._rows.sort(
                key=lambda r: _TYPE_CN.get(r.get("类型", ""), r.get("类型", "")),
                reverse=reverse,
            )
        elif col == "分类":
            self._rows.sort(
                key=lambda r: (r.get("分类") or "-").lower(),
                reverse=reverse,
            )
        elif col == "视频标题":
            self._rows.sort(
                key=lambda r: (r.get("标题") or "").lower(),
                reverse=reverse,
            )
        elif col == "观看时间":
            self._rows.sort(key=_time_ts, reverse=reverse)
        elif col == "BV号":
            self._rows.sort(
                key=lambda r: self._record_id(r).lower(),
                reverse=reverse,
            )
        else:
            return
        self.layoutChanged.emit()
