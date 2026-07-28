"""网络层测试"""
import time
from unittest.mock import MagicMock, patch

import pytest
import requests

from fetcher import HTTPClient, BilibiliFetcher, SLOW_REQUEST_THRESHOLD
from exceptions import APIError, CookieError, NetworkError, RetryExhaustedError


@pytest.fixture
def client(monkeypatch):
    """提供已移除真实网络请求的 HTTPClient"""
    with patch("fetcher.get_config") as mock_config:
        cfg = MagicMock()
        cfg.user_agents = ["TestAgent"]
        cfg.cookie = "test_cookie"
        cfg.http_total_retries = 0
        cfg.http_backoff_factor = 0.1
        cfg.max_retries = 2
        cfg.retry_wait = 0
        cfg.history_api = "https://api.test/history/cursor"
        cfg.page_size = 20
        cfg.fetch_all = False
        mock_config.return_value = cfg
        yield HTTPClient()


def test_get_success_logs_request(client, monkeypatch, caplog):
    """GET 成功应记录请求摘要"""
    fake_response = MagicMock()
    fake_response.status_code = 200
    fake_response.text = '{"code":0}'
    fake_response.raise_for_status = MagicMock()

    monkeypatch.setattr(client.session, "get", lambda *args, **kwargs: fake_response)

    with caplog.at_level("INFO"):
        resp = client.get("https://api.test/history")

    assert resp is fake_response
    assert "GET" in caplog.text
    assert "https://api.test/history" in caplog.text


def test_slow_request_warning(client, monkeypatch, caplog):
    """超过阈值的请求应发出慢请求告警"""
    class SlowResponse:
        status_code = 200
        text = '{"code":0}'

        def raise_for_status(self):
            pass

    def slow_get(*args, **kwargs):
        time.sleep(SLOW_REQUEST_THRESHOLD + 0.05)
        return SlowResponse()

    monkeypatch.setattr(client.session, "get", slow_get)

    with caplog.at_level("WARNING"):
        client.get("https://api.test/slow")

    assert "慢请求" in caplog.text


def test_http_error_401_raises_cookie_error(client, monkeypatch):
    """401/403 应转换为 CookieError"""

    def raise_http_error(*args, **kwargs):
        resp = MagicMock()
        resp.status_code = 401
        raise requests.exceptions.HTTPError(response=resp)

    monkeypatch.setattr(client.session, "get", raise_http_error)

    with pytest.raises(CookieError):
        client.get("https://api.test/protected")


def test_connection_error_raises_network_error(client, monkeypatch):
    def raise_connection(*args, **kwargs):
        raise requests.exceptions.ConnectionError("boom")

    monkeypatch.setattr(client.session, "get", raise_connection)
    with pytest.raises(NetworkError):
        client.get("https://api.test/down")


def test_timeout_raises_network_error(client, monkeypatch):
    def raise_timeout(*args, **kwargs):
        raise requests.exceptions.Timeout("timeout")

    monkeypatch.setattr(client.session, "get", raise_timeout)
    with pytest.raises(NetworkError):
        client.get("https://api.test/timeout")


def test_fetch_page_returns_data_on_success(client, monkeypatch):
    """fetch_page 在 API 返回 code=0 时解析 data"""
    fake_response = MagicMock()
    fake_response.status_code = 200
    fake_response.json.return_value = {"code": 0, "data": {"list": [1, 2]}}
    fake_response.raise_for_status = MagicMock()

    monkeypatch.setattr(client, "get", lambda *args, **kwargs: fake_response)

    fetcher = BilibiliFetcher()
    fetcher.client = client
    result = fetcher.fetch_page()
    assert result == {"list": [1, 2]}


def test_fetch_page_raises_cookie_error(client, monkeypatch):
    fake_response = MagicMock()
    fake_response.status_code = 200
    fake_response.json.return_value = {"code": -101, "message": "未登录"}
    fake_response.raise_for_status = MagicMock()

    monkeypatch.setattr(client, "get", lambda *args, **kwargs: fake_response)

    fetcher = BilibiliFetcher()
    fetcher.client = client
    with pytest.raises(CookieError):
        fetcher.fetch_page()


def test_fetch_page_retry_exhausted(client, monkeypatch):
    """重试耗尽后抛出 RetryExhaustedError"""
    def raise_network(*args, **kwargs):
        raise NetworkError("boom")

    monkeypatch.setattr(client, "get", raise_network)

    fetcher = BilibiliFetcher()
    fetcher.client = client
    fetcher.max_retries = 2
    fetcher.retry_wait = 0

    with pytest.raises(RetryExhaustedError):
        fetcher.fetch_page()
