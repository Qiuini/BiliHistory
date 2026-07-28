"""
存储层 - CSV 文件读写（带 Schema 版本与脏行容错）
"""
import csv
import os
from datetime import datetime
from pathlib import Path
from typing import Callable, List, Optional

from models import VideoRecord, LiveRecord, ArticleRecord, ContentType
from exceptions import DataError
from logger import logger

# CSV 文件 Schema 版本，用于未来字段变更时做迁移
CSV_SCHEMA_VERSION = "1"
SCHEMA_HEADER = "_schema_version"


def _safe_parse_time(value: str, fmt: str = "%Y-%m-%d %H:%M:%S") -> Optional[datetime]:
    """安全解析时间字符串，失败返回 None"""
    if not value:
        return None
    try:
        return datetime.strptime(value, fmt)
    except (ValueError, TypeError):
        return None


def _type_key(item: dict) -> tuple[Optional[str], str]:
    """根据字典字段判断记录类型，返回 (id_value, id_prefix)"""
    if item.get('BV号'):
        return item.get('BV号'), 'video'
    if item.get('房间号'):
        return item.get('房间号'), 'live'
    if item.get('文章ID'):
        return item.get('文章ID'), 'article'
    return None, ''


def _unique_key(item: dict) -> Optional[str]:
    """生成记录唯一键，无法识别返回 None"""
    key, prefix = _type_key(item)
    if key and prefix:
        return f"{prefix}_{key}"
    return None


def _record_factory(item: dict):
    """根据字典字段构建对应类型的 Record 对象；无法构建返回 None"""
    view_time = _safe_parse_time(item.get('观看时间', ''))
    if view_time is None:
        logger.warning(f"跳过无法解析观看时间的记录: {item.get('标题', '')[:30]}")
        return None

    key, prefix = _type_key(item)
    if prefix == 'video':
        try:
            return VideoRecord(
                title=item.get('标题', ''),
                bvid=key or '',
                author=item.get('UP主', ''),
                view_time=view_time,
                link=item.get('视频链接', ''),
                category=item.get('分类', ''),
                duration=int(item.get('总时长(秒)', 0) or 0),
                progress=int(item.get('已观看(秒)', 0) or 0),
            )
        except (ValueError, TypeError) as e:
            logger.warning(f"构建 VideoRecord 失败: {e}")
            return None

    if prefix == 'live':
        try:
            return LiveRecord(
                title=item.get('标题', ''),
                room_id=key or '',
                author=item.get('主播', ''),
                view_time=view_time,
                link=item.get('直播链接', ''),
                category=item.get('分类', ''),
            )
        except (ValueError, TypeError) as e:
            logger.warning(f"构建 LiveRecord 失败: {e}")
            return None

    if prefix == 'article':
        try:
            return ArticleRecord(
                title=item.get('标题', ''),
                article_id=key or '',
                author=item.get('作者', ''),
                view_time=view_time,
                link=item.get('专栏链接', ''),
                category=item.get('分类', ''),
            )
        except (ValueError, TypeError) as e:
            logger.warning(f"构建 ArticleRecord 失败: {e}")
            return None

    logger.warning(f"未知记录类型，无法构建 Record: {item}")
    return None


class CSVStorage:
    """CSV 存储"""

    def __init__(self, file_path: str):
        self.file_path = file_path

    def load(self) -> List[dict]:
        """加载 CSV 文件，返回字典列表"""
        if not os.path.isfile(self.file_path):
            logger.info(f"文件不存在: {self.file_path}")
            return []

        try:
            with open(self.file_path, 'r', encoding='utf-8-sig') as f:
                reader = csv.DictReader(f)
                data = list(reader)
                logger.info(f"已加载 {len(data)} 条记录")
                return data
        except Exception as e:
            raise DataError(f"加载CSV失败: {e}")

    def save(self, records: List) -> bool:
        """保存记录到 CSV"""
        if not records:
            logger.warning("没有数据可保存")
            return False

        try:
            Path(self.file_path).parent.mkdir(parents=True, exist_ok=True)

            fieldnames = [SCHEMA_HEADER]
            for record in records:
                for key in record.to_dict().keys():
                    if key not in fieldnames:
                        fieldnames.append(key)

            with open(self.file_path, 'w', encoding='utf-8-sig', newline='') as f:
                writer = csv.DictWriter(f, fieldnames=fieldnames, extrasaction='ignore', restval='')
                writer.writeheader()
                for record in records:
                    row = record.to_dict()
                    row[SCHEMA_HEADER] = CSV_SCHEMA_VERSION
                    writer.writerow(row)

            logger.info(f"已保存 {len(records)} 条记录到 {self.file_path}")
            return True
        except PermissionError:
            raise DataError(f"权限不足，无法写入: {self.file_path}")
        except Exception as e:
            raise DataError(f"保存CSV失败: {e}")


class HistoryStorage:
    """历史记录存储管理器"""

    def __init__(self, file_path: str):
        self.file_path = file_path
        self.csv_storage = CSVStorage(file_path)
        self._data: List[dict] = []

    def load(self) -> 'HistoryStorage':
        """加载数据，脏行会被跳过并记录日志"""
        raw = self.csv_storage.load()
        valid = []
        skipped = 0
        for idx, item in enumerate(raw, start=1):
            if not isinstance(item, dict):
                skipped += 1
                continue
            if not _unique_key(item):
                logger.warning(f"第 {idx} 行缺少有效 ID（BV/房间/文章ID），跳过")
                skipped += 1
                continue
            if _safe_parse_time(item.get('观看时间', '')) is None:
                logger.warning(f"第 {idx} 行观看时间格式异常，跳过")
                skipped += 1
                continue
            valid.append(item)

        if skipped:
            logger.warning(f"CSV 加载完成，跳过 {skipped} 行异常数据")
        self._data = valid
        return self

    @property
    def records(self) -> List[dict]:
        """获取所有记录"""
        return self._data

    @property
    def count(self) -> int:
        """记录数量"""
        return len(self._data)

    def add_records(self, new_records: List) -> int:
        """添加新记录（增量去重）"""
        if not new_records:
            return 0

        existing_keys = {_unique_key(item) for item in self._data if _unique_key(item)}
        added = 0
        for record in new_records:
            key = record.get_unique_key()
            if key not in existing_keys:
                self._data.append(record.to_dict())
                existing_keys.add(key)
                added += 1

        logger.info(f"添加了 {added} 条新记录")
        return added

    def remove_duplicates_by_id(self) -> int:
        """按 ID 去重（保留最新）"""
        if not self._data:
            return 0

        self.sort_by_time(reverse=True)

        seen_ids = set()
        filtered = []
        for item in self._data:
            unique = _unique_key(item)
            if unique is None:
                filtered.append(item)
                continue
            if unique not in seen_ids:
                seen_ids.add(unique)
                filtered.append(item)

        removed = len(self._data) - len(filtered)
        self._data = filtered
        logger.info(f"移除了 {removed} 条重复记录")
        return removed

    def sort_by_time(self, reverse: bool = True):
        """按观看时间排序"""
        def parse_time(item):
            t = _safe_parse_time(item.get('观看时间', ''))
            return t if t is not None else datetime.min

        self._data.sort(key=parse_time, reverse=reverse)
        logger.info(f"已按观看时间{'降序' if reverse else '升序'}排序")

    def filter_by_type(self, content_type: ContentType) -> 'HistoryStorage':
        """按内容类型筛选"""
        type_map = {
            ContentType.VIDEO: '视频',
            ContentType.LIVE: '直播',
            ContentType.ARTICLE: '专栏'
        }

        type_name = type_map.get(content_type)
        if not type_name:
            return self

        self._data = [
            item for item in self._data
            if item.get('类型') == type_name
        ]
        logger.info(f"筛选后剩余 {len(self._data)} 条记录")
        return self

    def save(self) -> bool:
        """保存到文件"""
        return self.csv_storage.save(self._to_records())

    def save_snapshot(self, records: List, backup_dir: Optional[str] = None) -> Optional[str]:
        """将本次抓取的记录保存为带时间戳的快照备份"""
        if not records:
            logger.warning("本次无新抓取记录，跳过快照备份")
            return None

        if backup_dir is None:
            try:
                import paths
                backup_dir = paths.backups_dir()
            except Exception:
                backup_dir = Path(self.file_path).resolve().parent / "backups"
        backup_dir = Path(backup_dir)
        backup_dir.mkdir(parents=True, exist_ok=True)

        timestamp = datetime.now().strftime("%Y%m%d_%H%M%S")
        stem = Path(self.file_path).stem
        snapshot_path = backup_dir / f"{stem}_{timestamp}.csv"

        CSVStorage(str(snapshot_path)).save(records)
        logger.info(f"已生成快照备份: {snapshot_path}（{len(records)} 条）")
        return str(snapshot_path)

    def _to_records(self) -> List:
        """将 dict 转换回 Record 对象"""
        records = []
        for item in self._data:
            record = _record_factory(item)
            if record is not None:
                records.append(record)
        return records
