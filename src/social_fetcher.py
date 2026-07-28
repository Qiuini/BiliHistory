"""
社交/收藏数据获取层 - 关注列表、收藏夹、用户信息
"""
import random
import time
from datetime import datetime
from typing import List, Optional

from config import get_config
from exceptions import APIError, CookieError, NetworkError, RetryExhaustedError
from fetcher import HTTPClient
from logger import logger
from parser import FollowingParser, FavFolderParser, FavResourceParser, UserInfoParser


class FollowingFetcher:
    """B站关注列表获取器"""

    def __init__(self):
        self.config = get_config()
        self.client = HTTPClient()
        self.parser = FollowingParser()
        self.max_retries = self.config.max_retries
        self.retry_wait = self.config.retry_wait

    def fetch_all(self, vmid: Optional[str] = None) -> List:
        """获取全部关注列表（pn/ps 分页）"""
        if not self.config.cookie:
            raise CookieError("未设置 Cookie，无法抓取关注列表")

        page_size = 50
        page = 1
        all_records = []

        while True:
            url = (f"{self.config.followings_api}"
                   f"?vmid={vmid or ''}&pn={page}&ps={page_size}&order=desc&order_type=attention")

            data = self._fetch_with_retry(url)
            if data is None:
                break

            records = self.parser.parse(data)
            if not records:
                break

            all_records.extend(records)
            logger.info(f"第 {page} 页获取 {len(records)} 位关注（累计 {len(all_records)} 位）")

            total = data.get("data", {}).get("total", 0)
            if page * page_size >= total:
                break

            page += 1
            time.sleep(0.5 + random.uniform(0, 0.5))

        return all_records

    def _fetch_with_retry(self, url: str) -> Optional[dict]:
        for attempt in range(1, self.max_retries + 1):
            try:
                response = self.client.get(url)
                data = response.json()
                if data.get("code") == 0:
                    return data
                elif data.get("code") == -101:
                    raise CookieError()
                else:
                    logger.warning(f"API返回错误: {data.get('message', '未知错误')}")
                    return None
            except CookieError:
                raise
            except (APIError, NetworkError) as e:
                if attempt < self.max_retries:
                    logger.warning(f"{e}，{self.retry_wait}秒后重试...")
                    time.sleep(self.retry_wait)
                else:
                    logger.error(f"重试次数耗尽: {e}")
                    raise RetryExhaustedError(f"获取失败: {url}")
        return None

    def close(self):
        self.client.close()


class UserInfoFetcher:
    """B站用户信息获取器（注册时间等）"""

    def __init__(self):
        self.config = get_config()
        self.client = HTTPClient()
        self.parser = UserInfoParser()
        self.max_retries = self.config.max_retries
        self.retry_wait = self.config.retry_wait

    def fetch_registration_time(self, mid: str) -> Optional[datetime]:
        """查询指定用户的注册时间

        Args:
            mid: 用户 mid

        Returns:
            注册时间 datetime，失败返回 None
        """
        if not mid:
            return None

        url = f"{self.config.user_card_api}?mid={mid}&photo=false"
        data = self._fetch_with_retry(url)
        if data is None:
            return None

        return self.parser.parse_registration_time(data)

    def _fetch_with_retry(self, url: str) -> Optional[dict]:
        for attempt in range(1, self.max_retries + 1):
            try:
                response = self.client.get(url)
                data = response.json()
                if data.get("code") == 0:
                    return data
                elif data.get("code") == -101:
                    raise CookieError()
                else:
                    logger.warning(f"API返回错误: {data.get('message', '未知错误')}")
                    return None
            except CookieError:
                raise
            except (APIError, NetworkError) as e:
                if attempt < self.max_retries:
                    logger.warning(f"{e}，{self.retry_wait}秒后重试...")
                    time.sleep(self.retry_wait)
                else:
                    logger.error(f"重试次数耗尽: {e}")
                    raise RetryExhaustedError(f"获取失败: {url}")
        return None

    def close(self):
        self.client.close()


class FavoritesFetcher:
    """B站收藏夹获取器"""

    def __init__(self):
        self.config = get_config()
        self.client = HTTPClient()
        self.folder_parser = FavFolderParser()
        self.resource_parser = FavResourceParser()
        self.max_retries = self.config.max_retries
        self.retry_wait = self.config.retry_wait

    def fetch_folders(self) -> List:
        """获取收藏夹文件夹列表"""
        if not self.config.cookie:
            raise CookieError("未设置 Cookie，无法抓取收藏夹")

        url = f"{self.config.fav_folder_api}?up_mid=0"
        data = self._fetch_with_retry(url)
        if data is None:
            return []
        return self.folder_parser.parse(data)

    def fetch_resources(self, folder_id: str, page_size: int = 20) -> List:
        """获取指定收藏夹内的资源（分页）"""
        page = 1
        all_records = []

        while True:
            url = (f"{self.config.fav_resource_api}"
                   f"?media_id={folder_id}&pn={page}&ps={page_size}&platform=web")

            data = self._fetch_with_retry(url)
            if data is None:
                break

            records = self.resource_parser.parse(data)
            if not records:
                break

            all_records.extend(records)
            logger.info(f"收藏夹 {folder_id} 第 {page} 页获取 {len(records)} 条")

            info = data.get("data", {}).get("info", {})
            total = info.get("media_count", 0)
            if page * page_size >= total:
                break

            page += 1
            time.sleep(0.5 + random.uniform(0, 0.5))

        return all_records

    def _fetch_with_retry(self, url: str) -> Optional[dict]:
        for attempt in range(1, self.max_retries + 1):
            try:
                response = self.client.get(url)
                data = response.json()
                if data.get("code") == 0:
                    return data
                elif data.get("code") == -101:
                    raise CookieError()
                else:
                    logger.warning(f"API返回错误: {data.get('message', '未知错误')}")
                    return None
            except CookieError:
                raise
            except (APIError, NetworkError) as e:
                if attempt < self.max_retries:
                    logger.warning(f"{e}，{self.retry_wait}秒后重试...")
                    time.sleep(self.retry_wait)
                else:
                    logger.error(f"重试次数耗尽: {e}")
                    raise RetryExhaustedError(f"获取失败: {url}")
        return None

    def close(self):
        self.client.close()
