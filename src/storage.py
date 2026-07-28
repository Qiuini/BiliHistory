"""
存储层 - CSV 文件读写
"""
import csv
import os
from datetime import datetime
from pathlib import Path
from typing import List, Optional

from models import VideoRecord, LiveRecord, ArticleRecord, ContentType
from exceptions import DataError
from logger import logger


class CSVStorage:
    """CSV 存储"""

    def __init__(self, file_path: str):
        self.file_path = file_path

    def load(self) -> List[dict]:
        """加载 CSV 文件"""
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
            # 确保目录存在
            Path(self.file_path).parent.mkdir(parents=True, exist_ok=True)

            with open(self.file_path, 'w', encoding='utf-8-sig', newline='') as f:
                # 合并所有记录可能出现的字段，保证视频/直播/专栏混合保存时不会缺列
                fieldnames = []
                for record in records:
                    for key in record.to_dict().keys():
                        if key not in fieldnames:
                            fieldnames.append(key)
                writer = csv.DictWriter(f, fieldnames=fieldnames, extrasaction='ignore', restval='')
                writer.writeheader()
                for record in records:
                    writer.writerow(record.to_dict())

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
        """加载数据"""
        self._data = self.csv_storage.load()
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

        # 获取已有唯一键
        existing_keys = set()
        for item in self._data:
            if 'BV号' in item and item['BV号']:
                existing_keys.add(f"video_{item['BV号']}")
            elif '房间号' in item and item['房间号']:
                existing_keys.add(f"live_{item['房间号']}")
            elif '文章ID' in item and item['文章ID']:
                existing_keys.add(f"article_{item['文章ID']}")

        # 添加不重复的记录
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
            if 'BV号' in item and item['BV号']:
                key = item['BV号']
                id_type = 'video'
            elif '房间号' in item and item['房间号']:
                key = item['房间号']
                id_type = 'live'
            elif '文章ID' in item and item['文章ID']:
                key = item['文章ID']
                id_type = 'article'
            else:
                filtered.append(item)
                continue

            unique_key = f"{id_type}_{key}"
            if unique_key not in seen_ids:
                seen_ids.add(unique_key)
                filtered.append(item)

        removed = len(self._data) - len(filtered)
        self._data = filtered
        logger.info(f"移除了 {removed} 条重复记录")
        return removed

    def sort_by_time(self, reverse: bool = True):
        """按观看时间排序"""
        def parse_time(item):
            try:
                return datetime.strptime(item['观看时间'], "%Y-%m-%d %H:%M:%S")
            except:
                return datetime.min

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
        """将本次抓取的记录保存为带时间戳的快照备份

        Args:
            records: 本次抓取解析得到的记录列表
            backup_dir: 备份目录，默认为总表同级目录下的 backups/

        Returns:
            快照文件路径；无记录时返回 None
        """
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
            if 'BV号' in item and item['BV号']:
                records.append(VideoRecord(
                    title=item.get('标题', ''),
                    bvid=item['BV号'],
                    author=item.get('UP主', ''),
                    view_time=datetime.strptime(item['观看时间'], "%Y-%m-%d %H:%M:%S"),
                    link=item.get('视频链接', ''),
                    category=item.get('分类', ''),
                    duration=int(item.get('总时长(秒)', 0)),
                    progress=int(item.get('已观看(秒)', 0))
                ))
            elif '房间号' in item:
                records.append(LiveRecord(
                    title=item.get('标题', ''),
                    room_id=item['房间号'],
                    author=item.get('主播', ''),
                    view_time=datetime.strptime(item['观看时间'], "%Y-%m-%d %H:%M:%S"),
                    link=item.get('直播链接', ''),
                    category=item.get('分类', '')
                ))
            elif '文章ID' in item:
                records.append(ArticleRecord(
                    title=item.get('标题', ''),
                    article_id=item['文章ID'],
                    author=item.get('作者', ''),
                    view_time=datetime.strptime(item['观看时间'], "%Y-%m-%d %H:%M:%S"),
                    link=item.get('专栏链接', ''),
                    category=item.get('分类', '')
                ))

        return records
