# BiliHistory —— B站历史记录管理工具

[![Python](https://img.shields.io/badge/Python-3.10%2B-blue)](https://www.python.org/)
[![PyQt6](https://img.shields.io/badge/GUI-PyQt6-green)](https://www.riverbankcomputing.com/software/pyqt/)
[![License](https://img.shields.io/badge/License-MIT-orange)](LICENSE)

> 把你的 B 站观看历史、关注列表、收藏夹永久保存在自己电脑里的桌面工具。

---

## 项目简介

BiliHistory 是一款面向 Windows 与 Linux 的 B站历史记录本地化管理工具。它通过 B站官方 API 抓取用户的观看历史、关注列表、收藏夹等信息，并以 CSV 形式持久化到本地，方便后续搜索、筛选、统计与回溯。

### 核心设计理念

- **数据本地化**：所有数据 100% 存储在本地，避免云端历史滚动丢失。
- **增量抓取**：每次抓取自动生成时间戳快照，再增量合并到总表，只增不删。
- **免费试用**：新用户首次使用起 30 天免费试用，到期后需激活会员。
- **轻量桌面端**：单窗口 PyQt6 应用，支持自由缩放，界面遵循 B站粉 + B站蓝品牌色。

---

## 主要功能

### 1. 历史记录管理

- 一键抓取全部 B站观看历史（视频 / 直播 / 专栏）。
- 本地 CSV 存储，Excel 可直接打开。
- 快照备份：每次抓取自动生成 `backups/xxx_时间戳.csv`。
- 增量去重合并，保留最早与最新观看记录。

### 2. 数据浏览与筛选

- 表格展示：类型标签、标题、UP主、分类、观看进度、观看时间、BV号/房间号/文章ID。
- 关键词搜索（标题 / UP主）。
- 类型筛选：全部 / 视频 / 直播 / 专栏。
- 时间筛选：全部 / 今天 / 本周 / 本月。
- 表格排序：支持按 UP主、类型、分类、标题、观看时间、BV号排序，相同 UP主按观看时间倒序。

### 3. 我的关注

- 抓取并展示关注列表。
- 3 列卡片网格，异步加载头像。
- 显示 UP主名称、签名、等级、认证信息。

### 4. 我的收藏

- 树形分组展示收藏夹文件夹与收藏内容。
- 支持视频、专栏等混合内容类型。

### 5. 数据统计（我的画像）

基于本地 CSV 数据的纯本地统计：

- 最常看 UP主（次数 / 总时长）。
- 最常看分类。
- 观看时段分布（凌晨 / 早晨 / 下午 / 晚上）。
- 完播率分析。
- 最近 30 天观看趋势。

### 6. 会员与授权

- 30 天免费试用（自首次使用起）。
- 激活码离线激活，机器码绑定。
- 买断制永久会员可选。

### 7. 用户卡片

- 侧边栏底部展示当前用户与授权状态。
- 显示注册时间（通过 B站用户卡片 API 查询）。

---

## 技术栈

- **Python** 3.10+
- **PyQt6** —— 跨平台桌面 GUI
- **requests** —— HTTP 请求
- **cryptography** —— 授权码校验
- **pytest** —— 单元测试
- **Nuitka** / PyInstaller —— 打包发布

---

## 项目结构

```text
HistoryofBilibili/
├── gui/                      # PyQt6 界面层
│   ├── main_window.py        # 主窗口
│   ├── table_model.py        # 表格数据模型
│   ├── delegates.py          # 表格自定义绘制
│   ├── theme.py              # QSS 主题
│   ├── stats_page.py         # 数据统计页
│   ├── following_page.py     # 关注列表页
│   ├── favorites_page.py     # 收藏夹页
│   ├── workers.py            # 后台抓取线程
│   ├── dialogs.py            # 弹窗
│   ├── settings_dialog.py    # Cookie 设置弹窗
│   ├── activation_dialog.py  # 激活对话框
│   └── log_bridge.py         # 日志桥接
├── src/                      # 业务核心
│   ├── config.py             # 配置管理
│   ├── config.json           # 默认配置
│   ├── fetcher.py            # 历史记录获取器
│   ├── social_fetcher.py     # 关注/收藏/用户信息获取器
│   ├── parser.py             # API 响应解析器
│   ├── storage.py            # CSV 存储
│   ├── models.py             # 数据模型
│   ├── analytics.py          # 本地统计
│   ├── licensing/            # 授权与试用
│   ├── logger.py             # 日志
│   ├── paths.py              # 路径管理
│   └── exceptions.py         # 异常定义
├── tests/                    # 单元测试
├── packaging/                # 打包配置
├── scripts/                  # 构建脚本
├── BiliHistory-UI/           # 高保真原型与设计规范
├── docs/                     # 产品文档
├── main.py                   # 命令行入口
├── gui_main.py               # GUI 入口
├── requirements.txt          # 依赖
└── README.md                 # 本文件
```

---

## 安装与运行

### 环境要求

- Python 3.10 或更高版本
- Windows 10/11 x64
- Linux amd64 / arm64（提供 12 种发行版格式）

### 1. 克隆项目

```bash
git clone <仓库地址>
cd HistoryofBilibili
```

### 2. 创建虚拟环境

```bash
python -m venv .venv
.venv\Scripts\activate  # Windows
# source .venv/bin/activate  # Linux/macOS
```

### 3. 安装依赖

```bash
pip install -r requirements.txt
```

### 4. 配置 Cookie

Cookie 用于抓取用户私有数据（历史记录、关注、收藏）。支持两种方式（优先级从高到低）：

**方式一：环境变量**

```bash
set BILI_COOKIE=SESSDATA=xxx;DedeUserID=xxx
```

**方式二：`.secrets.json` 文件**

参照 [`.secrets.example.json`](.secrets.example.json)，在项目根目录创建 `.secrets.json`：

```json
{
  "cookie": "SESSDATA=xxx; DedeUserID=xxx"
}
```

> `.secrets.json` 已被 `.gitignore` 排除，不会误提交。

---

## 使用方式

### 启动 GUI（推荐）

```bash
python gui_main.py
```

### 命令行模式

```bash
# 完整流程：抓取 + 处理
python main.py

# 仅获取历史记录
python main.py --fetch

# 仅处理 CSV 文件
python main.py --process

# 去重 + 排序
python main.py --process --remove-dup --sort

# 按类型筛选
python main.py --process --filter-type video

# 指定 CSV 文件
python main.py --csv-file path/to/history.csv
```

---

## 测试

```bash
python -m pytest -q
```

---

## 打包发布

所有构建脚本位于 `scripts/`，产物输出到 `dist/`。

### Windows（PyInstaller 单文件 .exe + NSIS 安装包）

```powershell
# 一键脚本（自动检测 NSIS，生成安装包）
powershell -ExecutionPolicy Bypass -File scripts\build_windows.ps1 -Arch x64
```

产物：

| 文件 | 说明 |
| --- | --- |
| `dist/BiliHistory-x64.exe` | 便携单文件 |
| `dist/BiliHistory-<version>-x64-setup.exe` | Windows 安装包（写入 Program Files、开始菜单、桌面快捷方式） |

> 注意：请使用完整安装的 CPython（如 python.org 或 Microsoft Store 版本），不要使用嵌入式/embeddable 发行版，否则 PyInstaller/Nuitka 会报错。
>
> Windows x86（32 位）暂不支持，因为 PyQt6 官方未提供 win32 wheel。
>
> 安装包安装后，用户数据（Cookie、CSV、配置、授权文件）存放在 `%APPDATA%\BiliHistory`，与程序目录分离，方便修改和备份。

### Linux（12 种发行版格式）

在 Debian/Ubuntu 或其他 Linux 发行版执行：

```bash
bash scripts/build_linux.sh [amd64|arm64]
```

产物：

| 文件 | 说明 | 适用发行版 |
| --- | --- | --- |
| `dist/BiliHistory-<arch>` | PyInstaller 单文件可执行 | 通用 |
| `dist/BiliHistory-<arch>.tar.gz` | gzip 便携压缩包 | 通用 |
| `dist/BiliHistory-<arch>.tar.bz2` | bzip2 便携压缩包 | 通用 |
| `dist/BiliHistory-<arch>.tar.xz` | xz 便携压缩包 | 通用 |
| `dist/BiliHistory-<arch>.zip` | zip 便携压缩包 | 通用 |
| `dist/bilihistory_<version>_<arch>.deb` | Debian/Ubuntu 安装包 | Debian、Ubuntu、Deepin |
| `dist/bilihistory-<version>-1.<arch>.rpm` | RPM 安装包 | Fedora、openSUSE、RHEL |
| `dist/bilihistory-<version>-1-<arch>.pkg.tar.zst` | pacman 安装包 | Arch Linux、Manjaro |
| `dist/bilihistory-<version>-<arch>.apk` | Alpine 安装包 | Alpine Linux |
| `dist/bilihistory-<version>-<arch>.txz` | Slackware 安装包 | Slackware、Salix |
| `dist/bilihistory-<version>-<arch>.sh` | 自解压 shell 安装包 | 通用 |
| `dist/bilihistory-<version>.ebuild` | Gentoo ebuild 模板 | Gentoo |
| `dist/BiliHistory-<arch>.AppImage` | 通用 Linux 可执行（需 appimagetool） | 通用 |

构建依赖（按需安装）：`python3-dev`, `dpkg-dev`, `ruby-dev`, `build-essential`, `libarchive-tools`, `rpm`, `zip`, `zstd`；`appimagetool`（AppImage 需要）。fpm 会自动生成 deb/rpm/pacman/apk/sh/txz。

> Linux i386 / armhf 暂不支持，因为 PyQt6 官方未提供对应 wheel。

Linux 用户数据存放在 `~/.config/bili-history`。

### GitHub Actions 自动构建

仓库已配置 `.github/workflows/build.yml`：

- 每次 push / PR 自动跑测试。
- 推送 `v*` 标签时自动构建 Windows `.exe`、Linux 12 种发行版格式，并发布 Release。

最新 Release 下载：[https://github.com/Qiuini/BiliHistory/releases/latest](https://github.com/Qiuini/BiliHistory/releases/latest)

### 使用 PyInstaller（备用）

```bash
pip install pyinstaller
pyinstaller packaging/bilihistory.spec
```

---

## 输出文件

- `bilibili_history.csv` —— 历史记录总表
- `backups/` —— 每次抓取的快照备份
- `dist/` —— 打包产物
- `_shots/` —— 界面截图（开发用）

---

## 常见问题

**Q: 抓取失败提示 Cookie 失效？**  
A: 请重新获取 B站 Cookie 并更新 `.secrets.json` 或环境变量。

**Q: 为什么需要 Cookie？**  
A: B站历史记录、关注列表、收藏夹均为用户私有数据，必须携带登录 Cookie 才能访问。

**Q: 试用到期后还能使用哪些功能？**  
A: 本地数据的浏览、搜索、统计功能永久免费；抓取等联网功能需激活会员。

---

## 授权

MIT License
