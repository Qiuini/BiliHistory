"""
B站历史记录获取与处理工具

使用方式:
    python main.py                    # 完整流程
    python main.py --fetch           # 仅获取
    python main.py --process         # 仅处理
    python main.py --fetch-only      # 获取并保存新记录
"""
import argparse
import sys
import os

# 确保 src 目录在路径中
current_dir = os.path.dirname(os.path.abspath(__file__))
src_dir = os.path.join(current_dir, 'src')
if src_dir not in sys.path:
    sys.path.insert(0, src_dir)

from config import get_config
from fetcher import BilibiliFetcher
from parser import HistoryParser
from storage import HistoryStorage
from models import ContentType
from exceptions import BilibiliToolError
from logger import logger


def fetch_and_save():
    """获取历史记录并保存"""
    config = get_config()

    with BilibiliFetcher() as fetcher:
        logger.info("开始获取 B站 观看历史...")
        raw_data = fetcher.fetch_all()

        if not raw_data:
            logger.error("未获取到任何数据")
            return False

        logger.info(f"共获取 {len(raw_data)} 条原始记录")

    # 解析数据
    parser = HistoryParser()
    records = parser.parse({"data": {"list": raw_data}})
    logger.info(f"解析得到 {len(records)} 条有效记录")

    # 保存
    storage = HistoryStorage(config.csv_file)

    # 1) 先为本次抓取生成带时间戳的快照备份（原始留存，永不覆盖）
    storage.save_snapshot(records)

    # 2) 再汇总合并进总表（增量去重，只增不删）
    storage.load()
    before = storage.count
    storage.add_records(records)
    storage.sort_by_time()
    storage.save()
    logger.info(f"总表已更新: {before} -> {storage.count} 条")

    return True


def process_csv(csv_file: str, remove_dup: bool = True, sort: bool = True,
                filter_type: str = None):
    """处理 CSV 文件"""
    storage = HistoryStorage(csv_file)
    storage.load()

    if storage.count == 0:
        logger.warning("没有数据可处理")
        return True

    if remove_dup:
        storage.remove_duplicates_by_id()

    if sort:
        storage.sort_by_time()

    if filter_type:
        type_map = {
            'video': ContentType.VIDEO,
            'live': ContentType.LIVE,
            'article': ContentType.ARTICLE
        }
        content_type = type_map.get(filter_type)
        if content_type:
            storage.filter_by_type(content_type)

    storage.save()
    return True


def main():
    """主入口"""
    parser = argparse.ArgumentParser(
        description='B站历史记录获取与处理工具',
        formatter_class=argparse.RawDescriptionHelpFormatter,
        epilog="""
示例:
  python main.py                     # 完整流程
  python main.py --fetch            # 仅获取数据
  python main.py --process          # 仅处理CSV
  python main.py --csv test.csv     # 指定CSV文件
  python main.py --filter video     # 仅保留视频
        """
    )

    parser.add_argument('--fetch', action='store_true',
                        help='仅获取历史记录')
    parser.add_argument('--process', action='store_true',
                        help='仅处理 CSV 文件')
    parser.add_argument('--remove-dup', action='store_true',
                        help='移除重复记录')
    parser.add_argument('--sort', action='store_true',
                        help='按时间排序')
    parser.add_argument('--filter-type', choices=['video', 'live', 'article'],
                        help='按内容类型筛选')
    parser.add_argument('--csv-file',
                        help='CSV 文件路径（默认从配置读取）')

    args = parser.parse_args()

    try:
        config = get_config()
        csv_file = args.csv_file or config.csv_file

        # 确定执行模式
        fetch_only = args.fetch and not args.process
        process_only = args.process and not args.fetch
        full_mode = not args.fetch and not args.process

        if full_mode or fetch_only:
            if not fetch_and_save():
                sys.exit(1)

        if full_mode or process_only:
            if not process_csv(
                csv_file,
                remove_dup=args.remove_dup or full_mode,
                sort=args.sort or full_mode,
                filter_type=args.filter_type
            ):
                sys.exit(1)

        logger.info("执行完毕")
        return 0

    except BilibiliToolError as e:
        logger.error(f"{e.message} (错误码: {e.code})")
        sys.exit(e.code)
    except KeyboardInterrupt:
        logger.info("用户中断")
        sys.exit(130)
    except Exception as e:
        logger.exception(f"未预期错误: {e}")
        sys.exit(1)


if __name__ == '__main__':
    sys.exit(main())
