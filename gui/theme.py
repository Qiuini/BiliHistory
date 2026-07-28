"""
设计系统 - 依据 BiliHistory-UI 高保真原型 (colors_and_type.css) 复刻的 QSS 主题

tokens 与组件样式均取自设计稿：B站粉 #FB7299 主色 + B站蓝 #00AEEC 强调色，
Win11 Fluent 浅色风格。QSS 不支持 box-shadow / backdrop-filter，均以边框近似。

用法:
    app.setStyleSheet(theme.GLOBAL_QSS)
控件通过 objectName 或动态属性挂接样式，例如:
    btn.setProperty("cls", "primary"); btn.setProperty("size", "lg")
"""

# ---------------- 设计 tokens ----------------
PINK = "#FB7299"
PINK_HOVER = "#FF5C8A"
PINK_PRESSED = "#E8457A"
PINK_LIGHT = "#FFF0F4"
PINK_LIGHT_HOVER = "#FFE0EA"
PINK_SOFT = "#FFD6E2"

BLUE = "#00AEEC"
BLUE_HOVER = "#0098D4"
BLUE_LIGHT = "#E0F4FC"

BG = "#F7F6F9"
CARD = "#FFFFFF"
SURFACE_HOVER = "#FAFAFD"
SURFACE_PRESSED = "#F2F1F6"
SIDEBAR_BG = "#FBFAFD"
SIDEBAR_HOVER = "#F3EFF7"

TEXT = "#1F1F2E"
TEXT_2 = "#4A4A5A"
TEXT_3 = "#8A8A9A"
TEXT_DISABLED = "#B0B0BE"

BORDER = "#EBE8EF"
BORDER_LIGHT = "#F2F0F5"
BORDER_STRONG = "#D8D4DF"
BORDER_INPUT = "#DCD8E3"

SUCCESS = "#00B578"
SUCCESS_BG = "#DFF5EA"
WARN = "#FF9B29"
WARN_BG = "#FFF3E0"

# 头像占位调色板
ACCENT_PALETTE = [PINK, BLUE, SUCCESS, WARN, "#7B61FF", "#FF6B6B", "#4ECDC4", "#45B7D1"]
DANGER = "#F04838"
DANGER_BG = "#FEECEA"
GOLD = "#FF9B29"

FONT = '"Segoe UI Variable", "Segoe UI", "Microsoft YaHei UI", "Microsoft YaHei", "PingFang SC", "Hiragino Sans GB", "Noto Sans CJK SC", "SimHei", "SimSun", sans-serif'
MONO = '"Cascadia Code", Consolas, monospace'

# 分类 tag 配色（前景, 背景）——设计稿 tag 系列
TAG_COLORS = {
    "视频": (PINK, PINK_LIGHT),
    "直播": ("#FF4D4F", "#FFE8E8"),
    "专栏": ("#FA9D3B", "#FFF3E0"),
    "番剧": (PINK, PINK_LIGHT),
    "游戏": ("#7B61FF", "#EFEAFF"),
    "科技": (BLUE, BLUE_LIGHT),
    "生活": ("#FF8B3D", "#FFF0E5"),
    "知识": (SUCCESS, SUCCESS_BG),
    "音乐": ("#F759AB", "#FEE7F3"),
}
TAG_DEFAULT = (TEXT_3, SURFACE_PRESSED)

# 日志着色（设计稿日志面板）
LOG_TIME = "#7EC699"
LOG_INFO = "#82AAFF"
LOG_OK = "#C3E88D"
LOG_WARN = "#FFCB6B"
LOG_ERROR = "#FF6B6B"

PINK_GRAD = f"qlineargradient(x1:0, y1:0, x2:1, y2:1, stop:0 {PINK}, stop:1 {PINK_HOVER})"
BLUE_GRAD = f"qlineargradient(x1:0, y1:0, x2:1, y2:1, stop:0 {BLUE}, stop:1 #33C5F3)"
GREEN_GRAD = f"qlineargradient(x1:0, y1:0, x2:1, y2:1, stop:0 {SUCCESS}, stop:1 #33C896)"

GLOBAL_QSS = f"""
/* ================= 全局 ================= */
* {{
    font-family: {FONT};
    outline: none;
}}
QWidget {{
    color: {TEXT};
    font-size: 13px;
}}
QMainWindow, #CentralArea {{
    background: {BG};
}}
QToolTip {{
    background: {CARD};
    color: {TEXT_2};
    border: 1px solid {BORDER};
    padding: 4px 8px;
    border-radius: 6px;
}}

/* ================= 标题栏 ================= */
#TitleBar {{
    background: qlineargradient(x1:0, y1:0, x2:0, y2:1,
        stop:0 #FFFFFF, stop:1 #FBFAFD);
    border-bottom: 1px solid {BORDER_LIGHT};
}}
#TitleBarIcon {{
    background: {PINK_GRAD};
    color: white;
    font-size: 13px;
    font-weight: 700;
    border-radius: 6px;
}}
#TitleBarTitle {{
    color: {TEXT};
    font-size: 12px;
    font-weight: 600;
    background: transparent;
}}
QPushButton#TitleBarBtn {{
    background: transparent;
    border: none;
    color: {TEXT_2};
    font-size: 14px;
    font-weight: 500;
}}
QPushButton#TitleBarBtn:hover {{
    background: {SURFACE_PRESSED};
    color: {TEXT};
}}
QPushButton#TitleBarBtnClose {{
    background: transparent;
    border: none;
    color: {TEXT_2};
    font-size: 14px;
    font-weight: 500;
}}
QPushButton#TitleBarBtnClose:hover {{
    background: #E81123;
    color: white;
}}

/* ================= 侧边栏 ================= */
#Sidebar {{
    background: {SIDEBAR_BG};
    border-right: 1px solid {BORDER};
}}
#BrandIcon {{
    background: {PINK_GRAD};
    border-radius: 8px;
    color: white;
    font-size: 18px;
    font-weight: 700;
}}
#BrandTitle {{
    font-size: 16px;
    font-weight: 700;
    color: {PINK};
    background: transparent;
}}
#NavSectionLabel {{
    color: {TEXT_3};
    font-size: 11px;
    font-weight: 600;
    background: transparent;
    padding-left: 12px;
}}
QPushButton#NavBtn {{
    text-align: left;
    padding: 0;
    border: none;
    border-radius: 8px;
    background: transparent;
}}
QPushButton#NavBtn:hover {{
    background: {SIDEBAR_HOVER};
}}
QPushButton#NavBtn:checked {{
    background: {PINK_LIGHT};
}}
QPushButton#NavBtn:disabled {{
    background: transparent;
}}
QLabel#NavIcon {{
    color: {TEXT_3};
    font-size: 14px;
    background: transparent;
}}
QPushButton#NavBtn:checked QLabel#NavIcon {{
    color: {PINK};
}}
QPushButton#NavBtn:disabled QLabel#NavIcon {{
    color: {TEXT_DISABLED};
}}
QLabel#NavText {{
    color: {TEXT_2};
    font-size: 14px;
    font-weight: 500;
    background: transparent;
}}
QPushButton#NavBtn:checked QLabel#NavText {{
    color: {PINK};
    font-weight: 600;
}}
QPushButton#NavBtn:disabled QLabel#NavText {{
    color: {TEXT_DISABLED};
}}
QLabel#NavBadge {{
    background: {PINK};
    color: white;
    font-size: 10px;
    font-weight: 600;
    border-radius: 9px;
    padding: 1px 6px;
    min-width: 18px;
}}
#UserCard {{
    background: qlineargradient(x1:0, y1:0, x2:1, y2:1,
        stop:0 {PINK_LIGHT}, stop:1 {BLUE_LIGHT});
    border-radius: 10px;
    border: 1px solid {BORDER_LIGHT};
}}
#UserAvatar {{
    background: {PINK_GRAD};
    color: white;
    font-size: 15px;
    font-weight: 700;
    border-radius: 16px;
}}
#UserName {{
    font-size: 13px;
    font-weight: 600;
    color: {TEXT};
    background: transparent;
}}
#UserPlan {{
    font-size: 11px;
    color: {PINK};
    font-weight: 600;
    background: transparent;
}}
#VersionLabel {{
    color: {TEXT_DISABLED};
    font-size: 11px;
    background: transparent;
}}
#UserRegTime {{
    font-size: 10px;
    color: {TEXT_DISABLED};
    background: transparent;
}}

#StatusBar
/* ================= 按钮体系 ================= */
QPushButton[cls="primary"] {{
    background: {PINK_GRAD};
    color: white;
    border: none;
    border-radius: 8px;
    padding: 8px 18px;
    font-size: 14px;
    font-weight: 600;
    min-height: 20px;
}}
QPushButton[cls="primary"]:disabled {{
    background: {PINK_SOFT};
    color: white;
}}
QPushButton[cls="primary"]:hover {{
    background: qlineargradient(x1:0, y1:0, x2:1, y2:1,
        stop:0 {PINK_HOVER}, stop:1 #FF7BA8);
}}
QPushButton[cls="primary"]:pressed {{
    background: {PINK_PRESSED};
}}
QPushButton[cls="primary"]:disabled {{
    background: {PINK_SOFT};
    color: white;
}}
QPushButton[cls="accent"] {{
    background: {BLUE_GRAD};
    color: white;
    border: none;
    border-radius: 8px;
    padding: 8px 18px;
    font-size: 14px;
    font-weight: 600;
    min-height: 20px;
}}
QPushButton[cls="accent"]:hover {{
    background: {BLUE_HOVER};
}}
QPushButton[cls="success"] {{
    background: {GREEN_GRAD};
    color: white;
    border: none;
    border-radius: 8px;
    padding: 8px 18px;
    font-size: 14px;
    font-weight: 600;
    min-height: 20px;
}}
QPushButton[cls="destructive"] {{
    background: {DANGER};
    color: white;
    border: none;
    border-radius: 8px;
    padding: 8px 18px;
    font-size: 14px;
    font-weight: 600;
    min-height: 20px;
}}
QPushButton[cls="destructive"]:hover {{
    background: #D93A2C;
}}
QPushButton[cls="destructive"]:pressed {{
    background: #B52E22;
}}
QPushButton[cls="warning"] {{
    background: {WARN_BG};
    color: {WARN};
    border: 1.5px solid {WARN};
    border-radius: 8px;
    padding: 8px 18px;
    font-size: 14px;
    font-weight: 600;
    min-height: 20px;
}}
QPushButton[cls="warning"]:hover {{
    background: {WARN};
    color: white;
}}
QPushButton[cls="secondary"] {{
    background: {CARD};
    color: {TEXT_2};
    border: 1.5px solid {BORDER_STRONG};
    border-radius: 8px;
    padding: 8px 18px;
    font-size: 14px;
    font-weight: 600;
    min-height: 20px;
}}
QPushButton[cls="secondary"]:hover {{
    border-color: {PINK};
    color: {PINK};
}}
QPushButton[cls="text"] {{
    background: transparent;
    color: {TEXT_2};
    border: none;
    border-radius: 8px;
    padding: 8px 18px;
    font-size: 14px;
    font-weight: 600;
}}
QPushButton[cls="text"]:hover {{
    background: {PINK_LIGHT};
    color: {PINK};
}}
QPushButton[cls="danger-soft"] {{
    background: {DANGER_BG};
    color: {DANGER};
    border: none;
    border-radius: 8px;
    padding: 8px 14px;
    font-size: 14px;
    font-weight: 600;
}}
QPushButton[cls="danger-soft"]:hover {{
    background: #FCD9D5;
}}
QPushButton[size="lg"] {{
    padding: 11px 26px;
    font-size: 16px;
    border-radius: 10px;
    min-height: 22px;
}}
/* 工具栏图标按钮 */
QPushButton#IconBtn {{
    background: transparent;
    border: 1px solid transparent;
    border-radius: 8px;
    padding: 0;
    font-size: 16px;
    color: {TEXT_2};
    min-width: 36px;
    min-height: 36px;
}}
QPushButton#IconBtn:hover {{
    background: {PINK_LIGHT};
    color: {PINK};
    border-color: {PINK_SOFT};
}}
QPushButton#IconBtn:pressed {{
    background: {PINK_LIGHT_HOVER};
}}
/* 弹窗关闭按钮 */
QPushButton#CloseBtn {{
    background: transparent;
    border: none;
    border-radius: 8px;
    color: {TEXT_3};
    font-size: 14px;
    font-weight: 600;
}}
QPushButton#CloseBtn:hover {{
    background: {DANGER};
    color: white;
}}

/* ================= 工具栏 / 搜索 / 下拉 ================= */
#Toolbar {{
    background: {CARD};
    border-bottom: 1px solid {BORDER_LIGHT};
}}
#SearchBox {{
    background: {SURFACE_PRESSED};
    border: 1.5px solid transparent;
    border-radius: 18px;
}}
#SearchBox:focus-within {{
    border-color: {PINK};
    background: {CARD};
}}
QLabel#SearchBoxIcon {{
    color: {TEXT_3};
    font-size: 12px;
    background: transparent;
}}
QLineEdit#SearchEditInner {{
    background: transparent;
    border: none;
    padding: 0;
    font-size: 13px;
    color: {TEXT};
}}
QLineEdit#SearchEditInner:focus {{
    border: none;
}}
/* 设计稿风格的带图标下拉框 */
#SelectBox {{
    background: {CARD};
    border: 1.5px solid {BORDER_STRONG};
    border-radius: 8px;
}}
#SelectBox:hover {{
    border-color: {PINK};
}}
QLabel#SelectIcon {{
    color: {TEXT_3};
    font-size: 13px;
    background: transparent;
}}
QComboBox#SelectCombo {{
    background: transparent;
    border: none;
    padding: 0;
    font-size: 13px;
    color: {TEXT_2};
    font-weight: 500;
}}
QComboBox#SelectCombo::drop-down {{
    border: none;
    width: 20px;
}}
QComboBox#SelectCombo::down-arrow {{
    image: none;
    border-left: 4px solid transparent;
    border-right: 4px solid transparent;
    border-top: 5px solid {TEXT_3};
}}
QComboBox#SelectCombo QAbstractItemView {{
    background: {CARD};
    border: 1px solid {BORDER};
    border-radius: 8px;
    selection-background-color: {PINK_LIGHT};
    selection-color: {PINK};
    padding: 4px;
}}

/* ================= 表格卡片 ================= */
#TableCard {{
    background: {CARD};
    border: 1px solid {BORDER};
    border-radius: 14px;
}}
QTableView {{
    background: {CARD};
    border: none;
    border-radius: 14px;
    gridline-color: transparent;
    selection-background-color: {PINK_LIGHT};
    selection-color: {TEXT};
    alternate-background-color: {CARD};
    font-size: 13px;
}}
QTableView::item {{
    padding: 8px 10px;
    border-bottom: 1px solid {BORDER_LIGHT};
}}
QTableView::item:hover {{
    background: {PINK_LIGHT};
}}
QTableView::item:selected {{
    background: {PINK_LIGHT};
    color: {TEXT};
}}
QHeaderView::section {{
    background: qlineargradient(x1:0, y1:0, x2:0, y2:1,
        stop:0 #FAF9FD, stop:1 #F5F3F8);
    color: {TEXT_3};
    font-size: 12px;
    font-weight: 600;
    border: none;
    border-bottom: 1px solid {BORDER};
    padding: 9px 10px;
}}
QTableCornerButton::section {{
    background: #FAF9FD;
    border: none;
}}

/* ================= 滚动条 ================= */
QScrollBar:vertical {{
    background: transparent;
    width: 10px;
    margin: 2px;
}}
QScrollBar::handle:vertical {{
    background: {BORDER_STRONG};
    border-radius: 4px;
    min-height: 32px;
}}
QScrollBar::handle:vertical:hover {{
    background: {PINK_SOFT};
}}
QScrollBar::add-line:vertical, QScrollBar::sub-line:vertical {{
    height: 0;
}}
QScrollBar:horizontal {{
    background: transparent;
    height: 10px;
    margin: 2px;
}}
QScrollBar::handle:horizontal {{
    background: {BORDER_STRONG};
    border-radius: 4px;
    min-width: 32px;
}}
QScrollBar::add-line:horizontal, QScrollBar::sub-line:horizontal {{
    width: 0;
}}

/* ================= 状态栏 ================= */
#StatusBar {{
    background: {CARD};
    border-top: 1px solid {BORDER_LIGHT};
}}
#StatusBar QLabel {{
    font-size: 12px;
    color: {TEXT_3};
    background: transparent;
}}
QLabel#StatusDot {{
    border-radius: 4px;
    background: {TEXT_DISABLED};
}}
QLabel#StatusStrong {{
    color: {TEXT_2};
    font-weight: 600;
    font-size: 12px;
}}
QLabel#StatusSep {{
    color: {BORDER};
    background: {BORDER};
}}
QLabel#TrialLabel {{
    color: {TEXT_3};
    font-size: 12px;
}}
QLabel#StatusLight {{
    color: {TEXT_3};
    font-size: 12px;
    background: transparent;
}}
QLabel#SnapshotLabel {{
    color: {SUCCESS};
    font-family: {MONO};
    font-size: 11px;
}}
QLabel#SnapshotIcon {{
    color: {SUCCESS};
    font-size: 11px;
    font-weight: 700;
    background: transparent;
}}
QLabel#StatusDb {{
    color: {TEXT_DISABLED};
    font-family: {MONO};
    font-size: 11px;
}}
QLabel#PlanBadge {{
    color: {TEXT_2};
    font-weight: 500;
    font-size: 12px;
    background: transparent;
}}

/* ================= 日志条 / 日志面板 ================= */
#LogBar {{
    background: qlineargradient(x1:0, y1:0, x2:0, y2:1,
        stop:0 #2C2C38, stop:1 #252530);
    border: none;
    border-top: 1px solid rgba(255,255,255,0.05);
    min-height: 30px;
    max-height: 30px;
}}
#LogBar QLabel {{
    color: #A0A0B2;
    font-size: 12px;
    background: transparent;
}}
QLabel#LogBarChevron {{
    color: #A0A0B2;
    font-size: 10px;
}}
QLabel#LogBarTitle {{
    color: #B0B0C0;
    font-weight: 500;
}}
QLabel#LogBarHint {{
    color: rgba(255,255,255,0.3);
    font-size: 11px;
    background: transparent;
}}
QLabel#LogBarStatus {{
    color: {SUCCESS};
    font-weight: 600;
    font-size: 11px;
}}
QLabel#LogBarStatusIcon {{
    color: {SUCCESS};
    font-size: 10px;
    font-weight: 700;
}}
QPlainTextEdit#LogPanel {{
    background: qlineargradient(x1:0, y1:0, x2:0, y2:1,
        stop:0 #252530, stop:1 #1E1E28);
    color: #C8C8D8;
    border: none;
    font-family: {MONO};
    font-size: 11px;
    padding: 8px 12px;
}}

/* ================= 空状态引导页 ================= */
#EmptyIconCircle {{
    background: qlineargradient(x1:0, y1:0, x2:1, y2:1,
        stop:0 {PINK_LIGHT}, stop:1 {BLUE_LIGHT});
    border-radius: 60px;
    color: {PINK};
    font-size: 52px;
}}
QLabel#EmptyTitle {{
    font-size: 20px;
    font-weight: 700;
    color: {TEXT};
    background: transparent;
}}
QLabel#EmptyDesc {{
    font-size: 13px;
    color: {TEXT_3};
    background: transparent;
}}
#StepChip {{
    background: {CARD};
    border: 1px solid {BORDER_LIGHT};
    border-radius: 10px;
}}
QLabel#StepNum {{
    background: {PINK_GRAD};
    color: white;
    font-size: 12px;
    font-weight: 700;
    border-radius: 12px;
}}
QLabel#StepText {{
    font-size: 13px;
    color: {TEXT_2};
    font-weight: 500;
    background: transparent;
}}
QLabel#StepArrow {{
    color: {TEXT_DISABLED};
    font-size: 18px;
    background: transparent;
}}
QPushButton#LinkBtn {{
    background: transparent;
    border: none;
    color: {PINK};
    font-size: 13px;
    text-align: center;
}}
QPushButton#LinkBtn:hover {{
    color: {PINK_HOVER};
    text-decoration: underline;
}}

/* ================= 抓取进度卡 ================= */
#ProgressCard {{
    background: {CARD};
    border: 1px solid {BORDER};
    border-radius: 18px;
}}
QLabel#ProgressSpinner {{
    color: {PINK};
    font-size: 16px;
    font-weight: 700;
    background: transparent;
}}
QLabel#ProgressTitle {{
    font-size: 20px;
    font-weight: 700;
    color: {TEXT};
    background: transparent;
}}
QLabel#ProgressHint {{
    font-size: 12px;
    color: {TEXT_3};
    background: transparent;
}}
QLabel#ProgressBarLabel {{
    font-size: 12px;
    color: {TEXT_2};
    font-weight: 500;
    background: transparent;
}}
QLabel#ProgressBarValue {{
    font-size: 12px;
    color: {PINK};
    font-weight: 700;
    background: transparent;
}}
QProgressBar#MainProgress {{
    background: {SURFACE_PRESSED};
    border: none;
    border-radius: 5px;
    min-height: 10px;
    max-height: 10px;
    text-align: center;
}}
QProgressBar#MainProgress::chunk {{
    background: qlineargradient(x1:0, y1:0, x2:1, y2:0,
        stop:0 {PINK}, stop:1 {BLUE});
    border-radius: 5px;
}}
QProgressBar#PageProgress {{
    background: {SURFACE_PRESSED};
    border: none;
    border-radius: 4px;
    min-height: 8px;
    max-height: 8px;
}}
QProgressBar#PageProgress::chunk {{
    background: {PINK};
    border-radius: 4px;
}}
#StatDivider {{
    border-top: 1.5px dashed {BORDER_STRONG};
    background: transparent;
}}
QLabel#StatValue {{
    font-size: 26px;
    font-weight: 700;
    color: {PINK};
    background: transparent;
}}
QLabel#StatValuePlain {{
    font-size: 26px;
    font-weight: 700;
    color: {TEXT};
    background: transparent;
}}
QLabel#StatLabel {{
    font-size: 12px;
    color: {TEXT_3};
    background: transparent;
}}

/* ================= 完成/警告横幅 ================= */
#SuccessBanner {{
    background: {SUCCESS_BG};
    border: 1px solid rgba(0,181,120,0.15);
    border-top: none;
    border-radius: 0 0 10px 10px;
    padding: 14px 24px;
}}
#SuccessBanner[variant="warning"] {{
    background: {WARN_BG};
    border-color: rgba(255,155,41,0.2);
}}
QLabel#SuccessBannerIcon {{
    color: {SUCCESS};
    font-size: 20px;
    font-weight: 700;
    background: transparent;
}}
#SuccessBanner[variant="warning"] QLabel#SuccessBannerIcon {{
    color: {WARN};
}}
QLabel#SuccessBannerText {{
    color: {SUCCESS};
    font-size: 14px;
    line-height: 1.6;
    background: transparent;
}}
#SuccessBanner[variant="warning"] QLabel#SuccessBannerText {{
    color: {WARN};
}}
QPushButton#SuccessBannerClose {{
    background: transparent;
    border: none;
    border-radius: 8px;
    color: {SUCCESS};
    font-size: 14px;
    font-weight: 600;
    min-width: 24px;
    min-height: 24px;
}}
#SuccessBanner[variant="warning"] QPushButton#SuccessBannerClose {{
    color: {WARN};
}}
QPushButton#SuccessBannerClose:hover {{
    background: rgba(0,181,120,0.12);
}}
#SuccessBanner[variant="warning"] QPushButton#SuccessBannerClose:hover {{
    background: rgba(255,155,41,0.12);
}}

/* ================= 弹窗通用 ================= */
QDialog {{
    background: #FCFCFE;
}}
#ModalHeader {{
    background: qlineargradient(x1:0, y1:0, x2:0, y2:1,
        stop:0 #FEFDFF, stop:1 {SIDEBAR_BG});
    border-bottom: 1px solid {BORDER_LIGHT};
}}
QLabel#ModalTitle {{
    font-size: 16px;
    font-weight: 700;
    color: {TEXT};
    background: transparent;
}}
QLabel#ModalSub {{
    font-size: 12px;
    color: {TEXT_3};
    background: transparent;
}}
QLabel#ModalHeadIcon {{
    background: {PINK_GRAD};
    color: white;
    font-size: 16px;
    border-radius: 9px;
}}
QLabel#ModalHeadIconGreen {{
    background: {GREEN_GRAD};
    color: white;
    font-size: 16px;
    border-radius: 9px;
}}
QLabel#ModalHeadIconRed {{
    background: qlineargradient(x1:0, y1:0, x2:1, y2:1,
        stop:0 {DANGER}, stop:1 #F76C5E);
    color: white;
    font-size: 16px;
    border-radius: 9px;
}}
#ModalFooter {{
    background: {SURFACE_HOVER};
    border-top: 1px solid {BORDER_LIGHT};
}}

/* ================= 表单 ================= */
QLabel#FormLabel {{
    font-size: 14px;
    font-weight: 600;
    color: {TEXT};
    background: transparent;
}}
QLineEdit#FormInput {{
    background: {CARD};
    border: 1.5px solid {BORDER_INPUT};
    border-radius: 8px;
    padding: 9px 12px;
    font-size: 13px;
    color: {TEXT};
    min-height: 20px;
}}
QLineEdit#FormInput:focus {{
    border-color: {PINK};
}}
QPlainTextEdit#FormTextArea {{
    background: {CARD};
    border: 1.5px solid {BORDER_INPUT};
    border-radius: 8px;
    padding: 8px 12px;
    font-family: {MONO};
    font-size: 13px;
    color: {TEXT};
}}
QPlainTextEdit#FormTextArea:focus {{
    border-color: {PINK};
}}
QLabel#FormHint {{
    font-size: 11px;
    color: {TEXT_3};
    background: transparent;
}}

/* ================= alert 提示条 ================= */
QLabel[alert="info"] {{
    background: {BLUE_LIGHT};
    color: {BLUE_HOVER};
    border-radius: 10px;
    padding: 12px 16px;
    font-size: 12px;
    font-weight: 500;
}}
QLabel[alert="error"] {{
    background: {DANGER_BG};
    color: #C43A2E;
    border-radius: 10px;
    padding: 12px 16px;
    font-size: 12px;
    font-weight: 500;
}}
QLabel[alert="success"] {{
    background: {SUCCESS_BG};
    color: #067A54;
    border-radius: 10px;
    padding: 12px 16px;
    font-size: 12px;
    font-weight: 500;
}}

/* ================= 教程步骤 ================= */
QLabel#TutorialNum {{
    background: {PINK_LIGHT};
    color: {PINK};
    font-size: 13px;
    font-weight: 700;
    border-radius: 13px;
}}
QLabel#TutorialTitle {{
    font-size: 14px;
    font-weight: 600;
    color: {TEXT};
    background: transparent;
}}
QLabel#TutorialDesc {{
    font-size: 12px;
    color: {TEXT_3};
    background: transparent;
}}

/* ================= 授权状态卡 / 机器码盒 ================= */
#LicenseCard {{
    background: qlineargradient(x1:0, y1:0, x2:1, y2:0,
        stop:0 {PINK_LIGHT}, stop:1 {BLUE_LIGHT});
    border: 1px solid {BORDER_LIGHT};
    border-radius: 14px;
}}
QLabel#LicenseIcon {{
    background: {SURFACE_PRESSED};
    color: {TEXT_3};
    font-size: 22px;
    border-radius: 12px;
}}
QLabel#LicenseIconActive {{
    background: {PINK_GRAD};
    color: white;
    font-size: 22px;
    border-radius: 12px;
}}
QLabel#LicenseState {{
    font-size: 16px;
    font-weight: 700;
    color: {TEXT};
    background: transparent;
}}
QLabel#LicenseDetail {{
    font-size: 13px;
    color: {PINK};
    font-weight: 700;
    background: transparent;
}}
#MachineBox {{
    background: {SURFACE_PRESSED};
    border: 1.5px dashed {BORDER_STRONG};
    border-radius: 10px;
}}
QLabel#MachineCode {{
    font-family: {MONO};
    font-size: 14px;
    font-weight: 600;
    letter-spacing: 1px;
    color: {TEXT};
    background: transparent;
}}
QPushButton#CopyBtn {{
    background: {CARD};
    border: 1px solid {BORDER_STRONG};
    border-radius: 6px;
    padding: 4px 12px;
    font-size: 12px;
    color: {TEXT_2};
}}
QPushButton#CopyBtn:hover {{
    border-color: {PINK};
    color: {PINK};
}}

/* ================= 价格卡 ================= */
#PriceCard {{
    background: {CARD};
    border: 2px solid {BORDER};
    border-radius: 14px;
}}
#PriceCard[featured="true"] {{
    border-color: {PINK};
}}
#PriceCard[gold="true"] {{
    border-color: {GOLD};
}}
QLabel#PriceName {{
    font-size: 13px;
    font-weight: 600;
    color: {TEXT_2};
    background: transparent;
}}
QLabel#PriceValue {{
    font-size: 22px;
    font-weight: 800;
    color: {TEXT};
    background: transparent;
}}
QLabel#PriceUnit {{
    font-size: 11px;
    color: {TEXT_3};
    background: transparent;
}}
QLabel#PriceFeature {{
    font-size: 11px;
    color: {TEXT_2};
    background: transparent;
}}
QLabel#PriceBadge {{
    background: {PINK};
    color: white;
    font-size: 10px;
    font-weight: 700;
    border-radius: 8px;
    padding: 2px 8px;
}}
QLabel#PriceBadgeGold {{
    background: {GOLD};
    color: white;
    font-size: 10px;
    font-weight: 700;
    border-radius: 8px;
    padding: 2px 8px;
}}
QLabel#PromoPill {{
    background: {PINK_LIGHT};
    color: {PINK};
    font-size: 12px;
    font-weight: 600;
    border-radius: 12px;
    padding: 5px 14px;
}}
#PurchaseGuide {{
    background: {SURFACE_HOVER};
    border: 1.5px dashed {BORDER_STRONG};
    border-radius: 10px;
}}
#PurchaseGuide QLabel {{
    font-size: 12px;
    color: {TEXT_2};
    background: transparent;
}}

/* ================= 结果态（成功/失败圆标） ================= */
QLabel#ResultIconSuccess {{
    background: {GREEN_GRAD};
    color: white;
    font-size: 32px;
    border-radius: 36px;
}}
QLabel#ResultIconError {{
    background: qlineargradient(x1:0, y1:0, x2:1, y2:1,
        stop:0 {DANGER}, stop:1 #F76C5E);
    color: white;
    font-size: 32px;
    border-radius: 36px;
}}
QLabel#ResultIconLock {{
    background: {PINK_LIGHT};
    color: {PINK};
    font-size: 32px;
    border-radius: 36px;
}}
QLabel#ResultTitleSuccess {{
    font-size: 20px;
    font-weight: 700;
    color: {SUCCESS};
    background: transparent;
}}
QLabel#ResultTitleError {{
    font-size: 20px;
    font-weight: 700;
    color: {DANGER};
    background: transparent;
}}
QLabel#ResultTitle {{
    font-size: 20px;
    font-weight: 700;
    color: {TEXT};
    background: transparent;
}}
QLabel#ResultDesc {{
    font-size: 13px;
    color: {TEXT_3};
    background: transparent;
}}

/* ================= 统计卡（完成弹窗） ================= */
#StatCard {{
    background: {SURFACE_HOVER};
    border: 1px solid {BORDER_LIGHT};
    border-radius: 10px;
}}
#StatCard[accent="pink"] {{
    background: {PINK_LIGHT};
    border-color: {PINK_SOFT};
}}
#StatCard[accent="blue"] {{
    background: {BLUE_LIGHT};
    border-color: #BDE7F8;
}}
QLabel#StatCardValue {{
    font-size: 26px;
    font-weight: 800;
    color: {PINK};
    background: transparent;
}}
QLabel#StatCardValuePlain {{
    font-size: 26px;
    font-weight: 800;
    color: {TEXT};
    background: transparent;
}}
QLabel#StatCardValueBlue {{
    font-size: 26px;
    font-weight: 800;
    color: {BLUE};
    background: transparent;
}}
QLabel#StatCardLabel {{
    font-size: 12px;
    color: {TEXT_3};
    background: transparent;
}}
QLabel#InfoKey {{
    font-size: 12px;
    color: {TEXT_3};
    background: transparent;
}}
QLabel#InfoValue {{
    font-size: 12px;
    color: {TEXT_2};
    font-weight: 600;
    background: transparent;
}}
QLabel#InfoValueMono {{
    font-family: {MONO};
    font-size: 11px;
    color: {TEXT_2};
    background: {SURFACE_PRESSED};
    border-radius: 6px;
    padding: 3px 8px;
}}

/* ================= 数据统计页面 ================= */
#StatsPage {{
    background: transparent;
}}
#ChartCard {{
    background: {CARD};
    border: 1px solid {BORDER};
    border-radius: 14px;
}}
#ChartCard QLabel#ChartCardTitle {{
    font-size: 14px;
    font-weight: 700;
    color: {TEXT};
    background: transparent;
}}
#StatCard {{
    background: {CARD};
    border: 1px solid {BORDER};
    border-radius: 12px;
}}
#StatCard QLabel#StatCardValue {{
    font-size: 28px;
    font-weight: 800;
    color: {TEXT};
    background: transparent;
}}
#StatCard QLabel#StatCardLabel {{
    font-size: 12px;
    color: {TEXT_3};
    background: transparent;
}}
#StatCard QLabel#StatCardSub {{
    font-size: 11px;
    color: {TEXT_2};
    background: transparent;
}}

/* ================= 关注列表页面 ================= */
#FollowingPage {{
    background: transparent;
}}
#FollowingCard {{
    background: {CARD};
    border: 1px solid {BORDER};
    border-radius: 12px;
}}
#FollowingCard:hover {{
    background: {SURFACE_HOVER};
    border-color: {BORDER_STRONG};
}}

/* ================= 对比表（试用耗尽） ================= */
#CompareTable {{
    background: {CARD};
    border: 1px solid {BORDER};
    border-radius: 10px;
}}
QLabel#CmpHead {{
    background: {SURFACE_PRESSED};
    font-size: 12px;
    font-weight: 700;
    color: {TEXT_2};
    padding: 8px;
}}
QLabel#CmpHeadPink {{
    background: {PINK_LIGHT};
    font-size: 12px;
    font-weight: 700;
    color: {PINK};
    padding: 8px;
}}
QLabel#CmpHeadGold {{
    background: {WARN_BG};
    font-size: 12px;
    font-weight: 700;
    color: {GOLD};
    padding: 8px;
}}
QLabel#CmpCell {{
    font-size: 12px;
    color: {TEXT_2};
    padding: 7px 8px;
    border-top: 1px solid {BORDER_LIGHT};
    background: transparent;
}}
QLabel#CmpCellYes {{
    font-size: 12px;
    color: {SUCCESS};
    font-weight: 700;
    padding: 7px 8px;
    border-top: 1px solid {BORDER_LIGHT};
    background: transparent;
}}
QLabel#CmpCellNo {{
    font-size: 12px;
    color: {TEXT_DISABLED};
    padding: 7px 8px;
    border-top: 1px solid {BORDER_LIGHT};
    background: transparent;
}}
"""
