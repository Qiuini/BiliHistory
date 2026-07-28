"""
本地统计分析 - 基于已有 CSV 观看记录生成用户画像
无需额外网络请求，仅使用标准库。
"""
from collections import Counter, defaultdict
from datetime import datetime, timedelta
from typing import List, Dict, Any


def _parse_view_time(item: dict) -> datetime:
    """解析观看时间字符串"""
    try:
        return datetime.strptime(item.get("观看时间", ""), "%Y-%m-%d %H:%M:%S")
    except Exception:
        return datetime.min


def _author_name(item: dict) -> str:
    """统一获取作者/主播/UP 主名称"""
    return (item.get("UP主") or item.get("主播") or item.get("作者") or "未知作者").strip()


def _content_type_name(item: dict) -> str:
    """内容类型中文显示"""
    mapping = {"video": "视频", "live": "直播", "article": "专栏"}
    return mapping.get(item.get("类型", ""), item.get("类型", "其他"))


def _duration_seconds(item: dict) -> int:
    """获取总时长（秒），仅视频有意义"""
    try:
        return int(item.get("总时长(秒)", 0) or 0)
    except (TypeError, ValueError):
        return 0


def _progress_seconds(item: dict) -> int:
    """获取已观看时长（秒）"""
    try:
        progress = int(item.get("已观看(秒)", 0) or 0)
        # -1 在游标接口表示已看完，这里已经过 parser 转换，兼容兜底
        if progress < 0:
            return _duration_seconds(item)
        return progress
    except (TypeError, ValueError):
        return 0


def _completion_ratio(item: dict) -> float:
    """计算单条完成度 0.0~1.0"""
    duration = _duration_seconds(item)
    progress = _progress_seconds(item)
    if duration <= 0:
        return 0.0
    return min(1.0, progress / duration)


def compute_basic_stats(records: List[dict]) -> Dict[str, Any]:
    """计算基础统计数字"""
    total = len(records)
    if total == 0:
        return {
            "total_records": 0,
            "total_videos": 0,
            "total_lives": 0,
            "total_articles": 0,
            "total_watch_seconds": 0,
            "total_watch_time_text": "0分钟",
            "unique_authors": 0,
            "avg_completion": "0%",
        }

    type_counter = Counter(_content_type_name(r) for r in records)
    watch_seconds = sum(_progress_seconds(r) for r in records)

    # 平均完播率：仅统计有总时长的视频
    video_ratios = [_completion_ratio(r) for r in records if _duration_seconds(r) > 0]
    avg_completion = (sum(video_ratios) / len(video_ratios) * 100) if video_ratios else 0

    return {
        "total_records": total,
        "total_videos": type_counter.get("视频", 0),
        "total_lives": type_counter.get("直播", 0),
        "total_articles": type_counter.get("专栏", 0),
        "total_watch_seconds": watch_seconds,
        "total_watch_time_text": _format_duration(watch_seconds),
        "unique_authors": len(set(_author_name(r) for r in records)),
        "avg_completion": f"{avg_completion:.1f}%",
    }


def top_authors(records: List[dict], top_n: int = 10) -> List[Dict[str, Any]]:
    """最常看 UP 主：次数 + 累计观看时长 + 平均完播率"""
    author_records = defaultdict(list)
    for r in records:
        author = _author_name(r)
        if author and author != "未知作者":
            author_records[author].append(r)

    result = []
    for author, items in author_records.items():
        watch_seconds = sum(_progress_seconds(r) for r in items)
        ratios = [_completion_ratio(r) for r in items if _duration_seconds(r) > 0]
        avg_completion = (sum(ratios) / len(ratios)) if ratios else 0
        result.append({
            "name": author,
            "count": len(items),
            "watch_seconds": watch_seconds,
            "watch_time_text": _format_duration(watch_seconds),
            "avg_completion": avg_completion,
        })

    # 按观看次数降序，次数相同按时长降序
    result.sort(key=lambda x: (x["count"], x["watch_seconds"]), reverse=True)
    return result[:top_n]


def top_categories(records: List[dict], top_n: int = 10) -> List[Dict[str, Any]]:
    """最常看分类：次数 + 累计时长"""
    category_records = defaultdict(list)
    for r in records:
        category = (r.get("分类") or "未分类").strip()
        category_records[category].append(r)

    result = []
    for category, items in category_records.items():
        watch_seconds = sum(_progress_seconds(r) for r in items)
        result.append({
            "name": category,
            "count": len(items),
            "watch_seconds": watch_seconds,
            "watch_time_text": _format_duration(watch_seconds),
        })

    result.sort(key=lambda x: (x["count"], x["watch_seconds"]), reverse=True)
    return result[:top_n]


def time_of_day_distribution(records: List[dict]) -> Dict[str, int]:
    """观看时段分布：凌晨/上午/下午/晚上"""
    buckets = {"凌晨": 0, "上午": 0, "下午": 0, "晚上": 0}
    for r in records:
        dt = _parse_view_time(r)
        if dt == datetime.min:
            continue
        hour = dt.hour
        if 0 <= hour < 6:
            buckets["凌晨"] += 1
        elif 6 <= hour < 12:
            buckets["上午"] += 1
        elif 12 <= hour < 18:
            buckets["下午"] += 1
        else:
            buckets["晚上"] += 1
    return buckets


def top_authors_by_completion(records: List[dict], min_count: int = 3, top_n: int = 10) -> List[Dict[str, Any]]:
    """完播率最高的 UP 主（至少看过 min_count 次，避免只看一次就 100% 的偏差）"""
    author_records = defaultdict(list)
    for r in records:
        author = _author_name(r)
        if author and author != "未知作者" and _duration_seconds(r) > 0:
            author_records[author].append(r)

    result = []
    for author, items in author_records.items():
        if len(items) < min_count:
            continue
        ratios = [_completion_ratio(r) for r in items]
        avg_completion = sum(ratios) / len(ratios)
        result.append({
            "name": author,
            "count": len(items),
            "avg_completion": avg_completion,
            "avg_completion_text": f"{avg_completion * 100:.1f}%",
        })

    result.sort(key=lambda x: x["avg_completion"], reverse=True)
    return result[:top_n]


def daily_trend(records: List[dict], days: int = 30) -> List[Dict[str, Any]]:
    """最近 N 天每日观看数量趋势"""
    if not records:
        return []

    today = datetime.now().replace(hour=0, minute=0, second=0, microsecond=0)
    counts = {today - timedelta(days=i): 0 for i in range(days)}

    for r in records:
        dt = _parse_view_time(r)
        if dt == datetime.min:
            continue
        day = dt.replace(hour=0, minute=0, second=0, microsecond=0)
        if day in counts:
            counts[day] += 1

    return [
        {"date": day.strftime("%m-%d"), "count": count}
        for day, count in sorted(counts.items())
    ]


def _format_duration(total_seconds: int) -> str:
    """把秒数格式化为易读文本"""
    if total_seconds < 60:
        return f"{total_seconds}秒"
    if total_seconds < 3600:
        return f"{total_seconds // 60}分钟"
    if total_seconds < 86400:
        hours = total_seconds / 3600
        return f"{hours:.1f}小时"
    days = total_seconds / 86400
    return f"{days:.1f}天"
