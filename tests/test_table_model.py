"""表格模型测试 - 重点覆盖增量刷新与 hover 动画"""
import pytest
import os
import sys

sys.path.insert(0, os.path.dirname(os.path.dirname(os.path.abspath(__file__))))

from PyQt6.QtCore import Qt, QModelIndex

from gui.table_model import HistoryTableModel, NewRole, HoverRole


def _video_row(bvid: str, title: str = "标题", author: str = "UP主",
               view_time: str = "2024-06-20 12:00:00", category: str = "") -> dict:
    return {
        "类型": "video",
        "标题": title,
        "UP主": author,
        "BV号": bvid,
        "观看时间": view_time,
        "分类": category,
        "总时长(秒)": 600,
        "已观看(秒)": 300,
    }


class TestHistoryTableModel:
    """HistoryTableModel 单元测试"""

    def test_set_rows(self, qtbot):
        model = HistoryTableModel()
        with qtbot.wait_signal(model.modelReset):
            model.set_rows([_video_row("BV1"), _video_row("BV2")])
        assert model.rowCount() == 2

    def test_update_rows_insert_incrementally(self, qtbot):
        model = HistoryTableModel([_video_row("BV1")])
        assert model.rowCount() == 1

        with qtbot.wait_signal(model.rowsInserted):
            model.update_rows([_video_row("BV1"), _video_row("BV2")])
        assert model.rowCount() == 2
        assert model._rows[1]["BV号"] == "BV2"

    def test_update_rows_remove_incrementally(self, qtbot):
        model = HistoryTableModel([_video_row("BV1"), _video_row("BV2")])
        with qtbot.wait_signal(model.rowsRemoved):
            model.update_rows([_video_row("BV1")])
        assert model.rowCount() == 1
        assert model._rows[0]["BV号"] == "BV1"

    def test_update_rows_update_in_place(self, qtbot):
        model = HistoryTableModel([_video_row("BV1", title="旧标题")])
        with qtbot.wait_signal(model.dataChanged):
            model.update_rows([_video_row("BV1", title="新标题")])
        assert model.rowCount() == 1
        assert model._rows[0]["标题"] == "新标题"

    def test_update_rows_preserves_existing_identity(self, qtbot):
        """增量刷新应保持已有行的身份与顺序，排序由视图层 sortByColumn 负责"""
        model = HistoryTableModel([_video_row("BV1"), _video_row("BV2")])
        original_id = id(model._rows[0])
        model.update_rows([_video_row("BV1"), _video_row("BV2", title="更新")])
        assert id(model._rows[0]) == original_id
        assert model._rows[1]["标题"] == "更新"

    def test_set_new_ids_emits_data_changed(self, qtbot):
        model = HistoryTableModel([_video_row("BV1"), _video_row("BV2")])
        with qtbot.wait_signal(model.dataChanged):
            model.set_new_ids({"video_BV1"})
        assert model.data(model.index(0, 0), NewRole) is True
        assert model.data(model.index(1, 0), NewRole) is False

    def test_set_new_ids_no_reset(self, qtbot):
        model = HistoryTableModel([_video_row("BV1")])
        # modelReset 不应被触发
        with qtbot.assert_not_emitted(model.modelReset):
            model.set_new_ids({"video_BV1"})

    def test_hover_row_animation(self, qtbot):
        model = HistoryTableModel([_video_row("BV1")])
        model.set_hover_row(0)
        qtbot.wait(50)
        assert model._hover_row == 0
        assert 0.0 <= model._hover_progress <= 1.0
        assert model.data(model.index(0, 0), HoverRole) == model._hover_progress

        model.set_hover_row(-1)
        qtbot.wait(50)
        assert model._hover_row == -1
