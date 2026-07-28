#!/usr/bin/env python3
"""
资源准备脚本：从 favicon.ico 提取不同尺寸的 PNG 图标，供 Linux 打包使用。

依赖：Pillow（构建时安装）
"""
import os
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
ICO = ROOT / "favicon.ico"
OUT_DIR = ROOT / "packaging"


def main() -> int:
    try:
        from PIL import Image
    except ImportError:
        print("错误：需要 Pillow，请先安装 pip install Pillow")
        return 1

    if not ICO.is_file():
        print(f"错误：找不到图标文件 {ICO}")
        return 1

    OUT_DIR.mkdir(parents=True, exist_ok=True)

    # 读取 .ico 中所有尺寸，选择最大的保存为 256x256 PNG
    with Image.open(ICO) as img:
        sizes = img.info.get("sizes", [(img.width, img.height)])
        largest = max(sizes, key=lambda s: s[0] * s[1])
        img.size = largest
        img.load()
        png = img.convert("RGBA")
        if largest != (256, 256):
            png = png.resize((256, 256), Image.Resampling.LANCZOS)

        out_path = OUT_DIR / "icon.png"
        png.save(out_path, "PNG")
        print(f"已生成 Linux 图标: {out_path}")

    return 0


if __name__ == "__main__":
    sys.exit(main())
