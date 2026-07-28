"""
异常定义
"""


class BilibiliToolError(Exception):
    """工具基础异常"""
    def __init__(self, message: str, code: int = 1):
        self.message = message
        self.code = code
        super().__init__(self.message)


class CookieError(BilibiliToolError):
    """Cookie 相关错误"""
    def __init__(self, message: str = "Cookie无效或已过期"):
        super().__init__(message, code=401)


class APIError(BilibiliToolError):
    """API 请求错误"""
    def __init__(self, message: str, status_code: int = None):
        self.status_code = status_code
        msg = f"API错误: {message}"
        if status_code:
            msg += f" (状态码: {status_code})"
        super().__init__(msg, code=500)


class NetworkError(BilibiliToolError):
    """网络请求错误"""
    def __init__(self, message: str):
        super().__init__(f"网络错误: {message}", code=502)


class ConfigError(BilibiliToolError):
    """配置错误"""
    def __init__(self, message: str):
        super().__init__(f"配置错误: {message}", code=100)


class DataError(BilibiliToolError):
    """数据处理错误"""
    def __init__(self, message: str):
        super().__init__(f"数据错误: {message}", code=200)


class RetryExhaustedError(BilibiliToolError):
    """重试次数耗尽"""
    def __init__(self, message: str = "重试次数耗尽"):
        super().__init__(message, code=503)
