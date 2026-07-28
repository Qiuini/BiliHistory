"""
日志配置
"""
import logging
import sys
from pathlib import Path


def setup_logger(name: str = "bilibili_tool", level: int = logging.INFO) -> logging.Logger:
    """配置并返回日志器"""
    logger = logging.getLogger(name)
    logger.setLevel(level)

    # 避免重复添加 handler
    if logger.handlers:
        return logger

    # 控制台处理器
    console_handler = logging.StreamHandler(sys.stdout)
    console_handler.setLevel(level)

    # 格式
    formatter = logging.Formatter(
        fmt='%(asctime)s [%(levelname)s] %(message)s',
        datefmt='%H:%M:%S'
    )
    console_handler.setFormatter(formatter)

    logger.addHandler(console_handler)

    return logger


# 全局日志器
logger = setup_logger()
