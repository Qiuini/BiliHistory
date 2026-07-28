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
from secrets_store import SecretsStore
import paths


class ConfigLoader:
    """配置加载器"""

    # 环境变量前缀
    ENV_PREFIX = "BILI_"

    def __init__(self, config_path: Optional[str] = None):
        self.config_path = Path(config_path) if config_path else self._get_default_config_path()
        self.bundled_config_path = self._get_bundled_config_path()
        self.secrets_path = self._get_default_secrets_path()
        self._ensure_user_config()

    def _get_default_config_path(self) -> Path:
        """获取用户可写的默认配置文件路径（优先使用）"""
        return paths.app_data_dir() / "config.json"

    def _get_bundled_config_path(self) -> Path:
        """获取程序内置的默认配置文件路径（作为模板）"""
        return Path(__file__).parent / "config.json"

    def _get_default_secrets_path(self) -> Path:
        """获取默认敏感信息文件路径（用户数据目录，可写且不随程序目录）"""
        return paths.secrets_file()

    def _ensure_user_config(self) -> None:
        """若用户配置文件不存在，则从程序内置模板复制一份"""
        if self.config_path.is_file():
            return
        if self.bundled_config_path.is_file():
            try:
                self.config_path.parent.mkdir(parents=True, exist_ok=True)
                self.config_path.write_text(
                    self.bundled_config_path.read_text(encoding='utf-8'),
                    encoding='utf-8'
                )
                logger.info(f"已创建用户配置文件: {self.config_path}")
            except Exception as e:
                logger.warning(f"复制配置文件失败: {e}")

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
        """加载 Cookie，优先级: 环境变量 > 加密 .secrets.json"""
        cookie = os.environ.get(f"{self.ENV_PREFIX}COOKIE", "")
        if cookie:
            return cookie

        return SecretsStore(self.secrets_path).get("cookie", "")

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
    """保存 Cookie 到加密的 .secrets.json 并刷新全局配置（供 GUI 设置对话框调用）"""
    loader = ConfigLoader()
    store = SecretsStore(loader.secrets_path)

    data = store.load()
    data['cookie'] = cookie.strip()
    store.save(data)

    logger.info("Cookie 已加密保存到 .secrets.json")
    return reload_config()
