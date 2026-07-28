"""
数据模型层 - 定义所有数据结构
"""
from dataclasses import dataclass, field
from datetime import datetime
from enum import Enum
from typing import Optional


class ContentType(Enum):
    """内容类型枚举"""
    VIDEO = "video"
    LIVE = "live"
    ARTICLE = "article"


@dataclass
class VideoRecord:
    """视频记录"""
    title: str
    bvid: str
    author: str
    view_time: datetime
    duration: int = 0  # 总时长(秒)
    progress: int = 0  # 已观看(秒)
    link: str = ""
    category: str = ""  # 分区/分类，如 音乐、科技
    content_type: ContentType = field(default=ContentType.VIDEO)

    def get_unique_key(self) -> str:
        return f"video_{self.bvid}"

    @property
    def completion(self) -> str:
        """完成度"""
        if self.duration > 0 and self.progress >= 0:
            return f"{self.progress / self.duration * 100:.1f}%"
        return "N/A"

    def to_dict(self) -> dict:
        return {
            '标题': self.title,
            'BV号': self.bvid,
            'UP主': self.author,
            '观看时间': self.view_time.strftime("%Y-%m-%d %H:%M:%S"),
            '视频链接': self.link,
            '总时长(秒)': self.duration,
            '已观看(秒)': self.progress,
            '完成度': self.completion,
            '分类': self.category,
            '类型': self.content_type.value
        }


@dataclass
class LiveRecord:
    """直播记录"""
    title: str
    room_id: str
    author: str
    view_time: datetime
    link: str = ""
    category: str = ""  # 直播状态/分类，如 直播中、未开播、电台
    content_type: ContentType = field(default=ContentType.LIVE)

    def get_unique_key(self) -> str:
        return f"live_{self.room_id}"

    def to_dict(self) -> dict:
        return {
            '标题': self.title,
            '房间号': self.room_id,
            '主播': self.author,
            '观看时间': self.view_time.strftime("%Y-%m-%d %H:%M:%S"),
            '直播链接': self.link,
            '分类': self.category,
            '类型': self.content_type.value
        }


@dataclass
class ArticleRecord:
    """专栏记录"""
    title: str
    article_id: str
    author: str
    view_time: datetime
    link: str = ""
    category: str = ""  # 专栏分类，如 数码、游戏
    content_type: ContentType = field(default=ContentType.ARTICLE)

    def get_unique_key(self) -> str:
        return f"article_{self.article_id}"

    def to_dict(self) -> dict:
        return {
            '标题': self.title,
            '文章ID': self.article_id,
            '作者': self.author,
            '观看时间': self.view_time.strftime("%Y-%m-%d %H:%M:%S"),
            '专栏链接': self.link,
            '分类': self.category,
            '类型': self.content_type.value
        }


@dataclass
class HistoryConfig:
    """应用配置"""
    csv_file: str
    cookie: str
    page_size: int = 30  # 新版游标接口单页上限为 30
    fetch_all: bool = True
    incremental_update: bool = True
    user_agents: list = field(default_factory=lambda: [
        "Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/120.0.0.0 Safari/537.36"
    ])
    history_api: str = "https://api.bilibili.com/x/web-interface/history/cursor"
    followings_api: str = "https://api.bilibili.com/x/relation/followings"
    fav_folder_api: str = "https://api.bilibili.com/x/v3/fav/folder/list"
    fav_resource_api: str = "https://api.bilibili.com/x/v3/fav/resource/list"
    user_card_api: str = "https://api.bilibili.com/x/web-interface/card"
    max_retries: int = 3
    retry_wait: int = 2
    http_total_retries: int = 3
    http_backoff_factor: float = 1.0


@dataclass
class FollowingRecord:
    """关注 UP 主记录"""
    mid: str
    name: str
    face: str = ""  # 头像 URL
    sign: str = ""  # 签名
    official: str = ""  # 认证信息
    level: int = 0  # 等级
    link: str = ""

    def get_unique_key(self) -> str:
        return f"following_{self.mid}"


@dataclass
class FavFolderRecord:
    """收藏夹文件夹"""
    folder_id: str
    title: str
    media_count: int = 0
    link: str = ""


@dataclass
class FavResourceRecord:
    """收藏夹内的资源（视频/专栏等）"""
    title: str
    bvid: str = ""
    link: str = ""
    author: str = ""
    cover: str = ""
    resource_type: str = "video"  # video / article / etc.


# 类型别名
HistoryRecord = VideoRecord | LiveRecord | ArticleRecord
