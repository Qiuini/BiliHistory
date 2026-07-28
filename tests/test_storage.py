"""存储层测试"""
import pytest
import tempfile
import os
from datetime import datetime

import sys
sys.path.insert(0, os.path.dirname(os.path.dirname(os.path.abspath(__file__))))

from storage import CSVStorage, HistoryStorage
from models import VideoRecord, LiveRecord, ArticleRecord


class TestCSVStorage:
    """CSV 存储测试"""

    def test_save_and_load(self):
        with tempfile.NamedTemporaryFile(mode='w', suffix='.csv', delete=False, encoding='utf-8') as f:
            temp_file = f.name

        try:
            records = [
                VideoRecord(
                    title="测试视频",
                    bvid="BV123",
                    author="UP主",
                    view_time=datetime(2024, 6, 20, 12, 0, 0),
                    duration=600,
                    progress=300,
                    link="https://example.com"
                )
            ]

            storage = CSVStorage(temp_file)
            assert storage.save(records) is True

            # 加载
            storage2 = CSVStorage(temp_file)
            data = storage2.load()
            assert len(data) == 1
            assert data[0]['标题'] == "测试视频"
            assert data[0]['BV号'] == "BV123"

        finally:
            if os.path.exists(temp_file):
                os.unlink(temp_file)

    def test_load_nonexistent_file(self):
        storage = CSVStorage("/nonexistent/path/test.csv")
        data = storage.load()
        assert data == []

    def test_save_mixed_records(self):
        """混合视频/直播/专栏记录保存时不缺列、不乱序"""
        with tempfile.NamedTemporaryFile(mode='w', suffix='.csv', delete=False, encoding='utf-8') as f:
            temp_file = f.name

        try:
            records = [
                VideoRecord(
                    title="视频",
                    bvid="BV1",
                    author="UP",
                    view_time=datetime(2024, 6, 20, 12, 0, 0),
                    duration=600,
                    progress=300,
                    category="音乐"
                ),
                LiveRecord(
                    title="直播",
                    room_id="123",
                    author="主播",
                    view_time=datetime(2024, 6, 20, 13, 0, 0),
                    category="未开播"
                ),
                ArticleRecord(
                    title="专栏",
                    article_id="456",
                    author="作者",
                    view_time=datetime(2024, 6, 20, 14, 0, 0),
                    category="数码"
                )
            ]

            storage = CSVStorage(temp_file)
            assert storage.save(records) is True

            loaded = CSVStorage(temp_file).load()
            assert len(loaded) == 3

            by_id = {r.get('BV号') or r.get('房间号') or r.get('文章ID'): r for r in loaded}
            assert by_id['BV1']['分类'] == '音乐'
            assert by_id['BV1']['UP主'] == 'UP'
            assert by_id['123']['主播'] == '主播'
            assert by_id['123']['分类'] == '未开播'
            assert by_id['456']['作者'] == '作者'
            assert by_id['456']['分类'] == '数码'

        finally:
            if os.path.exists(temp_file):
                os.unlink(temp_file)


class TestHistoryStorage:
    """历史存储测试"""

    def test_add_records_deduplication(self):
        with tempfile.NamedTemporaryFile(mode='w', suffix='.csv', delete=False, encoding='utf-8') as f:
            temp_file = f.name

        try:
            # 初始数据
            initial_records = [
                VideoRecord(
                    title="已有视频",
                    bvid="BV001",
                    author="UP",
                    view_time=datetime(2024, 6, 20),
                    duration=100,
                    progress=50,
                    link=""
                )
            ]

            storage = HistoryStorage(temp_file)
            storage._data = [r.to_dict() for r in initial_records]

            # 添加重复
            new_records = [
                VideoRecord(
                    title="已有视频",
                    bvid="BV001",  # 重复
                    author="UP",
                    view_time=datetime(2024, 6, 21),
                    duration=100,
                    progress=100,
                    link=""
                ),
                VideoRecord(
                    title="新视频",
                    bvid="BV002",
                    author="UP",
                    view_time=datetime(2024, 6, 22),
                    duration=200,
                    progress=200,
                    link=""
                )
            ]

            added = storage.add_records(new_records)
            assert added == 1  # 只添加1条新记录
            assert storage.count == 2

        finally:
            if os.path.exists(temp_file):
                os.unlink(temp_file)

    def test_remove_duplicates_by_id(self):
        with tempfile.NamedTemporaryFile(mode='w', suffix='.csv', delete=False, encoding='utf-8') as f:
            temp_file = f.name

        try:
            storage = HistoryStorage(temp_file)
            storage._data = [
                {'标题': '旧', 'BV号': 'BV1', '观看时间': '2024-06-01 00:00:00', 'UP主': 'UP'},
                {'标题': '新', 'BV号': 'BV1', '观看时间': '2024-06-02 00:00:00', 'UP主': 'UP'},
                {'标题': '独1', 'BV号': 'BV2', '观看时间': '2024-06-01 00:00:00', 'UP主': 'UP'},
            ]

            removed = storage.remove_duplicates_by_id()
            assert removed == 1  # 移除1条重复
            assert storage.count == 2
            # 应该保留最新的（6月2日的）
            assert storage._data[0]['标题'] == '新'

        finally:
            if os.path.exists(temp_file):
                os.unlink(temp_file)

    def test_sort_by_time(self):
        with tempfile.NamedTemporaryFile(mode='w', suffix='.csv', delete=False, encoding='utf-8') as f:
            temp_file = f.name

        try:
            storage = HistoryStorage(temp_file)
            storage._data = [
                {'标题': '早', '观看时间': '2024-06-01 00:00:00'},
                {'标题': '晚', '观看时间': '2024-06-03 00:00:00'},
                {'标题': '中', '观看时间': '2024-06-02 00:00:00'},
            ]

            storage.sort_by_time(reverse=True)
            assert storage._data[0]['标题'] == '晚'
            assert storage._data[2]['标题'] == '早'

        finally:
            if os.path.exists(temp_file):
                os.unlink(temp_file)
