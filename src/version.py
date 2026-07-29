"""
应用版本信息 - 单一事实来源

所有需要版本号的地方（打包脚本、CI、NSIS、GUI）都应从此模块读取，
避免在多个文件中重复硬编码。
"""

APP_NAME = "BiliHistory"
APP_NAME_CN = "BiliHistory - B站历史记录管理工具"
APP_VERSION = "1.0.11"
APP_VERSION_TUPLE = (1, 0, 11)
PUBLISHER = "Qiuini"
APP_URL = "https://github.com/Qiuini/BiliHistory"


def full_version() -> str:
    """返回带 v 前缀的版本号，例如 v1.0.0"""
    return f"v{APP_VERSION}"


def display_version() -> str:
    """返回 GUI 展示用版本号，例如 BiliHistory v1.0.0"""
    return f"{APP_NAME} v{APP_VERSION}"
