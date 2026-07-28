"""
导出模块 - 支持将历史记录导出为多种格式

支持的格式：
- CSV  (.csv)  兼容 Excel/WPS
- JSON (.json) 结构化数据
- HTML (.html) 可浏览表格
- Markdown (.md) 轻量文本
"""
import csv
import html
import json
import os
from datetime import datetime
from pathlib import Path
from typing import Callable, List

from exceptions import DataError
from logger import logger


ExportFormat = Callable[[List[dict], str], str]


def export_csv(records: List[dict], file_path: str) -> str:
    """导出为 CSV"""
    if not records:
        raise DataError("没有数据可导出")

    fieldnames = []
    for record in records:
        for key in record.keys():
            if key not in fieldnames:
                fieldnames.append(key)

    try:
        Path(file_path).parent.mkdir(parents=True, exist_ok=True)
        with open(file_path, 'w', encoding='utf-8-sig', newline='') as f:
            writer = csv.DictWriter(f, fieldnames=fieldnames, extrasaction='ignore', restval='')
            writer.writeheader()
            writer.writerows(records)
        logger.info(f"已导出 {len(records)} 条记录到 CSV: {file_path}")
        return file_path
    except PermissionError as e:
        raise DataError(f"导出 CSV 权限不足: {e}")
    except Exception as e:
        raise DataError(f"导出 CSV 失败: {e}")


def export_json(records: List[dict], file_path: str) -> str:
    """导出为 JSON"""
    if not records:
        raise DataError("没有数据可导出")

    payload = {
        "exported_at": datetime.now().strftime("%Y-%m-%d %H:%M:%S"),
        "count": len(records),
        "records": records,
    }
    try:
        Path(file_path).parent.mkdir(parents=True, exist_ok=True)
        with open(file_path, 'w', encoding='utf-8') as f:
            json.dump(payload, f, ensure_ascii=False, indent=2)
        logger.info(f"已导出 {len(records)} 条记录到 JSON: {file_path}")
        return file_path
    except PermissionError as e:
        raise DataError(f"导出 JSON 权限不足: {e}")
    except Exception as e:
        raise DataError(f"导出 JSON 失败: {e}")


def export_html(records: List[dict], file_path: str) -> str:
    """导出为 HTML 表格"""
    if not records:
        raise DataError("没有数据可导出")

    fieldnames = []
    for record in records:
        for key in record.keys():
            if key not in fieldnames:
                fieldnames.append(key)

    rows_html = []
    for record in records:
        cells = "\n".join(
            f"<td>{html.escape(str(record.get(k, '')))}</td>" for k in fieldnames
        )
        rows_html.append(f"<tr>\n{cells}\n</tr>")

    headers = "\n".join(f"<th>{html.escape(k)}</th>" for k in fieldnames)
    table = f"""
    <table>
      <thead><tr>{headers}</tr></thead>
      <tbody>
        {chr(10).join(rows_html)}
      </tbody>
    </table>
    """

    page = f"""<!DOCTYPE html>
<html lang="zh-CN">
<head>
  <meta charset="UTF-8">
  <title>BiliHistory 导出</title>
  <style>
    body {{ font-family: -apple-system, BlinkMacSystemFont, "Segoe UI", Roboto, sans-serif; margin: 24px; color: #1a1d26; background: #f8f9fb; }}
    h1 {{ font-size: 20px; margin-bottom: 8px; }}
    .meta {{ color: #6b7280; font-size: 13px; margin-bottom: 16px; }}
    table {{ border-collapse: collapse; width: 100%; background: #fff; border-radius: 8px; overflow: hidden; box-shadow: 0 1px 3px rgba(0,0,0,0.06); }}
    th, td {{ padding: 10px 12px; text-align: left; border-bottom: 1px solid #eef0f4; font-size: 13px; }}
    th {{ background: #f3f4f6; font-weight: 600; }}
    tr:hover {{ background: #f9fafb; }}
  </style>
</head>
<body>
  <h1>BiliHistory 历史记录</h1>
  <div class="meta">导出时间：{datetime.now().strftime("%Y-%m-%d %H:%M:%S")}，共 {len(records)} 条</div>
  {table}
</body>
</html>"""

    try:
        Path(file_path).parent.mkdir(parents=True, exist_ok=True)
        with open(file_path, 'w', encoding='utf-8') as f:
            f.write(page)
        logger.info(f"已导出 {len(records)} 条记录到 HTML: {file_path}")
        return file_path
    except PermissionError as e:
        raise DataError(f"导出 HTML 权限不足: {e}")
    except Exception as e:
        raise DataError(f"导出 HTML 失败: {e}")


def export_markdown(records: List[dict], file_path: str) -> str:
    """导出为 Markdown 列表"""
    if not records:
        raise DataError("没有数据可导出")

    lines = [
        "# BiliHistory 历史记录",
        "",
        f"> 导出时间：{datetime.now().strftime('%Y-%m-%d %H:%M:%S')}，共 {len(records)} 条",
        "",
    ]
    for idx, record in enumerate(records, start=1):
        title = record.get("标题", "无标题")
        lines.append(f"## {idx}. {title}")
        for key, value in record.items():
            if key == "标题":
                continue
            lines.append(f"- **{key}**：{value or '-'}")
        lines.append("")

    try:
        Path(file_path).parent.mkdir(parents=True, exist_ok=True)
        with open(file_path, 'w', encoding='utf-8') as f:
            f.write("\n".join(lines))
        logger.info(f"已导出 {len(records)} 条记录到 Markdown: {file_path}")
        return file_path
    except PermissionError as e:
        raise DataError(f"导出 Markdown 权限不足: {e}")
    except Exception as e:
        raise DataError(f"导出 Markdown 失败: {e}")


# 格式注册表：扩展名 -> (描述, 导出函数, 默认文件名后缀)
EXPORT_FORMATS: dict[str, tuple[str, ExportFormat, str]] = {
    ".csv": ("CSV 表格", export_csv, ".csv"),
    ".json": ("JSON 数据", export_json, ".json"),
    ".html": ("HTML 网页", export_html, ".html"),
    ".md": ("Markdown 文档", export_markdown, ".md"),
}


def guess_format(file_path: str) -> str:
    """根据文件扩展名推断导出格式，未知时返回空字符串"""
    ext = os.path.splitext(file_path)[1].lower()
    return ext if ext in EXPORT_FORMATS else ""


def export_records(records: List[dict], file_path: str) -> str:
    """根据文件扩展名自动分发到对应导出函数"""
    ext = guess_format(file_path)
    if not ext:
        raise DataError(f"不支持的导出格式: {file_path}")
    _, func, _ = EXPORT_FORMATS[ext]
    return func(records, file_path)


def supported_filters() -> str:
    """返回 QFileDialog 可用的文件过滤器字符串"""
    parts = [f"{desc} (*{ext})" for ext, (desc, _, _) in EXPORT_FORMATS.items()]
    parts.append("所有文件 (*)")
    return ";;".join(parts)
