# BiliHistory 项目结构说明

> 本文档介绍项目根目录下每个文件夹与关键文件的用途。
> 技术栈：C++20 + Qt6 Widgets，C++20 协程驱动异步网络层，分层架构 + 依赖注入 + PIMPL。

---

## 顶层目录总览

```
HistoryofBilibili/
├── .github/            CI 持续集成配置
├── BiliHistory-UI/     早期 UI 设计稿（HTML 静态原型）
├── _shots/             应用截图（用于 README/产品介绍）
├── cmake/              CMake 辅助模块
├── docs/               项目文档
├── src/                源码主目录（分层架构）
├── tests/              单元测试
├── third_party/        第三方库（vendored）
├── tools/              离线工具（密钥生成 / 授权签发）
├── CMakeLists.txt      根构建脚本
├── vcpkg.json          vcpkg 依赖清单
├── README.md           项目说明
├── LICENSE             开源许可证
├── index.html          项目主页（GitHub Pages 入口）
├── docs.html           在线文档页
├── favicon.ico         站点图标
├── .gitattributes      Git 属性
├── .gitignore          Git 忽略规则
└── .secrets.example.json  密钥配置示例（不含真实密钥）
```

---

## 1. `src/` — 源码主目录（分层架构）

项目核心，按职责分为 5 层，依赖方向严格自上而下：

```
src/
├── core/        核心基础层（无业务、无 UI）
├── licensing/   授权加密层（与 core 平级）
├── network/     网络通信层（依赖 core）
├── business/    业务逻辑层（依赖 core + network）
├── gui/         图形界面层（依赖所有下层）
└── main.cpp     程序入口（依赖组装根）
```

**依赖图**：`licensing ⇄ core → network → business → gui`，`main.cpp` 是组合根，统一装配依赖注入。

### 1.1 `src/core/` — 核心基础层

最底层基础设施，不含业务逻辑与 UI。

| 文件 | 用途 |
|------|------|
| `i_config.h` | 配置抽象接口（DI 入口） |
| `config.h/cpp` | 配置具体实现（JSON 持久化） |
| `i_logger.h` | 日志抽象接口 |
| `logger.h/cpp` | 日志实现（文件写入 + 轮转） |
| `i_feature_access.h` | 特性门控接口（Pro/试用） |
| `models.h/cpp` | 数据模型（`BaseRecord` 多态层次：Video/Live/Article） |
| `csv_parser.h/cpp` | RFC 4180 风格 CSV 解析器 |
| `csv_storage.h/cpp` | 历史记录 CSV 持久化 |
| `parser.h/cpp` | Bilibili API JSON 响应解析 |
| `exceptions.h/cpp` | 全局异常类型（Network/Cookie/Storage/Export） |
| `paths.h/cpp` | 应用路径（配置目录、日志目录等） |
| `version.h/cpp` | 版本号管理 |

### 1.2 `src/licensing/` — 授权加密层

授权、试用、加密相关，与 core 平级（互不依赖）。

| 文件 | 用途 |
|------|------|
| `i_license_manager.h` | 授权管理抽象接口 |
| `license_manager.h/cpp` | 授权校验实现（HMAC 签名） |
| `i_feature_access.h`（在 core） | 特性门控接口 |
| `feature_access.h/cpp` | 特性门控实现（Pro 解锁判定） |
| `trial.h/cpp` | 30 天试用机制（防篡改 + 防回拨） |
| `crypto.h/cpp` | OpenSSL 加密/签名工具 |
| `machine_id.h/cpp` | 机器指纹生成 |
| `keys.h/cpp` | 内置公钥 |
| `secrets_store.h/cpp` | Cookie / 敏感数据加密存储 |
| `exceptions.h` | 授权层异常 |

### 1.3 `src/network/` — 网络通信层

基于 C++20 协程的异步网络栈，依赖 core。

```
network/
├── coro/              C++20 协程基础设施
│   ├── task.h              Task<T> 协程任务类型
│   ├── cancellation_token.h 取消令牌（Guard RAII）
│   ├── network_awaitable.h  QNetworkReply → co_await 适配器
│   └── timer_awaitable.h    定时器 → co_await 适配器
├── i_http_client.h    HTTP 客户端抽象接口
├── http_client.h/cpp  HTTP 客户端实现（重试/退避/取消）
├── i_api_client.h     Bilibili API 抽象接口
├── api_client.h/cpp   Bilibili API 客户端实现
├── i_fetcher.h        数据抓取器抽象接口（历史/关注/收藏）
├── i_user_profile_fetcher.h  用户资料抓取器抽象接口
└── fetchers.h/cpp     各抓取器实现（继承 BaseFetcher）
```

### 1.4 `src/business/` — 业务逻辑层

业务编排，依赖 core + network，不依赖 GUI。

| 文件 | 用途 |
|------|------|
| `i_fetch_worker.h` | 后台抓取工作线程抽象接口 |
| `fetch_worker.h/cpp` | 抓取工作线程实现（独立 QThread） |
| `filter.h/cpp` | 多维度记录筛选（时间/类型/分类/作者/进度/关键字） |
| `analytics.h/cpp` | 统计分析（TopN/时段分布/趋势，模板化聚合） |
| `exporter.h/cpp` | 多格式导出（CSV/JSON/HTML/Markdown/Xlsx/PDF） |
| `xlsx_writer.h/cpp` | 基于 miniz 的 xlsx 写入器 |
| `updater.h/cpp` | 版本更新检查（GitHub Releases API） |

### 1.5 `src/gui/` — 图形界面层

Qt6 Widgets 表现层，依赖所有下层。所有类使用 PIMPL 隔离实现。

| 文件 | 用途 |
|------|------|
| `main_window.h/cpp` | 主窗口（侧边栏导航、页面切换、抓取状态机） |
| `history_table_model.h/cpp` | 历史记录表格模型（QAbstractTableModel） |
| `following_page.h/cpp` | 关注列表页（卡片网格） |
| `favorites_page.h/cpp` | 收藏夹页（树形结构） |
| `stats_page.h/cpp` | 统计分析页（图表展示） |
| `profile_page.h/cpp` | 个人资料页（头像/昵称/注册时间） |
| `filter_dialog.h/cpp` | 高级筛选对话框 |
| `settings_dialog.h/cpp` | 设置对话框 |
| `activation_dialog.h/cpp` | 授权激活对话框 |
| `trial_expired_dialog.h/cpp` | 试用到期对话框 |
| `image_loader.h/cpp` | 图片异步加载器（缓存/去重/工作线程） |
| `theme.h/cpp` | 全局主题与样式 |
| `animation_utils.h/cpp` | 动画工具（hover 缩放等） |

### 1.6 `src/main.cpp` — 程序入口（组合根）

依赖注入装配点：独立构造 `Config` / `LogWriter` / `LicenseManager` / `HttpClient` / `ApiClient` / `FetchWorker`，以接口指针注入各组件。

---

## 2. `tests/` — 单元测试

按源码模块对应组织，使用 GoogleTest + Qt。当前 **174 个测试全绿**。

```
tests/
├── core/              核心层测试（config/csv_parser/csv_storage/parser）
├── licensing/         授权层测试（crypto/license_manager/secrets_store/trial）
├── network/           网络层测试（http_client/api_client/fetchers）
│   ├── coro_test_helper.h   协程测试辅助（awaitTask 同步等待）
│   └── test_http_server.h   本地 mock HTTP 服务器
├── business/          业务层测试（analytics/exporter/filter/fetch_worker/updater/xlsx_writer）
├── gui/               GUI 测试（filter_dialog/image_loader/table_model/theme）
├── CMakeLists.txt     测试构建配置
└── main.cpp           测试入口（QCoreApplication + gtest main）
```

---

## 3. `third_party/` — 第三方库（vendored）

```
third_party/
└── miniz/             单文件 C 语言 ZIP 库（用于 xlsx 写入）
```

**说明**：这是项目里唯一的"原生 C"代码（您可能记成 `native` 的就是这个）。不走 vcpkg，直接 vendored 进仓库，由 `third_party/miniz/CMakeLists.txt` 构建为静态库。

---

## 4. `tools/` — 离线工具

C++ 命令行工具，用于授权体系运维（不打进主程序）。

| 文件 | 用途 |
|------|------|
| `keygen.cpp` | 生成 Ed25519 公私钥对 |
| `issue_license.cpp` | 用私钥签发授权文件 |
| `public_key.pem` | 内置公钥（验证用） |

---

## 5. `BiliHistory-UI/` — UI 设计稿

早期 HTML 静态原型，用于设计验证，**非生产代码**。

```
BiliHistory-UI/
├── pages/             各页面/模态框 HTML 原型
├── .preflight/        设计预检
├── colors_and_type.css  设计系统（色彩/字体）
└── validation-report.json  设计验证报告
```

---

## 6. `docs/` — 项目文档

```
docs/
├── PRODUCT_BRIEF.md   产品简介（业务/技术栈/试用机制）
└── PROJECT_STRUCTURE.md  本文档
```

---

## 7. `cmake/` — CMake 辅助模块

```
cmake/
└── windeployqt_install.cmake.in  Windows 部署 Qt 依赖的 CMake 模板
```

---

## 8. `.github/` — CI 持续集成

```
.github/
└── workflows/
    └── build.yml      GitHub Actions 构建工作流（C++20 + Qt6）
```

---

## 9. `_shots/` — 应用截图

README 与产品介绍用的 PNG 截图（主界面/各页面/模态框）。

---

## 顶层关键文件

| 文件 | 用途 |
|------|------|
| `CMakeLists.txt` | 根构建脚本（C++20、Qt6 6.2+、Ninja） |
| `vcpkg.json` | vcpkg 依赖清单（qtbase、openssl） |
| `README.md` | 项目说明 |
| `LICENSE` | 开源许可证 |
| `index.html` | GitHub Pages 主页 |
| `docs.html` | 在线文档页 |
| `.secrets.example.json` | 密钥配置示例（实际密钥不入库） |
| `.gitattributes` / `.gitignore` | Git 配置 |

---

## 分层依赖关系（CMake 层面）

```
licensing  ←─ core (PUBLIC link licensing，因 config.cpp cookie 逻辑用 SecretsStore)
              │
              ▼
           network  (PUBLIC link core)
              │
              ▼
           business (PRIVATE link core + network + miniz + Qt6::Gui)
              │
              ▼
           gui      (PRIVATE link core + network + business)
              │
              ▼
           BiliHistory (可执行，链接全部 + Qt6::Widgets)
```

**已知遗留**：core 仍 PUBLIC link licensing（因 `core/config.cpp` 的 cookie 存取实例化 `SecretsStore`）。彻底消除需迁移 Config cookie 逻辑或引入 `ISecretsStore` 接口注入。
