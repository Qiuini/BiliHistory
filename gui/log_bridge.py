"""
日志桥接 - 将 logging 输出通过 Qt 信号转发到 GUI 日志面板

设计要点:
- QtLogHandler 继承 logging.Handler，内部持有一个 QObject 发射器
- fetcher / storage 等业务模块无需任何改动，只要把此 handler 挂到全局 logger 上
"""
import logging

from PyQt6.QtCore import QObject, pyqtSignal


class _LogEmitter(QObject):
    """信号发射器（Handler 本身不能直接继承 QObject 与 Handler 的元类冲突）"""
    message = pyqtSignal(str, int)  # (格式化后的文本, 日志级别)


class QtLogHandler(logging.Handler):
    """把日志记录通过 Qt 信号转发出去的 Handler"""

    def __init__(self, level: int = logging.INFO):
        super().__init__(level)
        self.emitter = _LogEmitter()
        self.setFormatter(logging.Formatter(
            fmt='%(asctime)s [%(levelname)s] %(message)s',
            datefmt='%H:%M:%S'
        ))

    def emit(self, record: logging.LogRecord):
        try:
            msg = self.format(record)
            # 通过信号跨线程安全地送回主线程（Qt 队列连接）
            self.emitter.message.emit(msg, record.levelno)
        except Exception:
            self.handleError(record)


def install_qt_log_handler(level: int = logging.INFO) -> QtLogHandler:
    """创建并挂载到项目全局 logger 上，返回 handler 供界面连接其信号"""
    from logger import logger

    handler = QtLogHandler(level)
    logger.addHandler(handler)
    return handler
