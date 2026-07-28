"""
HTTP 请求层 - 独立封装网络请求逻辑
"""
import random
import threading
import time
from typing import Optional

import requests
from requests.adapters import HTTPAdapter
from urllib3.util.retry import Retry

from config import get_config
from exceptions import APIError, CookieError, NetworkError, RetryExhaustedError
from logger import logger


class HTTPClient:
    """HTTP 客户端"""

    def __init__(self):
        self.config = get_config()
        self.session = self._create_session()

    def _create_session(self) -> requests.Session:
        """创建带重试策略的 Session"""
        session = requests.Session()

        retry_strategy = Retry(
            total=self.config.http_total_retries,
            backoff_factor=self.config.http_backoff_factor,
            status_forcelist=[429, 500, 502, 503, 504]
        )

        adapter = HTTPAdapter(max_retries=retry_strategy)
        session.mount("https://", adapter)
        session.mount("http://", adapter)

        return session

    def _get_headers(self) -> dict:
        """构建请求头"""
        return {
            "User-Agent": random.choice(self.config.user_agents),
            "Cookie": self.config.cookie,
            "Accept": "application/json, text/plain, */*",
            "Referer": "https://www.bilibili.com/"
        }

    def get(self, url: str, **kwargs) -> requests.Response:
        """发送 GET 请求"""
        headers = self._get_headers()
        headers.update(kwargs.pop('headers', {}))

        try:
            response = self.session.get(url, headers=headers, timeout=10, **kwargs)
            response.raise_for_status()
            return response
        except requests.exceptions.HTTPError as e:
            status_code = e.response.status_code
            if status_code == 401 or status_code == 403:
                raise CookieError("Cookie无效或已过期，请更新 Cookie")
            raise APIError(str(e), status_code=status_code)
        except requests.exceptions.ConnectionError as e:
            raise NetworkError(f"连接失败: {e}")
        except requests.exceptions.Timeout as e:
            raise NetworkError(f"请求超时: {e}")
        except Exception as e:
            raise NetworkError(f"请求异常: {e}")

    def close(self):
        """关闭会话"""
        self.session.close()


class BilibiliFetcher:
    """B站历史记录获取器"""

    def __init__(self, cancel_event: Optional[threading.Event] = None):
        self.config = get_config()
        self.client = HTTPClient()
        self.max_retries = self.config.max_retries
        self.retry_wait = self.config.retry_wait
        self._cancel_event = cancel_event

    def fetch_page(self, max_oid: int = 0, view_at: int = 0, business: str = "") -> Optional[dict]:
        """获取单页历史记录（游标分页）

        Args:
            max_oid: 游标起始目标 ID（上一页 cursor.max），首页传 0
            view_at: 游标起始时间戳（上一页 cursor.view_at），首页传 0
            business: 游标业务类型（上一页 cursor.business），首页传空

        Returns:
            data 字段字典（含 cursor 和 list），失败返回 None
        """
        page_size = min(self.config.page_size, 30)  # 接口单页上限 30
        url = (f"{self.config.history_api}"
               f"?max={max_oid}&view_at={view_at}&business={business}&ps={page_size}")

        for attempt in range(1, self.max_retries + 1):
            try:
                logger.info(f"获取游标 max={max_oid} view_at={view_at}（第 {attempt}/{self.max_retries} 次尝试）...")
                response = self.client.get(url)
                data = response.json()

                if data.get("code") == 0:
                    return data.get("data", {})
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
                    raise RetryExhaustedError(f"获取游标 max={max_oid} 处的页面失败")

        return None

    def fetch_all(self) -> list:
        """获取所有历史记录（沿游标翻页直到结束）"""
        if not self.config.cookie:
            raise CookieError(
                "未设置 Cookie，无法抓取。请在设置中填入 Cookie，"
                "或设置环境变量 BILI_COOKIE / .secrets.json"
            )

        all_records = []
        max_oid, view_at, business = 0, 0, ""
        page = 1

        while True:
            data = self.fetch_page(max_oid, view_at, business)

            if data is None:
                # API 逻辑错误（非网络问题），游标无法推进，只能终止
                logger.error("接口返回异常，停止获取")
                break

            history_list = data.get("list") or []
            current_count = len(history_list)

            if current_count == 0:
                logger.info("已到达最后一页")
                break

            all_records.extend(history_list)
            logger.info(f"第 {page} 页获取 {current_count} 条记录（累计 {len(all_records)} 条）")

            if not self.config.fetch_all:
                logger.info("调试模式：仅获取第一页")
                break

            # 推进游标
            cursor = data.get("cursor") or {}
            max_oid = cursor.get("max", 0)
            view_at = cursor.get("view_at", 0)
            business = cursor.get("business", "")

            if max_oid == 0 and view_at == 0:
                logger.info("游标已结束，获取完成")
                break

            page += 1
            if self._cancel_event and self._cancel_event.is_set():
                logger.info("用户取消抓取")
                break
            time.sleep(1.5 + random.uniform(0, 1.5))
            if self._cancel_event and self._cancel_event.is_set():
                logger.info("用户取消抓取")
                break

        return all_records

    def close(self):
        """关闭客户端"""
        self.client.close()

    def __enter__(self):
        return self

    def __exit__(self, exc_type, exc_val, exc_tb):
        self.close()
