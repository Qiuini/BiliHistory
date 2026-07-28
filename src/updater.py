"""
自动更新检查 - 查询 GitHub Releases 最新版本

提供非阻塞的更新检查：
- 在后台线程中请求 GitHub API
- 解析语义化版本号并比较
- 返回新版本信息或错误说明
"""
import re
import threading
from typing import NamedTuple, Optional

import requests

from logger import logger


class UpdateInfo(NamedTuple):
    """更新检查结果"""
    has_update: bool
    current_version: str
    latest_version: str
    release_url: str
    release_notes: str
    error: str = ""


GITHUB_RELEASES_API = "https://api.github.com/repos/{owner}/{repo}/releases/latest"


def parse_version(version: str) -> tuple[int, ...]:
    """将版本字符串解析为可比较的整数元组，忽略 v 前缀"""
    cleaned = version.lower().lstrip("v").strip()
    parts = re.split(r"[.-]", cleaned)
    numbers = []
    for part in parts:
        try:
            numbers.append(int(part))
        except ValueError:
            break
    return tuple(numbers) if numbers else (0,)


def is_newer(current: str, latest: str) -> bool:
    """判断 latest 是否比 current 新"""
    return parse_version(latest) > parse_version(current)


def check_update(
    current_version: str,
    owner: str = "Qiuini",
    repo: str = "BiliHistory",
    timeout: int = 10,
) -> UpdateInfo:
    """同步检查 GitHub Releases 是否有新版本"""
    url = GITHUB_RELEASES_API.format(owner=owner, repo=repo)
    try:
        resp = requests.get(
            url,
            timeout=timeout,
            headers={
                "Accept": "application/vnd.github+json",
                "User-Agent": f"BiliHistory/{current_version}",
                "X-GitHub-Api-Version": "2022-11-28",
            },
        )
        resp.raise_for_status()
        data = resp.json()

        latest = data.get("tag_name", "").strip()
        release_url = data.get("html_url", "")
        release_notes = data.get("body", "") or ""

        if not latest:
            return UpdateInfo(
                has_update=False,
                current_version=current_version,
                latest_version="",
                release_url="",
                release_notes="",
                error="无法解析最新版本号",
            )

        has_update = is_newer(current_version, latest)
        return UpdateInfo(
            has_update=has_update,
            current_version=current_version,
            latest_version=latest,
            release_url=release_url,
            release_notes=release_notes,
        )

    except requests.exceptions.HTTPError as e:
        status = e.response.status_code if e.response else 0
        if status == 404:
            return UpdateInfo(
                False, current_version, "", "", "", "未找到发布页面（仓库可能没有 Release）"
            )
        return UpdateInfo(False, current_version, "", "", "", f"GitHub API 错误 ({status})")
    except requests.exceptions.Timeout:
        return UpdateInfo(False, current_version, "", "", "", "检查更新超时")
    except requests.exceptions.RequestException as e:
        return UpdateInfo(False, current_version, "", "", "", f"网络请求失败: {e}")
    except Exception as e:
        return UpdateInfo(False, current_version, "", "", "", f"检查更新异常: {e}")


class UpdateChecker(threading.Thread):
    """后台更新检查线程"""

    def __init__(
        self,
        current_version: str,
        owner: str = "Qiuini",
        repo: str = "BiliHistory",
        callback=None,
    ):
        super().__init__(daemon=True)
        self.current_version = current_version
        self.owner = owner
        self.repo = repo
        self.callback = callback

    def run(self):
        info = check_update(self.current_version, self.owner, self.repo)
        if self.callback:
            try:
                self.callback(info)
            except Exception as e:
                logger.warning(f"更新检查回调异常: {e}")
