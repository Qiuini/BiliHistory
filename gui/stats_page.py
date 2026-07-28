"""
数据统计页面 - 基于本地 CSV 生成用户画像
无额外图表库依赖，使用 QProgressBar + QLabel 拼出图表效果。
"""
from typing import List, Dict, Any

from PyQt6.QtCore import Qt
from PyQt6.QtWidgets import (
    QWidget, QVBoxLayout, QHBoxLayout, QLabel, QGridLayout, QSizePolicy,
    QProgressBar
)

from gui import theme
from analytics import (
    compute_basic_stats,
    top_authors,
    top_categories,
    time_of_day_distribution,
    top_authors_by_completion,
    daily_trend,
)


class StatCard(QWidget):
    """顶部概览数字卡"""

    def __init__(self, title: str, value: str, sub: str = "", parent=None):
        super().__init__(parent)
        self.setObjectName("StatCard")
        lay = QVBoxLayout(self)
        lay.setContentsMargins(20, 18, 20, 18)
        lay.setSpacing(6)

        self.value_label = QLabel(value)
        self.value_label.setObjectName("StatCardValue")
        self.value_label.setStyleSheet(f"font-size:28px;font-weight:800;color:{theme.TEXT};")

        self.title_label = QLabel(title)
        self.title_label.setObjectName("StatCardLabel")
        self.title_label.setStyleSheet(f"font-size:12px;color:{theme.TEXT_3};")

        self.sub_label = QLabel(sub)
        self.sub_label.setObjectName("StatCardSub")
        self.sub_label.setStyleSheet(f"font-size:11px;color:{theme.TEXT_2};")
        self.sub_label.setVisible(bool(sub))

        lay.addWidget(self.value_label)
        lay.addWidget(self.title_label)
        lay.addWidget(self.sub_label)
        lay.addStretch(1)


class HorizontalBarRow(QWidget):
    """单行水平条：排名 + 名称 + 进度条 + 数值"""

    def __init__(self, rank: int, name: str, value: float, max_value: float,
                 text: str, color: str, unit: str = "", parent=None):
        super().__init__(parent)
        self.setMinimumHeight(30)
        lay = QHBoxLayout(self)
        lay.setContentsMargins(0, 4, 0, 4)
        lay.setSpacing(8)

        rank_label = QLabel(str(rank))
        rank_label.setFixedWidth(18)
        rank_label.setAlignment(Qt.AlignmentFlag.AlignCenter)
        rank_label.setStyleSheet(f"font-size:11px;color:{theme.TEXT_DISABLED};")

        name_label = QLabel(name)
        name_label.setFixedWidth(80)
        name_label.setAlignment(Qt.AlignmentFlag.AlignVCenter | Qt.AlignmentFlag.AlignLeft)
        name_label.setStyleSheet(f"font-size:12px;color:{theme.TEXT};")
        name_label.setToolTip(name)

        bar = QProgressBar()
        bar.setTextVisible(False)
        bar.setMaximum(int(max_value * 100) if max_value > 0 else 1)
        bar.setValue(int(value * 100))
        bar.setFixedHeight(8)
        bar.setSizePolicy(QSizePolicy.Policy.Expanding, QSizePolicy.Policy.Fixed)
        bar.setStyleSheet(f"""
            QProgressBar {{
                background: {theme.SURFACE_PRESSED};
                border: none;
                border-radius: 4px;
            }}
            QProgressBar::chunk {{
                background: {color};
                border-radius: 4px;
            }}
        """)

        value_label = QLabel(f"{int(value)}{unit}" if unit else text)
        value_label.setFixedWidth(52)
        value_label.setAlignment(Qt.AlignmentFlag.AlignVCenter | Qt.AlignmentFlag.AlignRight)
        value_label.setStyleSheet(f"font-size:11px;color:{theme.TEXT_2};")

        lay.addWidget(rank_label)
        lay.addWidget(name_label)
        lay.addWidget(bar, 1)
        lay.addWidget(value_label)


class HorizontalBarChart(QWidget):
    """水平条形图容器"""

    def __init__(self, rows: List[Dict[str, Any]], value_key: str, max_items: int = 8,
                 bar_color: str = theme.PINK, unit: str = "", parent=None):
        super().__init__(parent)
        self.rows = rows[:max_items]
        self.value_key = value_key
        self.bar_color = bar_color
        self.unit = unit
        self._max_value = max((r.get(value_key, 0) or 1) for r in self.rows) if self.rows else 1
        self._lay = QVBoxLayout(self)
        self._lay.setContentsMargins(0, 0, 0, 0)
        self._lay.setSpacing(0)
        self._render()

    def update_rows(self, rows: List[Dict[str, Any]]):
        # 清空旧行
        while self._lay.count():
            item = self._lay.takeAt(0)
            if item.widget():
                item.widget().deleteLater()
        self.rows = rows[:8]
        self._max_value = max((r.get(self.value_key, 0) or 1) for r in self.rows) if self.rows else 1
        self._render()

    def _render(self):
        for i, row in enumerate(self.rows):
            value = row.get(self.value_key, 0)
            text = row.get("watch_time_text", "") or row.get("avg_completion_text", "")
            if not text:
                text = str(value)
            row_widget = HorizontalBarRow(
                rank=i + 1,
                name=row.get("name", ""),
                value=value,
                max_value=self._max_value,
                text=text,
                color=self.bar_color,
                unit=self.unit,
            )
            self._lay.addWidget(row_widget)
        self._lay.addStretch(1)


class VerticalBarGroup(QWidget):
    """竖条组：用于时段分布和每日趋势"""

    def __init__(self, data: List[Dict[str, Any]], value_key: str, label_key: str,
                 color: str, parent=None):
        super().__init__(parent)
        self.data = data
        self.value_key = value_key
        self.label_key = label_key
        self.color = color
        self.setMinimumHeight(160)
        self._lay = QHBoxLayout(self)
        self._lay.setContentsMargins(0, 0, 0, 0)
        self._lay.setSpacing(12)
        self._render()

    def update_data(self, data: List[Dict[str, Any]]):
        while self._lay.count():
            item = self._lay.takeAt(0)
            if item.widget():
                item.widget().deleteLater()
        self.data = data
        self._render()

    def _render(self):
        max_value = max((d.get(self.value_key, 0) for d in self.data), default=1) or 1
        for d in self.data:
            value = d.get(self.value_key, 0)
            label = d.get(self.label_key, "")
            col = QVBoxLayout()
            col.setSpacing(4)
            col.setAlignment(Qt.AlignmentFlag.AlignHCenter)

            count_label = QLabel(str(value))
            count_label.setAlignment(Qt.AlignmentFlag.AlignCenter)
            count_label.setStyleSheet(f"font-size:11px;color:{theme.TEXT};")

            bar = QProgressBar()
            bar.setOrientation(Qt.Orientation.Vertical)
            bar.setTextVisible(False)
            bar.setMaximum(int(max_value * 100))
            bar.setValue(int(value * 100))
            bar.setFixedWidth(36)
            bar.setSizePolicy(QSizePolicy.Policy.Fixed, QSizePolicy.Policy.Expanding)
            bar.setStyleSheet(f"""
                QProgressBar {{
                    background: {theme.SURFACE_PRESSED};
                    border: none;
                    border-radius: 6px;
                }}
                QProgressBar::chunk {{
                    background: {self.color};
                    border-radius: 6px;
                }}
            """)

            label_label = QLabel(label)
            label_label.setAlignment(Qt.AlignmentFlag.AlignCenter)
            label_label.setStyleSheet(f"font-size:10px;color:{theme.TEXT_2};")
            label_label.setWordWrap(True)
            label_label.setFixedWidth(44)

            col.addWidget(count_label)
            col.addWidget(bar, 1)
            col.addWidget(label_label)

            wrapper = QWidget()
            wrapper.setLayout(col)
            self._lay.addWidget(wrapper, alignment=Qt.AlignmentFlag.AlignBottom)
        self._lay.addStretch(1)


class ChartCard(QWidget):
    """带标题的图表卡片"""

    def __init__(self, title: str, parent=None):
        super().__init__(parent)
        self.setObjectName("ChartCard")
        self.lay = QVBoxLayout(self)
        self.lay.setContentsMargins(20, 16, 20, 16)
        self.lay.setSpacing(12)

        self.title_label = QLabel(title)
        self.title_label.setObjectName("ChartCardTitle")
        self.title_label.setStyleSheet(f"font-size:14px;font-weight:700;color:{theme.TEXT};")
        self.lay.addWidget(self.title_label)

    def set_chart(self, widget: QWidget):
        self.lay.addWidget(widget)


class StatsPage(QWidget):
    """数据统计主页面"""

    def __init__(self, parent=None):
        super().__init__(parent)
        self.setObjectName("StatsPage")
        self._build_ui()

    def _build_ui(self):
        root = QVBoxLayout(self)
        root.setContentsMargins(24, 20, 24, 20)
        root.setSpacing(20)

        # 页面标题
        header = QLabel("我的画像")
        header.setObjectName("StatsPageTitle")
        header.setStyleSheet(f"font-size:20px;font-weight:800;color:{theme.TEXT};")
        root.addWidget(header)

        # 概览卡片
        self.overview_grid = QGridLayout()
        self.overview_grid.setSpacing(12)
        self.cards = {
            "total_records": StatCard("总记录数", "0", "视频/直播/专栏合计"),
            "watch_time": StatCard("累计观看时长", "0分钟", "按已观看进度估算"),
            "unique_authors": StatCard("关注创作者", "0", "不重复 UP 主/主播/作者"),
            "avg_completion": StatCard("平均完播率", "0%", "仅统计有进度视频"),
        }
        positions = [(0, 0), (0, 1), (0, 2), (0, 3)]
        for (name, card), (r, c) in zip(self.cards.items(), positions):
            self.overview_grid.addWidget(card, r, c)
        root.addLayout(self.overview_grid)

        # 图表区：2 列
        charts_row = QHBoxLayout()
        charts_row.setSpacing(16)

        left_col = QVBoxLayout()
        left_col.setSpacing(16)
        self.author_card = ChartCard("最常看 UP 主 TOP10")
        self.author_chart = HorizontalBarChart([], "count", bar_color=theme.PINK)
        self.author_card.set_chart(self.author_chart)
        left_col.addWidget(self.author_card)

        self.completion_card = ChartCard("完播率最高 UP 主 TOP10")
        self.completion_chart = HorizontalBarChart([], "avg_completion", bar_color=theme.SUCCESS,
                                                   max_items=8, unit="%")
        self.completion_card.set_chart(self.completion_chart)
        left_col.addWidget(self.completion_card)

        right_col = QVBoxLayout()
        right_col.setSpacing(16)
        self.category_card = ChartCard("最常看分类 TOP10")
        self.category_chart = HorizontalBarChart([], "count", bar_color=theme.BLUE)
        self.category_card.set_chart(self.category_chart)
        right_col.addWidget(self.category_card)

        self.time_card = ChartCard("观看时段分布")
        self.time_chart = VerticalBarGroup([], "count", "label", theme.BLUE)
        self.time_card.set_chart(self.time_chart)
        right_col.addWidget(self.time_card)

        self.trend_card = ChartCard("最近 30 天观看趋势")
        self.trend_chart = VerticalBarGroup([], "count", "date", theme.PINK)
        self.trend_card.set_chart(self.trend_chart)
        right_col.addWidget(self.trend_card)

        charts_row.addLayout(left_col, stretch=5)
        charts_row.addLayout(right_col, stretch=5)
        root.addLayout(charts_row)
        root.addStretch(1)

    def load_data(self, records: List[dict]):
        """加载记录并刷新所有图表"""
        basic = compute_basic_stats(records)

        self.cards["total_records"].value_label.setText(str(basic["total_records"]))
        self.cards["total_records"].sub_label.setText(
            f"视频 {basic['total_videos']} · 直播 {basic['total_lives']} · 专栏 {basic['total_articles']}")
        self.cards["watch_time"].value_label.setText(basic["total_watch_time_text"])
        self.cards["unique_authors"].value_label.setText(str(basic["unique_authors"]))
        self.cards["avg_completion"].value_label.setText(basic["avg_completion"])

        authors = top_authors(records, top_n=10)
        self.author_chart.update_rows(authors)

        categories = top_categories(records, top_n=10)
        self.category_chart.update_rows(categories)

        time_dist = time_of_day_distribution(records)
        time_data = [
            {"label": "凌晨\n00-06", "count": time_dist["凌晨"]},
            {"label": "上午\n06-12", "count": time_dist["上午"]},
            {"label": "下午\n12-18", "count": time_dist["下午"]},
            {"label": "晚上\n18-24", "count": time_dist["晚上"]},
        ]
        self.time_chart.update_data(time_data)

        completions = top_authors_by_completion(records, min_count=2, top_n=8)
        for r in completions:
            r["avg_completion"] = r["avg_completion"] * 100
        self.completion_chart.update_rows(completions)

        trend = daily_trend(records, days=30)
        self.trend_chart.update_data(trend)
