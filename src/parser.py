"""
数据解析层 - 将 API 响应转换为数据模型
"""
from abc import ABC, abstractmethod
from datetime import datetime
from typing import List, Optional

from models import (
    VideoRecord,
    LiveRecord,
    ArticleRecord,
    ContentType,
    FollowingRecord,
    FavFolderRecord,
    FavResourceRecord,
)
from logger import logger


class BaseParser(ABC):
    """解析器基类"""

    @abstractmethod
    def parse(self, item: dict):
        """解析单个项目"""
        pass

    @staticmethod
    def parse_timestamp(timestamp: int) -> datetime:
        """解析时间戳"""
        if timestamp:
            return datetime.fromtimestamp(timestamp)
        return datetime.now()

    @staticmethod
    def get_history(item: dict) -> dict:
        """获取游标接口的 history 嵌套对象"""
        return item.get("history") or {}


class VideoParser(BaseParser):
    """视频解析器"""

    def parse(self, item: dict) -> Optional[VideoRecord]:
        """解析视频记录"""
        history = self.get_history(item)
        bvid = history.get("bvid") or item.get("bvid", "")
        if not bvid:
            return None

        title = item.get("title", "未知标题")
        duration = item.get("duration", 0)
        progress = item.get("progress", 0)
        if progress == -1:  # 游标接口中 -1 表示已看完
            progress = duration
        view_at_ts = item.get("view_at", 0)

        # 获取 UP 主信息
        author = self._get_author(item)
        category = self._get_category(item)

        return VideoRecord(
            title=title,
            bvid=bvid,
            author=author,
            view_time=self.parse_timestamp(view_at_ts),
            duration=duration,
            progress=progress,
            link=f"https://www.bilibili.com/video/{bvid}",
            category=category
        )

    @staticmethod
    def _get_category(item: dict) -> str:
        """获取视频分区/分类"""
        # 方式1: 游标接口常见字段 tname / tag_name / typename
        for key in ("tname", "tag_name", "typename"):
            value = item.get(key)
            if value:
                return value

        # 方式2: tag 列表取第一个
        tags = item.get("tag")
        if isinstance(tags, list) and tags:
            return tags[0].get("tag_name") or tags[0].get("name") or ""

        # 方式3: 嵌套 obj 中的分类名
        obj = item.get("stat") or item.get("dynamic") or {}
        if isinstance(obj, dict):
            return obj.get("tname") or ""

        return ""

    @staticmethod
    def _get_author(item: dict) -> str:
        """获取 UP 主名称"""
        # 方式1: 游标接口的 author_name 字段
        if item.get("author_name"):
            return item["author_name"]

        # 方式2: upper 字段
        upper = item.get("upper", {})
        if upper:
            return upper.get("name") or upper.get("uname")

        # 方式3: owner 字段
        owner = item.get("owner", {})
        if owner:
            return owner.get("name") or owner.get("uname")

        # 方式4: 根对象 name 字段
        return item.get("name") or "未知UP主"


class LiveParser(BaseParser):
    """直播解析器"""

    def parse(self, item: dict) -> Optional[LiveRecord]:
        """解析直播记录"""
        history = self.get_history(item)
        room_id = history.get("oid") or item.get("room_id", "")
        if not room_id:
            return None

        title = item.get("title", "未知标题")
        view_at_ts = item.get("view_at", 0)

        # 获取主播信息
        upper = item.get("upper") or item.get("owner") or {}
        author = item.get("author_name") or upper.get("name") or upper.get("uname", "未知主播")

        # badge 在直播场景通常表示 "直播中"/"未开播" 等状态
        category = item.get("badge") or ""

        return LiveRecord(
            title=title,
            room_id=str(room_id),
            author=author,
            view_time=self.parse_timestamp(view_at_ts),
            link=f"https://live.bilibili.com/{room_id}",
            category=category
        )


class ArticleParser(BaseParser):
    """专栏解析器"""

    def parse(self, item: dict) -> Optional[ArticleRecord]:
        """解析专栏记录"""
        history = self.get_history(item)
        article_id = history.get("oid") or item.get("id", "")
        if not article_id:
            return None

        title = item.get("title", "未知标题")
        view_at_ts = item.get("view_at", 0)

        # 获取作者信息
        upper = item.get("upper") or item.get("author") or {}
        author = item.get("author_name") or upper.get("name") or upper.get("uname", "未知作者")

        # 专栏分类：优先 category，否则按模板 ID 兜底为常见分区
        category = item.get("category") or ""
        if not category:
            category = self._category_from_template(item.get("template_id"))

        return ArticleRecord(
            title=title,
            article_id=str(article_id),
            author=author,
            view_time=self.parse_timestamp(view_at_ts),
            link=f"https://www.bilibili.com/read/cv{article_id}",
            category=category
        )

    @staticmethod
    def _category_from_template(template_id) -> str:
        """根据专栏模板 ID 映射常见分类（B站模板 ID 多为 4 位数字）"""
        mapping = {
            "4": "动画",
            "17": "游戏",
            "3": "音乐",
            "129": "舞蹈",
            "36": "知识",
            "188": "数码",
            "95": "科技",
            "122": "生活",
            "160": "时尚",
            "211": "美食",
            "119": "鬼畜",
            "155": "娱乐",
            "5": "影视",
            "181": "影视",
            "202": "资讯",
        }
        tid = str(template_id) if template_id is not None else ""
        return mapping.get(tid, "")


class UserInfoParser(BaseParser):
    """用户信息解析器（注册时间等）"""

    def parse(self, item: dict) -> Optional[datetime]:
        """解析用户卡片数据，返回注册时间"""
        return self.parse_registration_time(item)

    def parse_registration_time(self, raw_data: dict) -> Optional[datetime]:
        """从用户卡片 API 响应中解析注册时间

        Args:
            raw_data: API 原始响应

        Returns:
            注册时间 datetime，失败返回 None
        """
        if not isinstance(raw_data, dict):
            return None

        reg_ts = raw_data.get("data", {}).get("card", {}).get("regtime")
        if reg_ts:
            return self.parse_timestamp(int(reg_ts))
        return None


class HistoryParser:
    """历史记录解析器 - 统一调度"""

    def __init__(self):
        self.parsers = {
            "archive": VideoParser(),
            "live": LiveParser(),
            "article": ArticleParser()
        }

    def parse(self, raw_data: dict) -> List:
        """解析 API 返回的历史数据"""
        records = []

        # 获取列表数据
        history_list = raw_data.get("data", {}).get("list", []) if isinstance(raw_data, dict) else raw_data

        for item in history_list:
            # 游标接口 business 位于 history 嵌套对象中，兼容旧版扁平字段
            business_type = (item.get("history") or {}).get("business") or item.get("business")
            parser = self.parsers.get(business_type)

            if parser:
                record = parser.parse(item)
                if record:
                    records.append(record)

        return records

    def parse_page(self, raw_data: dict) -> tuple:
        """解析一页数据，返回 (记录列表, 是否还有更多)"""
        records = self.parse(raw_data)
        cursor = raw_data.get("data", {}).get("cursor", {}) if isinstance(raw_data, dict) else {}
        has_more = bool(cursor.get("max") or cursor.get("view_at"))
        return records, has_more


class FollowingParser:
    """关注列表解析器"""

    def parse(self, raw_data: dict) -> List[FollowingRecord]:
        items = raw_data.get("data", {}).get("list", []) if isinstance(raw_data, dict) else raw_data
        records = []
        for item in items:
            mid = item.get("mid")
            if not mid:
                continue
            official = item.get("official_verify") or {}
            records.append(FollowingRecord(
                mid=str(mid),
                name=item.get("uname") or item.get("username") or "未知UP主",
                face=item.get("face", ""),
                sign=item.get("sign", ""),
                official=official.get("desc", "") if isinstance(official, dict) else str(official),
                level=item.get("level", 0),
                link=f"https://space.bilibili.com/{mid}"
            ))
        return records


class FavFolderParser:
    """收藏夹文件夹解析器"""

    def parse(self, raw_data: dict) -> List[FavFolderRecord]:
        items = raw_data.get("data", {}).get("list", []) if isinstance(raw_data, dict) else raw_data
        records = []
        for item in items:
            folder_id = item.get("id")
            if not folder_id:
                continue
            records.append(FavFolderRecord(
                folder_id=str(folder_id),
                title=item.get("title", "未命名收藏夹"),
                media_count=item.get("media_count", 0),
                link=f"https://space.bilibili.com/favlist?fid={folder_id}"
            ))
        return records


class FavResourceParser:
    """收藏夹内容解析器"""

    def parse(self, raw_data: dict) -> List[FavResourceRecord]:
        items = raw_data.get("data", {}).get("medias", []) if isinstance(raw_data, dict) else raw_data
        records = []
        for item in items:
            title = item.get("title", "未知标题")
            upper = item.get("upper", {}) or {}
            bvid = item.get("bvid") or item.get("id", "")
            link = f"https://www.bilibili.com/video/{bvid}" if bvid else ""
            records.append(FavResourceRecord(
                title=title,
                bvid=bvid,
                link=link,
                author=upper.get("name") or upper.get("uname", ""),
                cover=item.get("cover", ""),
                resource_type=self._type_name(item.get("type", 2))
            ))
        return records

    @staticmethod
    def _type_name(type_id: int) -> str:
        mapping = {2: "video", 12: "article", 21: "video", 22: "bangumi"}
        return mapping.get(type_id, "video")
