"""
后台工作线程 - 把阻塞的抓取流程放到 QThread，避免卡死界面

FetchWorker 负责:
- 调用 BilibiliFetcher.fetch_all() 抓取（每页间隔 1.5~3 秒，阻塞）
- 解析 -> 生成时间戳快照备份 -> 合并进总表
- 通过信号把进度 / 完成 / 错误 / 取消发回主线程

进度来源: fetcher 内部已通过 logger 输出"第 N 页获取 X 条"，
由 QtLogHandler 桥接到界面日志面板；此处再补充结构化的开始/结束信号。
"""
import threading

from PyQt6.QtCore import QThread, pyqtSignal

from config import get_config
from fetcher import BilibiliFetcher
from parser import HistoryParser
from storage import HistoryStorage
from exceptions import BilibiliToolError


class FetchWorker(QThread):
    """抓取 + 合并的后台线程"""

    # 信号
    started_fetch = pyqtSignal()               # 开始抓取
    progress = pyqtSignal(str)                  # 阶段性进度文本
    finished_ok = pyqtSignal(int, int, str)    # (新增记录数, 总表当前条数, 快照路径)
    cancelled = pyqtSignal(int, int, str)      # 用户取消后保留已抓数据 (added, total, snapshot)
    failed = pyqtSignal(str)                    # 错误信息

    def __init__(self, parent=None):
        super().__init__(parent)
        self._cancel_event = threading.Event()

    def cancel(self):
        """请求取消抓取（当前页完成后会中断翻页）"""
        self._cancel_event.set()
        self.progress.emit("正在取消抓取...")

    def _is_cancelled(self) -> bool:
        return self._cancel_event.is_set()

    def run(self):
        try:
            self.started_fetch.emit()
            config = get_config()

            with BilibiliFetcher(cancel_event=self._cancel_event) as fetcher:
                self.progress.emit("开始获取 B站 观看历史...")
                raw_data = fetcher.fetch_all()

            cancelled = self._is_cancelled()

            if not raw_data:
                self.failed.emit("未获取到任何数据（可能 Cookie 失效或无历史记录）")
                return

            # 解析
            parser = HistoryParser()
            records = parser.parse({"data": {"list": raw_data}})
            self.progress.emit(f"解析得到 {len(records)} 条有效记录")

            # 存储：先快照备份，再合并进总表
            storage = HistoryStorage(config.csv_file)
            snapshot = storage.save_snapshot(records) or ""

            storage.load()
            before = storage.count
            added = storage.add_records(records)
            storage.sort_by_time()
            storage.save()

            if cancelled:
                self.cancelled.emit(added, storage.count, snapshot)
            else:
                self.finished_ok.emit(added, storage.count, snapshot)

        except BilibiliToolError as e:
            self.failed.emit(f"{e.message} (错误码: {e.code})")
        except Exception as e:
            self.failed.emit(f"未预期错误: {e}")
