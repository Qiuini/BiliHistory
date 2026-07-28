"""图片异步加载与缓存测试"""
import io
from pathlib import Path
from unittest.mock import MagicMock

import pytest

from PyQt6.QtGui import QPixmap

from gui.image_loader import ImageLoader, _url_to_key


def _png_bytes() -> bytes:
    """生成一个 1x1 像素的 PNG 数据"""
    try:
        from PIL import Image
        buf = io.BytesIO()
        Image.new("RGB", (1, 1), color=(255, 0, 0)).save(buf, format="PNG")
        return buf.getvalue()
    except Exception as exc:
        raise RuntimeError("测试需要 Pillow 生成 PNG 数据") from exc


@pytest.fixture
def loader(qtbot, tmp_path, monkeypatch):
    """提供已重置单例状态、使用临时磁盘缓存的 ImageLoader"""
    ImageLoader._instance = None
    ImageLoader._initialized = False
    monkeypatch.setattr(
        "gui.image_loader._image_cache_dir", lambda: tmp_path / "image_cache"
    )
    instance = ImageLoader(disk_cache=True)
    instance.clear_memory()
    instance.clear_disk()
    yield instance
    instance.cleanup()
    ImageLoader._instance = None
    ImageLoader._initialized = False


def test_url_to_key_deterministic():
    assert _url_to_key("http://example.com/a.png") == _url_to_key("http://example.com/a.png")
    assert _url_to_key("http://example.com/a.png") != _url_to_key("http://example.com/b.png")


def test_empty_url_is_noop(loader):
    loader.load("")
    assert len(loader._tasks) == 0
    assert len(loader._memory) == 0


def test_memory_cache_hit_does_not_fetch(loader, qtbot, monkeypatch):
    called = False

    def fake_get(*args, **kwargs):
        nonlocal called
        called = True
        return MagicMock(content=_png_bytes(), raise_for_status=lambda: None)

    monkeypatch.setattr("gui.image_loader.requests.get", fake_get)

    url = "http://example.com/avatar.png"
    received = []
    with qtbot.waitSignal(loader.loaded, timeout=2000):
        loader.load(url, callback=lambda pixmap: received.append(pixmap))

    assert len(received) == 1
    assert url in loader._memory

    # 再次加载应直接命中内存缓存，不发起请求
    called = False
    second = []
    loader.load(url, callback=lambda pixmap: second.append(pixmap))
    assert called is False
    assert len(second) == 1


def test_disk_cache_restored_into_memory(loader, monkeypatch):
    url = "http://example.com/disk.png"
    data = _png_bytes()

    # 预置磁盘缓存文件
    cache_path = loader._cache_path(url)
    cache_path.parent.mkdir(parents=True, exist_ok=True)
    cache_path.write_bytes(data)

    def should_not_fetch(*args, **kwargs):
        raise AssertionError("不应发起网络请求")

    monkeypatch.setattr("gui.image_loader.requests.get", should_not_fetch)

    result = []
    loader.load(url, callback=lambda pixmap: result.append(pixmap))
    assert len(result) == 1
    assert url in loader._memory


def test_lru_eviction(loader, qtbot, monkeypatch):
    monkeypatch.setattr(
        "gui.image_loader.requests.get",
        lambda *args, **kwargs: MagicMock(content=_png_bytes(), raise_for_status=lambda: None),
    )

    loader.MAX_MEMORY_CACHE = 3
    urls = [f"http://example.com/{i}.png" for i in range(5)]
    for url in urls:
        with qtbot.waitSignal(loader.loaded, timeout=2000):
            loader.load(url)

    assert len(loader._memory) == 3
    # 最早加入的 2 个应被淘汰
    assert urls[0] not in loader._memory
    assert urls[1] not in loader._memory
    assert urls[2] in loader._memory
    assert urls[3] in loader._memory
    assert urls[4] in loader._memory


def test_request_deduplication(loader, qtbot, monkeypatch):
    call_count = 0

    def fake_get(*args, **kwargs):
        nonlocal call_count
        call_count += 1
        return MagicMock(content=_png_bytes(), raise_for_status=lambda: None)

    monkeypatch.setattr("gui.image_loader.requests.get", fake_get)

    url = "http://example.com/dedup.png"
    results = [[], []]
    with qtbot.waitSignal(loader.loaded, timeout=2000):
        loader.load(url, callback=lambda pixmap, idx=0: results[0].append(pixmap))
        loader.load(url, callback=lambda pixmap, idx=1: results[1].append(pixmap))

    assert call_count == 1
    assert len(results[0]) == 1
    assert len(results[1]) == 1


def test_failure_does_not_crash(loader, qtbot, monkeypatch):
    def fake_get(*args, **kwargs):
        raise Exception("network error")

    monkeypatch.setattr("gui.image_loader.requests.get", fake_get)

    url = "http://example.com/fail.png"
    with qtbot.waitSignal(loader.failed, timeout=2000):
        loader.load(url, callback=lambda pixmap: pytest.fail("失败不应回调"))

    assert url not in loader._tasks
    assert url not in loader._memory
