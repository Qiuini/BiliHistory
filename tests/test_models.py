"""数据模型测试"""
import pytest
from datetime import datetime

import sys
import os
sys.path.insert(0, os.path.dirname(os.path.dirname(os.path.abspath(__file__))))

from models import (
    VideoRecord,
    LiveRecord,
    ArticleRecord,
    ContentType,
    HistoryConfig
)


class TestVideoRecord:
    """视频记录测试"""

    def test_create_video_record(self):
        record = VideoRecord(
            title="测试视频",
            bvid="BV1234567890",
            author="UP主A",
            view_time=datetime(2024, 6, 20, 12, 0, 0),
            duration=600,
            progress=300,
            link="https://www.bilibili.com/video/BV1234567890"
        )

        assert record.title == "测试视频"
        assert record.bvid == "BV1234567890"
        assert record.author == "UP主A"
        assert record.duration == 600
        assert record.progress == 300

    def test_completion_calculation(self):
        record = VideoRecord(
            title="测试",
            bvid="BV1",
            author="UP",
            view_time=datetime.now(),
            duration=100,
            progress=50
        )

        assert record.completion == "50.0%"

    def test_completion_zero_duration(self):
        record = VideoRecord(
            title="测试",
            bvid="BV1",
            author="UP",
            view_time=datetime.now(),
            duration=0,
            progress=50
        )

        assert record.completion == "N/A"

    def test_to_dict(self):
        record = VideoRecord(
            title="测试视频",
            bvid="BV1234567890",
            author="UP主A",
            view_time=datetime(2024, 6, 20, 12, 0, 0),
            duration=600,
            progress=300,
            link="https://example.com"
        )

        d = record.to_dict()
        assert d['标题'] == "测试视频"
        assert d['BV号'] == "BV1234567890"
        assert d['UP主'] == "UP主A"
        assert d['类型'] == "video"

    def test_get_unique_key(self):
        record = VideoRecord(
            title="测试",
            bvid="BV123",
            author="UP",
            view_time=datetime.now()
        )
        assert record.get_unique_key() == "video_BV123"


class TestLiveRecord:
    """直播记录测试"""

    def test_create_live_record(self):
        record = LiveRecord(
            title="测试直播",
            room_id="123456",
            author="主播A",
            view_time=datetime(2024, 6, 20, 12, 0, 0),
            link="https://live.bilibili.com/123456"
        )

        assert record.title == "测试直播"
        assert record.room_id == "123456"
        assert record.author == "主播A"
        assert record.content_type == ContentType.LIVE

    def test_get_unique_key(self):
        record = LiveRecord(
            title="测试",
            room_id="123",
            author="UP",
            view_time=datetime.now()
        )
        assert record.get_unique_key() == "live_123"


class TestArticleRecord:
    """专栏记录测试"""

    def test_create_article_record(self):
        record = ArticleRecord(
            title="测试专栏",
            article_id="98765",
            author="作者A",
            view_time=datetime(2024, 6, 20, 12, 0, 0),
            link="https://www.bilibili.com/read/cv98765"
        )

        assert record.title == "测试专栏"
        assert record.article_id == "98765"
        assert record.author == "作者A"
        assert record.content_type == ContentType.ARTICLE


class TestHistoryConfig:
    """配置测试"""

    def test_default_values(self):
        config = HistoryConfig(
            csv_file="test.csv",
            cookie="test_cookie"
        )

        assert config.csv_file == "test.csv"
        assert config.cookie == "test_cookie"
        assert config.page_size == 30  # 游标接口单页上限
        assert config.fetch_all is True
