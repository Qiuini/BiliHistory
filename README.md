# BiliHistory —— B站历史记录管理工具

[![C++](https://img.shields.io/badge/C%2B%2B-20-blue)](https://isocpp.org/)
[![Qt6](https://img.shields.io/badge/GUI-Qt6_Widgets-green)](https://www.qt.io/)
[![CMake](https://img.shields.io/badge/Build-CMake-orange)](https://cmake.org/)
[![License](https://img.shields.io/badge/License-MIT-orange)](LICENSE)

> 把你的 B 站观看历史、关注列表、收藏夹永久保存在自己电脑里的桌面工具。

---

## 项目简介

BiliHistory 是一款面向 Windows 与 Linux 的 B站历史记录本地化管理工具。它通过 B站官方 API 抓取用户的观看历史、关注列表、收藏夹等信息，并以 CSV 形式持久化到本地，方便后续搜索、筛选、统计与回溯。

### 核心设计理念

- **数据本地化**：所有数据 100% 存储在本地，避免云端历史滚动丢失。
- **增量抓取**：每次抓取自动生成时间戳快照，再增量合并到总表，只增不删。
- **免费试用**：新用户首次使用起 30 天免费试用，到期后需激活会员。
- **轻量桌面端**：单窗口 Qt6 Widgets 应用，支持自由缩放，界面遵循 B站粉 + B站蓝品牌色。

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

- **C++20**
- **Qt6 Widgets** —— 跨平台桌面 GUI
- **Qt6 Network** —— HTTP 请求与网络层
- **C++20 协程异步架构** —— `src/network/coro/` 下 Task / CancellationToken / NetworkAwaitable / SleepAwaitable，基于 `co_await` 的非阻塞网络与定时器调度
- **OpenSSL 3.x** —— 授权加密与密钥管理
- **GoogleTest** —— 单元测试
- **CMake 3.21+** —— 构建系统
- **vcpkg** —— 依赖管理
- **CPack** —— Windows NSIS / Linux DEB+RPM+TGZ 打包

---

## 项目结构

```text
HistoryofBilibili/
├── src/                      # C++20 + Qt6 完整实现
│   ├── core/                 # 核心层：配置、数据模型、CSV 存储、解析、日志、路径
│   ├── network/              # 网络层：HTTP 客户端、API 客户端、数据抓取器
│   │   └── coro/             # 协程基础设施：task.h, cancellation_token.h, network_awaitable.h, timer_awaitable.h
│   ├── business/             # 业务层：抓取工作线程、统计分析、导出、更新检查
│   ├── licensing/            # 授权层：试用、激活、机器码、加密
│   ├── gui/                  # 表示层：主窗口、表格模型、各页面、弹窗、主题
│   └── main.cpp              # 程序入口
├── tests/                    # 单元测试（按模块对应 src/）
├── tools/                    # 开发者工具：密钥生成、激活码签发
├── cmake/                    # CMake 辅助脚本
├── CMakeLists.txt            # 主构建配置
├── vcpkg.json                # vcpkg manifest
├── BiliHistory-UI/           # 高保真原型与设计规范
├── docs/                     # 产品文档
├── _shots/                   # 界面截图
├── .github/workflows/        # CI/CD：构建、测试、发布
├── .secrets.example.json     # Cookie 配置示例
├── favicon.ico               # 应用图标
├── index.html                # GitHub Pages 入口
├── docs.html                 # 在线文档页
├── LICENSE                   # MIT License
└── README.md                 # 本文件
```

### 分层架构

```
┌─────────────────────────────────────┐
│  GUI（Qt6 Widgets）                  │
│  主窗口 / 页面 / 表格模型 / 弹窗      │
├─────────────────────────────────────┤
│  Business（业务编排）                │
│  抓取工作线程 / 统计 / 导出 / 更新    │
├─────────────────────────────────────┤
│  Network（网络访问）                 │
│  HTTP 客户端 / API 封装 / 数据抓取器  │
├─────────────────────────────────────┤
│  Core（领域与持久化）                │
│  模型 / CSV 存储 / 解析 / 配置 / 日志 │
├─────────────────────────────────────┤
│  Licensing（授权与安全）             │
│  试用 / 激活 / 机器码 / OpenSSL 加密  │
└─────────────────────────────────────┘
```

---

## 构建

### 环境要求

- CMake 3.21+
- C++20 编译器（MSVC / GCC / Clang，需支持协程）
- [vcpkg](https://vcpkg.io/)（manifest 模式，自动安装 Qt6、OpenSSL、GTest）

### 1. 克隆项目

```bash
git clone <仓库地址>
cd HistoryofBilibili
```

### 2. 配置并构建

**Windows (x64)**

```powershell
cmake -B build -S . `
  -DCMAKE_TOOLCHAIN_FILE="$env:VCPKG_ROOT/scripts/buildsystems/vcpkg.cmake" `
  -DCMAKE_BUILD_TYPE=Release

cmake --build build -j4 --config Release
```

**Linux (amd64 / arm64)**

```bash
export VCPKG_DEFAULT_TRIPLET=x64-linux  # arm64 使用 arm64-linux
cmake -B build -S . \
  -DCMAKE_TOOLCHAIN_FILE="$VCPKG_ROOT/scripts/buildsystems/vcpkg.cmake" \
  -DCMAKE_BUILD_TYPE=Release

cmake --build build -j4
```

### 3. 测试

**Windows**

```powershell
ctest --test-dir build -j4 --output-on-failure
```

**Linux（需要 offscreen 平台）**

```bash
sudo apt-get install -y libegl1 libxkbcommon-x11-0 libgl1 libxkbcommon0
QT_QPA_PLATFORM=offscreen ctest --test-dir build -j4 --output-on-failure
```

### 4. 打包

**Windows**

```powershell
cd build
cpack -G "NSIS;ZIP"
# 产物：BiliHistory-<version>-x64-setup.exe、BiliHistory-<version>-x64.zip
```

**Linux**

```bash
cd build
cpack -G "DEB;RPM;TGZ"
# 产物：bilihistory_<version>_<arch>.deb、bilihistory-<version>-1.<arch>.rpm、BiliHistory-<version>-<arch>.tar.gz
```

---

## 配置 Cookie

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

启动构建产物中的桌面程序：

```bash
# Windows
build/bin/BiliHistory.exe

# Linux
build/bin/BiliHistory
```

---

## CI / CD

仓库已配置 `.github/workflows/build.yml`：

- 每次 push / PR 到 `main`/`master` 自动在 Ubuntu 上跑测试（Qt offscreen）。
- 推送 `v*` 标签时自动构建：
  - Windows x64：`.exe` + NSIS 安装包 + `.zip`
  - Linux amd64 / arm64：`.deb`、`.rpm`、`.tar.gz`
- 所有产物自动发布到 GitHub Release。

最新 Release 下载：[https://github.com/Qiuini/BiliHistory/releases/latest](https://github.com/Qiuini/BiliHistory/releases/latest)

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
