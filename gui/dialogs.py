"""
反馈弹窗集合 - 按设计稿复刻:
- TrialExhaustedDialog: 试用耗尽转化弹窗（功能对比表 + 前往激活）
- FetchCompleteDialog:  抓取完成统计弹窗（三统计卡 + 信息列表）
- FetchErrorDialog:     抓取失败弹窗（Cookie失效 / 通用错误）
"""
import webbrowser

from PyQt6.QtCore import Qt
from PyQt6.QtWidgets import (
    QLabel, QHBoxLayout, QVBoxLayout, QWidget, QGridLayout, QPushButton
)

from gui.modal import ModalDialog, make_button, make_alert

# 功能对比表（功能, 免费版, 月度会员, 永久买断）；✓/✗ 用样式区分
_COMPARE_ROWS = [
    ("抓取历史记录", "30天", "✓ 无限次", "✓ 无限次"),
    ("搜索筛选", "✓", "✓", "✓"),
    ("自动备份", "✗", "✓", "✓"),
    ("优先支持", "✗", "✓", "✓"),
    ("永久更新", "✗", "✗", "✓"),
    ("价格", "免费", "¥5/月", "¥38永久"),
]


class TrialExhaustedDialog(ModalDialog):
    """试用耗尽转化弹窗；accept=前往激活"""

    def __init__(self, parent=None):
        super().__init__(parent, width=480)
        self.setWindowTitle("免费试用已到期")
        self.add_header("免费试用已到期", icon="🔒", icon_style="pink")
        body = self.body_layout(scrollable=True, max_height=460)

        icon = QLabel("🔒")
        icon.setObjectName("ResultIconLock")
        icon.setFixedSize(72, 72)
        icon.setAlignment(Qt.AlignmentFlag.AlignCenter)
        body.addWidget(icon, alignment=Qt.AlignmentFlag.AlignHCenter)

        title = QLabel("免费试用已到期")
        title.setObjectName("ResultTitle")
        title.setAlignment(Qt.AlignmentFlag.AlignCenter)
        body.addWidget(title)

        desc = QLabel("30 天免费试用已结束，升级为会员即可继续无限次抓取历史记录，"
                      "继续囤积你的视频回忆。已有数据不受影响，可正常浏览。")
        desc.setObjectName("ResultDesc")
        desc.setWordWrap(True)
        desc.setAlignment(Qt.AlignmentFlag.AlignCenter)
        body.addWidget(desc)

        # 功能对比表
        table = QWidget()
        table.setObjectName("CompareTable")
        grid = QGridLayout(table)
        grid.setContentsMargins(0, 0, 0, 0)
        grid.setSpacing(0)
        heads = [("功能对比", "CmpHead"), ("免费版", "CmpHead"),
                 ("月度会员", "CmpHeadPink"), ("永久买断", "CmpHeadGold")]
        for c, (text, name) in enumerate(heads):
            h = QLabel(text)
            h.setObjectName(name)
            h.setAlignment(Qt.AlignmentFlag.AlignCenter if c else
                           Qt.AlignmentFlag.AlignLeft | Qt.AlignmentFlag.AlignVCenter)
            grid.addWidget(h, 0, c)
        for r, row in enumerate(_COMPARE_ROWS, start=1):
            for c, cell in enumerate(row):
                if c == 0:
                    name = "CmpCell"
                elif cell.startswith("✓"):
                    name = "CmpCellYes"
                elif cell == "✗":
                    name = "CmpCellNo"
                else:
                    name = "CmpCell"
                lbl = QLabel(cell)
                lbl.setObjectName(name)
                lbl.setAlignment(Qt.AlignmentFlag.AlignCenter if c else
                                 Qt.AlignmentFlag.AlignLeft |
                                 Qt.AlignmentFlag.AlignVCenter)
                grid.addWidget(lbl, r, c)
        grid.setColumnStretch(0, 3)
        for c in range(1, 4):
            grid.setColumnStretch(c, 2)
        body.addWidget(table)

        promo = QLabel("🎁 首发优惠：首月购买月度会员即送永久会员！")
        promo.setObjectName("PromoPill")
        promo.setAlignment(Qt.AlignmentFlag.AlignCenter)
        body.addWidget(promo, alignment=Qt.AlignmentFlag.AlignHCenter)

        btn_browse = make_button("继续浏览", "text")
        btn_go = make_button("前往激活", "primary", "lg")
        btn_browse.clicked.connect(self.reject)
        btn_go.clicked.connect(self.accept)
        self.add_footer([btn_browse, btn_go], align="between")


class FetchCompleteDialog(ModalDialog):
    """抓取完成统计弹窗；accept=查看记录"""

    def __init__(self, parent=None, added: int = 0, total: int = 0,
                 pages: int = 0, snapshot: str = "", elapsed: str = ""):
        super().__init__(parent, width=460)
        self.setWindowTitle("抓取完成")
        self.add_header("抓取完成", icon="✓", icon_style="green")
        body = self.body_layout()

        intro = QLabel("本次抓取已成功完成！以下是本次抓取统计：")
        intro.setObjectName("ResultDesc")
        body.addWidget(intro)

        # 三统计卡
        stats_row = QHBoxLayout()
        stats_row.setSpacing(10)
        stats = [
            (f"+{added}", "新增记录", "StatCardValue", "pink"),
            (str(total), "总计记录", "StatCardValuePlain", ""),
            (str(pages) if pages else "—", "抓取页数", "StatCardValueBlue", "blue"),
        ]
        for value, label, value_name, accent in stats:
            card = QWidget()
            card.setObjectName("StatCard")
            if accent:
                card.setProperty("accent", accent)
            lay = QVBoxLayout(card)
            lay.setContentsMargins(12, 14, 12, 14)
            lay.setSpacing(2)
            v = QLabel(value)
            v.setObjectName(value_name)
            v.setAlignment(Qt.AlignmentFlag.AlignCenter)
            l = QLabel(label)
            l.setObjectName("StatCardLabel")
            l.setAlignment(Qt.AlignmentFlag.AlignCenter)
            lay.addWidget(v)
            lay.addWidget(l)
            stats_row.addWidget(card, stretch=1)
        body.addLayout(stats_row)

        # 信息列表
        info_rows = []
        if snapshot:
            info_rows.append(("快照备份", snapshot, True))
        if elapsed:
            info_rows.append(("耗时", elapsed, False))
        for key, value, mono in info_rows:
            row = QHBoxLayout()
            k = QLabel(key)
            k.setObjectName("InfoKey")
            v = QLabel(value)
            v.setObjectName("InfoValueMono" if mono else "InfoValue")
            v.setTextInteractionFlags(
                Qt.TextInteractionFlag.TextSelectableByMouse)
            v.setWordWrap(True)
            row.addWidget(k)
            row.addStretch(1)
            row.addWidget(v, stretch=3)
            body.addLayout(row)

        body.addWidget(make_alert(
            "数据已安全保存到本地，快照备份已创建。可随时重新抓取增量更新。",
            "success"))

        btn_close = make_button("关闭", "text")
        btn_view = make_button("查看记录", "primary")
        btn_close.clicked.connect(self.reject)
        btn_view.clicked.connect(self.accept)
        self.add_footer([btn_close, btn_view], align="between")


class FetchErrorDialog(ModalDialog):
    """抓取失败弹窗（Cookie失效等）；accept=去设置Cookie"""

    def __init__(self, parent=None, error: str = "",
                 cookie_expired: bool = False):
        super().__init__(parent, width=440)
        self.setWindowTitle("抓取失败")
        self.add_header("抓取失败", icon="⚠", icon_style="red")
        body = self.body_layout()

        icon = QLabel("⚠")
        icon.setObjectName("ResultIconError")
        icon.setFixedSize(72, 72)
        icon.setAlignment(Qt.AlignmentFlag.AlignCenter)
        body.addWidget(icon, alignment=Qt.AlignmentFlag.AlignHCenter)

        title = QLabel("Cookie已失效" if cookie_expired else "抓取失败")
        title.setObjectName("ResultTitleError")
        title.setAlignment(Qt.AlignmentFlag.AlignCenter)
        body.addWidget(title)

        desc_text = ("你的B站登录凭证已过期，请重新获取并设置Cookie后再试。"
                     if cookie_expired else
                     "本次抓取过程中发生错误，已获取的数据均已自动保存。")
        desc = QLabel(desc_text)
        desc.setObjectName("ResultDesc")
        desc.setWordWrap(True)
        desc.setAlignment(Qt.AlignmentFlag.AlignCenter)
        body.addWidget(desc)

        if error:
            body.addWidget(make_alert(f"错误详情\n{error}", "error"))

        btn_close = make_button("关闭", "text")
        if cookie_expired:
            btn_action = make_button("去设置Cookie", "primary")
        else:
            btn_action = make_button("重试", "primary")
        btn_close.clicked.connect(self.reject)
        btn_action.clicked.connect(self.accept)
        self.add_footer([btn_close, btn_action], align="between")


class AboutAuthorDialog(ModalDialog):
    """关于作者弹窗：展示作者各平台主页链接"""

    def __init__(self, parent=None):
        super().__init__(parent, width=400)
        self.setWindowTitle("关于作者")
        self.add_header("关于作者", icon="👤", icon_style="pink")
        body = self.body_layout()

        intro = QLabel("BiliHistory 由 Qiuini 开发与维护，欢迎通过以下平台联系与交流：")
        intro.setObjectName("ResultDesc")
        intro.setWordWrap(True)
        intro.setAlignment(Qt.AlignmentFlag.AlignCenter)
        body.addWidget(intro)

        links = [
            ("知乎", "https://www.zhihu.com/people/qiuini"),
            ("GitHub", "https://github.com/Qiuini"),
            ("Bilibili", "https://space.bilibili.com/32210252"),
        ]
        for name, url in links:
            row = QHBoxLayout()
            row.setSpacing(8)
            label = QLabel(f"{name}：")
            label.setObjectName("InfoKey")
            link = QLabel(f'<a href="{url}" style="color:#00AEEC;text-decoration:none;">{url}</a>')
            link.setObjectName("InfoValue")
            link.setOpenExternalLinks(True)
            link.setTextInteractionFlags(Qt.TextInteractionFlag.TextBrowserInteraction)
            link.setWordWrap(True)
            row.addWidget(label)
            row.addWidget(link, stretch=1)
            body.addLayout(row)

        body.addWidget(make_alert(
            "点击链接即可在浏览器中打开对应页面。",
            "info"))

        btn_ok = make_button("知道了", "primary")
        btn_ok.clicked.connect(self.accept)
        self.add_footer([btn_ok], align="center")
