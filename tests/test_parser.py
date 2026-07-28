"""解析器测试"""
import pytest
import json
import os

import sys
sys.path.insert(0, os.path.dirname(os.path.dirname(os.path.abspath(__file__))))

from parser import (
    VideoParser, LiveParser, ArticleParser, HistoryParser,
    FollowingParser, FavFolderParser, FavResourceParser,
    UserInfoParser,
)
from models import ContentType


# 加载 mock 数据
FIXTURE_PATH = os.path.join(os.path.dirname(__file__), 'fixtures', 'mock_api_response.json')
with open(FIXTURE_PATH, 'r', encoding='utf-8') as f:
    MOCK_DATA = json.load(f)


class TestVideoParser:
    """视频解析器测试"""

    def test_parse_valid_video(self):
        parser = VideoParser()
        item = MOCK_DATA['data']['list'][0]  # 第一个视频

        record = parser.parse(item)

        assert record is not None
        assert record.title == "测试视频1"
        assert record.bvid == "BV1234567890"
        assert record.author == "UP主A"
        assert record.duration == 600
        assert record.progress == 300
        assert record.category == "音乐"
        assert record.content_type == ContentType.VIDEO

    def test_parse_missing_bvid(self):
        parser = VideoParser()
        item = {"title": "无BV号", "business": "archive"}

        record = parser.parse(item)
        assert record is None

    def test_parse_no_upper(self):
        parser = VideoParser()
        item = {
            "title": "测试",
            "bvid": "BV1",
            "owner": {"name": "OwnerUP"},
            "view_at": 1718900000
        }

        record = parser.parse(item)
        assert record.author == "OwnerUP"


class TestLiveParser:
    """直播解析器测试"""

    def test_parse_valid_live(self):
        parser = LiveParser()
        item = MOCK_DATA['data']['list'][2]  # 直播

        record = parser.parse(item)

        assert record is not None
        assert record.title == "测试直播"
        assert record.room_id == "123456"
        assert record.author == "主播A"
        assert record.category == "未开播"
        assert record.content_type == ContentType.LIVE

    def test_parse_missing_room_id(self):
        parser = LiveParser()
        item = {"title": "无房间号", "business": "live"}

        record = parser.parse(item)
        assert record is None


class TestArticleParser:
    """专栏解析器测试"""

    def test_parse_valid_article(self):
        parser = ArticleParser()
        item = MOCK_DATA['data']['list'][3]  # 专栏

        record = parser.parse(item)

        assert record is not None
        assert record.title == "测试专栏"
        assert record.article_id == "98765"
        assert record.author == "作者A"
        assert record.category == "数码"
        assert record.content_type == ContentType.ARTICLE


    def test_parse_article_category_from_template(self):
        parser = ArticleParser()
        item = {
            "title": "游戏攻略",
            "history": {"oid": 111, "business": "article"},
            "author_name": "作者B",
            "view_at": 1718600000,
            "template_id": 17
        }

        record = parser.parse(item)
        assert record is not None
        assert record.category == "游戏"


class TestHistoryParser:
    """历史解析器测试"""

    def test_parse_all_types(self):
        parser = HistoryParser()
        records = parser.parse(MOCK_DATA)

        assert len(records) == 4
        assert any(r.content_type == ContentType.VIDEO for r in records)
        assert any(r.content_type == ContentType.LIVE for r in records)
        assert any(r.content_type == ContentType.ARTICLE for r in records)

    def test_parse_empty_list(self):
        parser = HistoryParser()
        records = parser.parse({"data": {"list": []}})

        assert len(records) == 0


class TestFollowingParser:
    """关注列表解析器测试"""

    def test_parse_valid(self):
        parser = FollowingParser()
        data = {
            "code": 0,
            "data": {
                "list": [
                    {"mid": 123, "uname": "UP主A", "sign": "签名A", "level": 6, "face": "http://a.jpg"},
                    {"mid": 456, "uname": "UP主B", "sign": "签名B", "level": 5, "face": ""},
                ]
            }
        }
        records = parser.parse(data)
        assert len(records) == 2
        assert records[0].mid == "123"
        assert records[0].name == "UP主A"
        assert records[0].link == "https://space.bilibili.com/123"

    def test_parse_missing_mid(self):
        parser = FollowingParser()
        data = {"code": 0, "data": {"list": [{"uname": "无名"}]}}
        assert len(parser.parse(data)) == 0


class TestFavFolderParser:
    """收藏夹文件夹解析器测试"""

    def test_parse_valid(self):
        parser = FavFolderParser()
        data = {
            "code": 0,
            "data": {
                "list": [
                    {"id": 11, "title": "默认收藏夹", "media_count": 10},
                    {"id": 22, "title": "技术", "media_count": 5},
                ]
            }
        }
        records = parser.parse(data)
        assert len(records) == 2
        assert records[0].folder_id == "11"
        assert records[0].title == "默认收藏夹"
        assert records[0].link == "https://space.bilibili.com/favlist?fid=11"


class TestFavResourceParser:
    """收藏夹内容解析器测试"""

    def test_parse_valid(self):
        parser = FavResourceParser()
        data = {
            "code": 0,
            "data": {
                "medias": [
                    {"title": "视频1", "bvid": "BV1xx", "upper": {"name": "UP主A"}, "type": 2},
                    {"title": "专栏1", "id": "cv123", "upper": {"name": "UP主B"}, "type": 12},
                ]
            }
        }
        records = parser.parse(data)
        assert len(records) == 2
        assert records[0].title == "视频1"
        assert records[0].author == "UP主A"
        assert records[1].resource_type == "article"


class TestUserInfoParser:
    """用户信息解析器测试"""

    def test_parse_registration_time_valid(self):
        parser = UserInfoParser()
        data = {
            "code": 0,
            "data": {
                "card": {
                    "mid": "123456",
                    "name": "测试用户",
                    "regtime": 1609459200
                }
            }
        }
        reg_time = parser.parse_registration_time(data)
        assert reg_time is not None
        assert reg_time.year == 2021
        assert reg_time.month == 1
        assert reg_time.day == 1

    def test_parse_registration_time_missing(self):
        parser = UserInfoParser()
        data = {
            "code": 0,
            "data": {
                "card": {
                    "mid": "123456",
                    "name": "测试用户"
                }
            }
        }
        assert parser.parse_registration_time(data) is None

    def test_parse_registration_time_invalid(self):
        parser = UserInfoParser()
        assert parser.parse_registration_time(None) is None
        assert parser.parse_registration_time("not dict") is None
        assert parser.parse_registration_time({"code": -404}) is None
