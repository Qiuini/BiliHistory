"""
头像/封面图片异步加载与缓存

设计：
- 内存 LRU 缓存（默认最多 200 张），避免同一会话重复下载/解码
- 磁盘缓存落到平台用户数据目录，复用已下载图片
- 同一 URL 的并发请求会被合并，只触发一次后台下载
- 失败/空 URL 静默忽略，不阻塞 UI
"""
import hashlib
import os
import threading
from collections import OrderedDict
from pathlib import Path
from typing import Callable, Optional

import requests
from PyQt6.QtCore import QObject, QThread, pyqtSignal, Qt
from PyQt6.QtGui import QPixmap

from logger import logger


def _image_cache_dir() -> Path:
    """返回图片磁盘缓存目录"""
    try:
        import paths
        base = paths.app_data_dir() / "cache" / "images"
    except Exception:
        base = Path.home() / ".bilihistory_cache" / "images"
    base.mkdir(parents=True, exist_ok=True)
    return base


def _url_to_key(url: str) -> str:
    return hashlib.sha256(url.encode("utf-8")).hexdigest()


def _touch(path: Path) -> None:
    try:
        os.utime(path, None)
    except Exception:
        pass


class _DownloadTask(QThread):
    """后台下载图片线程"""

    finished = pyqtSignal(str, bytes)
    failed = pyqtSignal(str, str)

    def __init__(self, url: str, cache_path: Optional[Path] = None, parent=None):
        super().__init__(parent)
        self.url = url
        self.cache_path = cache_path

    def run(self):
        try:
            resp = requests.get(
                self.url,
                timeout=8,
                headers={
                    "User-Agent": (
                        "Mozilla/5.0 (Windows NT 10.0; Win64; x64) "
                        "AppleWebKit/537.36 (KHTML, like Gecko) "
                        "Chrome/120.0.0.0 Safari/537.36"
                    ),
                    "Referer": "https://www.bilibili.com/",
                },
            )
            resp.raise_for_status()
            data = resp.content
            if self.cache_path:
                try:
                    self.cache_path.parent.mkdir(parents=True, exist_ok=True)
                    self.cache_path.write_bytes(data)
                except Exception as e:
                    logger.warning(f"图片磁盘缓存写入失败: {e}")
            self.finished.emit(self.url, data)
        except Exception as e:
            self.failed.emit(self.url, str(e))


class ImageLoader(QObject):
    """图片异步加载器：内存 LRU + 磁盘缓存 + 请求合并"""

    MAX_MEMORY_CACHE = 200

    loaded = pyqtSignal(str, QPixmap)
    failed = pyqtSignal(str, str)

    _instance: Optional["ImageLoader"] = None
    _lock = threading.Lock()

    def __new__(cls, *args, **kwargs):
        if cls._instance is None:
            with cls._lock:
                if cls._instance is None:
                    cls._instance = super().__new__(cls)
                    cls._instance._initialized = False
        return cls._instance

    def __init__(self, parent=None, disk_cache: bool = True):
        super().__init__(parent)
        if self._initialized:
            return
        self._initialized = True

        self._disk_cache = disk_cache
        self._memory: OrderedDict[str, QPixmap] = OrderedDict()
        self._tasks: dict[str, _DownloadTask] = {}
        self._pending_callbacks: dict[str, list[Callable[[QPixmap], None]]] = {}
        self._cache_dir = _image_cache_dir()

    def _cache_path(self, url: str) -> Path:
        return self._cache_dir / _url_to_key(url)

    def _put_memory(self, url: str, pixmap: QPixmap) -> None:
        if url in self._memory:
            self._memory.move_to_end(url)
            return
        while len(self._memory) >= self.MAX_MEMORY_CACHE:
            self._memory.popitem(last=False)
        self._memory[url] = pixmap
        self._memory.move_to_end(url)

    def _load_from_disk(self, url: str) -> Optional[QPixmap]:
        if not self._disk_cache:
            return None
        path = self._cache_path(url)
        if not path.exists():
            return None
        try:
            pixmap = QPixmap(str(path))
            if not pixmap.isNull():
                _touch(path)
                return pixmap
        except Exception as e:
            logger.warning(f"图片磁盘缓存读取失败: {e}")
        return None

    def get(self, url: str) -> Optional[QPixmap]:
        """同步读取内存或磁盘缓存（不触发网络请求）"""
        if not url:
            return None
        if url in self._memory:
            self._memory.move_to_end(url)
            return self._memory[url]
        pixmap = self._load_from_disk(url)
        if pixmap:
            self._put_memory(url, pixmap)
        return pixmap

    def load(
        self,
        url: str,
        callback: Optional[Callable[[QPixmap], None]] = None,
        size: Optional[tuple[int, int]] = None,
    ) -> None:
        """异步加载图片。命中缓存直接回调；否则后台下载并合并相同 URL 的请求。"""
        if not url:
            return

        pixmap = self.get(url)
        if pixmap:
            scaled = self._scale(pixmap, size)
            if callback:
                callback(scaled)
            return

        if callback:
            self._pending_callbacks.setdefault(url, []).append(callback)

        if url in self._tasks:
            return

        cache_path = self._cache_path(url) if self._disk_cache else None
        task = _DownloadTask(url, cache_path=cache_path, parent=self)
        task.finished.connect(self._on_downloaded)
        task.failed.connect(self._on_failed)
        self._tasks[url] = task
        task.start()

    @staticmethod
    def _scale(pixmap: QPixmap, size: Optional[tuple[int, int]]) -> QPixmap:
        if size is None:
            return pixmap
        return pixmap.scaled(
            size[0],
            size[1],
            Qt.AspectRatioMode.KeepAspectRatioByExpanding,
            Qt.TransformationMode.SmoothTransformation,
        )

    def _on_downloaded(self, url: str, data: bytes) -> None:
        self._tasks.pop(url, None)
        pixmap = QPixmap()
        if not pixmap.loadFromData(data):
            self._on_failed(url, "无法解析图片数据")
            return

        self._put_memory(url, pixmap)
        self.loaded.emit(url, pixmap)

        callbacks = self._pending_callbacks.pop(url, [])
        for cb in callbacks:
            try:
                cb(pixmap)
            except Exception as e:
                logger.warning(f"图片加载回调异常: {e}")

    def _on_failed(self, url: str, message: str) -> None:
        self._tasks.pop(url, None)
        self._pending_callbacks.pop(url, None)
        self.failed.emit(url, message)
        logger.debug(f"图片加载失败 [{url}]: {message}")

    def clear_memory(self) -> None:
        """清空内存缓存"""
        self._memory.clear()

    def clear_disk(self) -> None:
        """清空磁盘缓存"""
        if not self._cache_dir.exists():
            return
        try:
            for f in self._cache_dir.iterdir():
                f.unlink(missing_ok=True)
        except Exception as e:
            logger.warning(f"清理磁盘缓存失败: {e}")

    def cleanup(self) -> None:
        """释放所有资源（应用退出时调用）"""
        self.clear_memory()
        for task in list(self._tasks.values()):
            if task.isRunning():
                task.terminate()
        self._tasks.clear()
        self._pending_callbacks.clear()
