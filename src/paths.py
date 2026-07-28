"""
应用数据目录 - 统一管理可写的用户数据路径

打包成 exe 后安装目录通常不可写，且 Cookie / CSV / 授权文件不应放在程序目录，
因此所有运行时可写数据统一落到平台标准的用户数据目录。
"""
import os
import sys
from pathlib import Path

APP_DIR_NAME = "BiliHistory"


def app_data_dir() -> Path:
    """返回平台标准的应用数据目录，不存在则创建

    - Windows: %APPDATA%\\BiliHistory
    - macOS:   ~/Library/Application Support/BiliHistory
    - Linux:   ~/.config/bili-history（遵循 XDG_CONFIG_HOME）
    """
    if sys.platform.startswith("win"):
        base = os.environ.get("APPDATA") or (Path.home() / "AppData" / "Roaming")
        path = Path(base) / APP_DIR_NAME
    elif sys.platform == "darwin":
        path = Path.home() / "Library" / "Application Support" / APP_DIR_NAME
    else:
        base = os.environ.get("XDG_CONFIG_HOME") or (Path.home() / ".config")
        path = Path(base) / "bili-history"

    path.mkdir(parents=True, exist_ok=True)
    return path


def secrets_file() -> Path:
    """Cookie 等敏感信息文件"""
    return app_data_dir() / "secrets.json"


def default_csv_file() -> Path:
    """默认历史记录总表"""
    return app_data_dir() / "bilibili_history.csv"


def backups_dir() -> Path:
    """快照备份目录"""
    path = app_data_dir() / "backups"
    path.mkdir(parents=True, exist_ok=True)
    return path


def license_file() -> Path:
    """授权许可文件"""
    return app_data_dir() / "license.dat"


def trial_file() -> Path:
    """试用计数文件"""
    return app_data_dir() / "trial.dat"
