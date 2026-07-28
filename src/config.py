"""
配置管理 - 支持环境变量和配置文件
"""
import json
import os
from pathlib import Path
from typing import Optional

from models import HistoryConfig
from exceptions import ConfigError
from logger import logger
import paths


class ConfigLoader:
    """配置加载器"""

    # 环境变量前缀
    ENV_PREFIX = "BILI_"

    def __init__(self, config_path: Optional[str] = None):
        self.config_path = config_path or self._get_default_config_path()
        self.secrets_path = self._get_default_secrets_path()

    def _get_default_config_path(self) -> str:
        """获取默认配置文件路径"""
        return Path(__file__).parent / "config.json"

    def _get_default_secrets_path(self) -> Path:
        """获取默认敏感信息文件路径（用户数据目录，可写且不随程序目录）"""
        return paths.secrets_file()

    def load(self) -> HistoryConfig:
        """加载配置"""
        config_dict = self._load_json_config()
        return self._build_config(config_dict)

    def _load_json_config(self) -> dict:
        """从 JSON 文件加载配置"""
        try:
            with open(self.config_path, 'r', encoding='utf-8') as f:
                return json.load(f)
        except FileNotFoundError:
            logger.warning(f"配置文件不存在: {self.config_path}")
            return {}
        except json.JSONDecodeError as e:
            raise ConfigError(f"配置文件格式错误: {e}")

    def _load_cookie(self) -> str:
        """加载 Cookie，优先级: 环境变量 > .secrets.json"""
        cookie = os.environ.get(f"{self.ENV_PREFIX}COOKIE", "")
        if cookie:
            return cookie

        try:
            with open(self.secrets_path, 'r', encoding='utf-8') as f:
                return json.load(f).get('cookie', '')
        except FileNotFoundError:
            return ''
        except json.JSONDecodeError as e:
            raise ConfigError(f"敏感信息文件格式错误 ({self.secrets_path}): {e}")

    def _build_config(self, config_dict: dict) -> HistoryConfig:
        """构建配置对象"""
        # 优先级: 环境变量 > JSON配置 > 默认值

        csv_file = os.environ.get(
            f"{self.ENV_PREFIX}CSV_FILE",
            config_dict.get('file_paths', {}).get('csv_file') or str(paths.default_csv_file())
        )

        cookie = self._load_cookie()

        # 注意: Cookie 允许为空。仅抓取动作真正需要 Cookie，
        # 由 BilibiliFetcher 在抓取时校验；处理本地 CSV（--process / GUI 处理）无需 Cookie。
        if not cookie:
            logger.warning(
                "Cookie 未设置，抓取功能不可用；"
                "可通过环境变量 BILI_COOKIE、.secrets.json 或 GUI 设置对话框填入"
            )

        page_size = int(os.environ.get(
            f"{self.ENV_PREFIX}PAGE_SIZE",
            config_dict.get('request_params', {}).get('page_size', 50)
        ))

        fetch_all = os.environ.get(
            f"{self.ENV_PREFIX}FETCH_ALL",
            str(config_dict.get('request_params', {}).get('fetch_all', True))
        ).lower() in ('true', '1', 'yes')

        return HistoryConfig(
            csv_file=csv_file,
            cookie=cookie,
            page_size=page_size,
            fetch_all=fetch_all,
            incremental_update=config_dict.get('request_params', {}).get('incremental_update', True),
            user_agents=config_dict.get('request_headers', {}).get('user_agents', [
                "Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/120.0.0.0 Safari/537.36"
            ]),
            history_api=config_dict.get('api_endpoints', {}).get('history_api', 'https://api.bilibili.com/x/v2/history'),
            followings_api=config_dict.get('api_endpoints', {}).get('followings_api', 'https://api.bilibili.com/x/relation/followings'),
            fav_folder_api=config_dict.get('api_endpoints', {}).get('fav_folder_api', 'https://api.bilibili.com/x/v3/fav/folder/list'),
            fav_resource_api=config_dict.get('api_endpoints', {}).get('fav_resource_api', 'https://api.bilibili.com/x/v3/fav/resource/list'),
            user_card_api=config_dict.get('api_endpoints', {}).get('user_card_api', 'https://api.bilibili.com/x/web-interface/card'),
            max_retries=config_dict.get('function_retry', {}).get('max_attempts', 3),
            retry_wait=config_dict.get('function_retry', {}).get('wait_seconds', 2),
            http_total_retries=config_dict.get('http_retry', {}).get('total', 3),
            http_backoff_factor=config_dict.get('http_retry', {}).get('backoff_factor', 1.0)
        )


# 全局配置实例（延迟加载）
_config: Optional[HistoryConfig] = None


def get_config() -> HistoryConfig:
    """获取全局配置（单例）"""
    global _config
    if _config is None:
        _config = ConfigLoader().load()
    return _config


def reload_config(config_path: Optional[str] = None) -> HistoryConfig:
    """重新加载配置"""
    global _config
    _config = ConfigLoader(config_path).load()
    return _config


def save_cookie(cookie: str) -> HistoryConfig:
    """保存 Cookie 到 .secrets.json 并刷新全局配置（供 GUI 设置对话框调用）"""
    loader = ConfigLoader()
    secrets_path = loader.secrets_path

    data = {}
    if Path(secrets_path).is_file():
        try:
            with open(secrets_path, 'r', encoding='utf-8') as f:
                data = json.load(f)
        except json.JSONDecodeError:
            data = {}

    data['cookie'] = cookie.strip()
    with open(secrets_path, 'w', encoding='utf-8') as f:
        json.dump(data, f, ensure_ascii=False, indent=2)

    logger.info("Cookie 已保存到 .secrets.json")
    return reload_config()
