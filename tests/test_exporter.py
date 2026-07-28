"""导出模块测试"""
import csv
import json
from pathlib import Path

import pytest

from exporter import (
    export_csv, export_json, export_html, export_markdown,
    export_records, guess_format, supported_filters, EXPORT_FORMATS,
)
from exceptions import DataError


@pytest.fixture
def sample_records():
    return [
        {"标题": "视频A", "BV号": "BV1", "UP主": "UP1", "类型": "video", "观看时间": "2024-06-01 10:00:00"},
        {"标题": "直播B", "房间号": "123", "主播": "主播1", "类型": "live", "观看时间": "2024-06-02 11:00:00"},
    ]


def test_guess_format():
    assert guess_format("a.csv") == ".csv"
    assert guess_format("a.JSON") == ".json"
    assert guess_format("a.html") == ".html"
    assert guess_format("a.md") == ".md"
    assert guess_format("a.exe") == ""


def test_supported_filters_contains_all_formats():
    filters = supported_filters()
    for ext in EXPORT_FORMATS:
        assert ext in filters


def test_export_csv(tmp_path, sample_records):
    path = tmp_path / "out.csv"
    export_csv(sample_records, str(path))

    with open(path, "r", encoding="utf-8-sig") as f:
        reader = csv.DictReader(f)
        rows = list(reader)

    assert len(rows) == 2
    assert rows[0]["标题"] == "视频A"
    assert rows[1]["房间号"] == "123"


def test_export_json(tmp_path, sample_records):
    path = tmp_path / "out.json"
    export_json(sample_records, str(path))

    with open(path, "r", encoding="utf-8") as f:
        data = json.load(f)

    assert data["count"] == 2
    assert len(data["records"]) == 2
    assert "exported_at" in data


def test_export_html(tmp_path, sample_records):
    path = tmp_path / "out.html"
    export_html(sample_records, str(path))

    content = path.read_text(encoding="utf-8")
    assert "<!DOCTYPE html>" in content
    assert "视频A" in content
    assert "主播1" in content
    assert "<table>" in content


def test_export_markdown(tmp_path, sample_records):
    path = tmp_path / "out.md"
    export_markdown(sample_records, str(path))

    content = path.read_text(encoding="utf-8")
    assert "# BiliHistory 历史记录" in content
    assert "视频A" in content
    assert "主播1" in content


def test_export_records_dispatches_by_extension(tmp_path, sample_records):
    path = tmp_path / "out.json"
    export_records(sample_records, str(path))
    with open(path, "r", encoding="utf-8") as f:
        assert json.load(f)["count"] == 2


def test_export_empty_records_raises():
    with pytest.raises(DataError):
        export_csv([], "any.csv")


def test_export_unsupported_format_raises(sample_records):
    with pytest.raises(DataError):
        export_records(sample_records, "out.exe")
