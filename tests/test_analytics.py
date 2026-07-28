"""本地统计分析测试"""
import os
import sys

sys.path.insert(0, os.path.dirname(os.path.dirname(os.path.abspath(__file__))))

from analytics import (
    compute_basic_stats,
    top_authors,
    top_categories,
    time_of_day_distribution,
    top_authors_by_completion,
    daily_trend,
)


def _make_video(title, author, category, view_at, duration, progress):
    return {
        "标题": title,
        "BV号": "BV1test",
        "UP主": author,
        "分类": category,
        "观看时间": view_at,
        "总时长(秒)": duration,
        "已观看(秒)": progress,
        "类型": "video",
    }


def _make_live(author, view_at):
    return {
        "标题": "直播标题",
        "房间号": "123",
        "主播": author,
        "分类": "未开播",
        "观看时间": view_at,
        "类型": "live",
    }


def _make_article(author, category, view_at):
    return {
        "标题": "专栏标题",
        "文章ID": "456",
        "作者": author,
        "分类": category,
        "观看时间": view_at,
        "类型": "article",
    }


class TestBasicStats:
    def test_empty(self):
        stats = compute_basic_stats([])
        assert stats["total_records"] == 0
        assert stats["total_watch_time_text"] == "0分钟"

    def test_counts_and_watch_time(self):
        records = [
            _make_video("v1", "UP科技", "科技", "2024-06-20 10:00:00", 600, 300),
            _make_video("v2", "UP科技", "科技", "2024-06-20 11:00:00", 600, 600),
            _make_live("主播A", "2024-06-20 12:00:00"),
            _make_article("作者A", "数码", "2024-06-20 13:00:00"),
        ]
        stats = compute_basic_stats(records)
        assert stats["total_records"] == 4
        assert stats["total_videos"] == 2
        assert stats["total_lives"] == 1
        assert stats["total_articles"] == 1
        assert stats["unique_authors"] == 3
        assert stats["avg_completion"] == "75.0%"


class TestTopAuthors:
    def test_order_by_count(self):
        records = [
            _make_video("v1", "A", "科技", "2024-06-20 10:00:00", 600, 600),
            _make_video("v2", "A", "科技", "2024-06-20 11:00:00", 600, 600),
            _make_video("v3", "B", "科技", "2024-06-20 12:00:00", 600, 600),
        ]
        authors = top_authors(records)
        assert authors[0]["name"] == "A"
        assert authors[0]["count"] == 2
        assert authors[1]["name"] == "B"


class TestTopCategories:
    def test_group_by_category(self):
        records = [
            _make_video("v1", "A", "音乐", "2024-06-20 10:00:00", 600, 600),
            _make_video("v2", "B", "音乐", "2024-06-20 11:00:00", 600, 600),
            _make_video("v3", "C", "科技", "2024-06-20 12:00:00", 600, 600),
        ]
        cats = top_categories(records)
        assert cats[0]["name"] == "音乐"
        assert cats[0]["count"] == 2
        assert cats[1]["name"] == "科技"


class TestTimeOfDay:
    def test_buckets(self):
        records = [
            _make_video("v1", "A", "音乐", "2024-06-20 02:00:00", 600, 600),
            _make_video("v2", "B", "音乐", "2024-06-20 08:00:00", 600, 600),
            _make_video("v3", "C", "音乐", "2024-06-20 14:00:00", 600, 600),
            _make_video("v4", "D", "音乐", "2024-06-20 20:00:00", 600, 600),
        ]
        dist = time_of_day_distribution(records)
        assert dist == {"凌晨": 1, "上午": 1, "下午": 1, "晚上": 1}


class TestCompletion:
    def test_min_count_filter(self):
        records = [
            _make_video("v1", "A", "音乐", "2024-06-20 10:00:00", 600, 600),
            _make_video("v2", "A", "音乐", "2024-06-20 11:00:00", 600, 600),
            _make_video("v3", "B", "音乐", "2024-06-20 12:00:00", 600, 600),
        ]
        result = top_authors_by_completion(records, min_count=2)
        assert len(result) == 1
        assert result[0]["name"] == "A"


class TestDailyTrend:
    def test_recent_days(self):
        from datetime import datetime, timedelta
        today = datetime.now()
        records = [
            _make_video("v1", "A", "音乐", today.strftime("%Y-%m-%d %H:%M:%S"), 600, 600),
        ]
        trend = daily_trend(records, days=7)
        assert len(trend) == 7
        assert trend[-1]["count"] == 1
        assert trend[0]["count"] == 0
