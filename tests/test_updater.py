"""自动更新检查测试"""
from unittest.mock import MagicMock, patch

import pytest
import requests

from updater import parse_version, is_newer, check_update, UpdateChecker


def test_parse_version_strips_v_prefix():
    assert parse_version("v1.2.3") == (1, 2, 3)
    assert parse_version("1.2.3") == (1, 2, 3)
    assert parse_version("1.2.3-beta") == (1, 2, 3)
    assert parse_version("2.0") == (2, 0)
    assert parse_version("") == (0,)


def test_is_newer():
    assert is_newer("1.0.0", "1.0.1") is True
    assert is_newer("1.0.0", "1.1.0") is True
    assert is_newer("1.0.0", "2.0.0") is True
    assert is_newer("1.0.0", "1.0.0") is False
    assert is_newer("1.0.1", "1.0.0") is False
    assert is_newer("v1.0.0", "v1.0.1") is True


def test_check_update_finds_new_version(monkeypatch):
    fake_response = MagicMock()
    fake_response.json.return_value = {
        "tag_name": "v1.1.0",
        "html_url": "https://github.com/Qiuini/BiliHistory/releases/tag/v1.1.0",
        "body": "修复了一些问题",
    }
    fake_response.raise_for_status = MagicMock()
    monkeypatch.setattr("updater.requests.get", lambda *args, **kwargs: fake_response)

    info = check_update("1.0.0")
    assert info.has_update is True
    assert info.latest_version == "v1.1.0"
    assert info.release_url == "https://github.com/Qiuini/BiliHistory/releases/tag/v1.1.0"
    assert info.error == ""


def test_check_update_no_new_version(monkeypatch):
    fake_response = MagicMock()
    fake_response.json.return_value = {
        "tag_name": "v1.0.0",
        "html_url": "https://example.com",
        "body": "",
    }
    fake_response.raise_for_status = MagicMock()
    monkeypatch.setattr("updater.requests.get", lambda *args, **kwargs: fake_response)

    info = check_update("1.0.0")
    assert info.has_update is False
    assert info.latest_version == "v1.0.0"


def test_check_update_handles_http_404(monkeypatch):
    def raise_404(*args, **kwargs):
        resp = MagicMock()
        resp.status_code = 404
        raise requests.exceptions.HTTPError(response=resp)

    monkeypatch.setattr("updater.requests.get", raise_404)
    info = check_update("1.0.0")
    assert info.has_update is False
    assert "未找到发布页面" in info.error


def test_check_update_handles_timeout(monkeypatch):
    def raise_timeout(*args, **kwargs):
        raise requests.exceptions.Timeout("timeout")

    monkeypatch.setattr("updater.requests.get", raise_timeout)
    info = check_update("1.0.0")
    assert info.has_update is False
    assert "超时" in info.error


def test_update_checker_runs_callback(monkeypatch):
    fake_response = MagicMock()
    fake_response.json.return_value = {
        "tag_name": "v1.2.0",
        "html_url": "https://example.com",
        "body": "",
    }
    fake_response.raise_for_status = MagicMock()
    monkeypatch.setattr("updater.requests.get", lambda *args, **kwargs: fake_response)

    results = []
    checker = UpdateChecker("1.0.0", callback=lambda info: results.append(info))
    checker.start()
    checker.join(timeout=2)

    # 线程已启动（daemon），join 等待完成
    assert len(results) == 1
    assert results[0].has_update is True
    assert results[0].latest_version == "v1.2.0"
