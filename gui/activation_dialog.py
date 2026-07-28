"""
会员激活对话框 - 按设计稿 modal-activation 系列复刻:
授权状态卡 / 机器码虚线盒+复制 / 激活码输入 / 三档价格卡 / 购买指引，
以及激活成功 / 激活失败两个结果弹窗。
"""
from PyQt6.QtCore import Qt, QTimer
from PyQt6.QtWidgets import (
    QLabel, QLineEdit, QHBoxLayout, QVBoxLayout, QWidget, QApplication
)

from licensing import license_manager as lm
from licensing import online_activation
from licensing import trial

from gui.modal import ModalDialog, make_button, make_alert

# 三档价格卡（名称, 价格, 单位, 徽章, 徽章样式, 特性列表, 卡片属性）
_PLANS = [
    ("免费试用", "3次", "限免", "", "", ["浏览本地数据", "搜索和筛选", "3次免费抓取"], ""),
    ("月度会员", "¥5", "/月", "推荐", "PriceBadge",
     ["无限次抓取", "自动备份快照", "优先客服支持"], "featured"),
    ("永久买断", "¥38", "永久", "首发特惠", "PriceBadgeGold",
     ["全部功能解锁", "永久免费更新", "首发买一送一"], "gold"),
]

_PURCHASE_GUIDE = (
    "购买方式：添加客服微信 BiliHistoryCS 付款，付款后发送机器码，"
    "1分钟内获取激活码。\n"
    "🎁 首发福利：首月购买月度会员即送永久会员！"
)


class ActivationDialog(ModalDialog):
    """会员激活对话框（未激活/已激活状态自适应）"""

    def __init__(self, parent=None):
        super().__init__(parent, width=560)
        self.setWindowTitle("会员激活")
        self._build_ui()
        self._refresh_status()

    def _build_ui(self):
        self.add_header("会员激活", "激活会员解锁无限次抓取",
                        icon="👑", icon_style="pink")
        body = self.body_layout(scrollable=True, max_height=520)

        # ---- 授权状态卡 ----
        card = QWidget()
        card.setObjectName("LicenseCard")
        card_lay = QHBoxLayout(card)
        card_lay.setContentsMargins(18, 16, 18, 16)
        card_lay.setSpacing(14)
        self.state_icon = QLabel("🛡")
        self.state_icon.setObjectName("LicenseIcon")
        self.state_icon.setFixedSize(48, 48)
        self.state_icon.setAlignment(Qt.AlignmentFlag.AlignCenter)
        card_lay.addWidget(self.state_icon)
        state_col = QVBoxLayout()
        state_col.setSpacing(4)
        self.state_label = QLabel("未激活")
        self.state_label.setObjectName("LicenseState")
        self.detail_label = QLabel("")
        self.detail_label.setObjectName("LicenseDetail")
        state_col.addWidget(self.state_label)
        state_col.addWidget(self.detail_label)
        card_lay.addLayout(state_col, stretch=1)
        body.addWidget(card)

        # ---- 机器码 ----
        mid_label = QLabel("本机机器码")
        mid_label.setObjectName("FormLabel")
        body.addWidget(mid_label)

        mid_box = QWidget()
        mid_box.setObjectName("MachineBox")
        mid_lay = QHBoxLayout(mid_box)
        mid_lay.setContentsMargins(14, 10, 10, 10)
        self.mid_code = QLabel(lm.machine_id())
        self.mid_code.setObjectName("MachineCode")
        self.mid_code.setTextInteractionFlags(
            Qt.TextInteractionFlag.TextSelectableByMouse)
        mid_lay.addWidget(self.mid_code, stretch=1)
        copy_btn = make_button("复制", "secondary")
        copy_btn.setObjectName("CopyBtn")
        copy_btn.setCursor(Qt.CursorShape.PointingHandCursor)
        copy_btn.clicked.connect(lambda: self._copy_mid(copy_btn))
        mid_lay.addWidget(copy_btn)
        body.addWidget(mid_box)

        mid_hint = QLabel("购买时请将此机器码发送给客服，激活码将绑定本机，换机不换码")
        mid_hint.setObjectName("FormHint")
        mid_hint.setWordWrap(True)
        body.addWidget(mid_hint)

        # ---- 激活码输入 ----
        code_label = QLabel("激活码 / 授权密钥")
        code_label.setObjectName("FormLabel")
        body.addWidget(code_label)
        self.code_edit = QLineEdit()
        self.code_edit.setObjectName("FormInput")
        self.code_edit.setPlaceholderText("请输入离线激活码或服务器授权密钥...")
        body.addWidget(self.code_edit)

        server_label = QLabel("授权服务器地址（可选）")
        server_label.setObjectName("FormLabel")
        body.addWidget(server_label)
        self.server_edit = QLineEdit()
        self.server_edit.setObjectName("FormInput")
        self.server_edit.setPlaceholderText("例如 http://127.0.0.1:8787")
        body.addWidget(self.server_edit)

        server_hint = QLabel("提示：输入服务器授权密钥（如 XXXX-XXXX-XXXX）并填写服务器地址，"
                             "即可在线激活；离线激活码直接粘贴即可。")
        server_hint.setObjectName("FormHint")
        server_hint.setWordWrap(True)
        body.addWidget(server_hint)

        # ---- 会员方案 ----
        plan_row = QHBoxLayout()
        plan_label = QLabel("会员方案")
        plan_label.setObjectName("FormLabel")
        promo = QLabel("首发优惠")
        promo.setObjectName("PromoPill")
        plan_row.addWidget(plan_label)
        plan_row.addWidget(promo)
        plan_row.addStretch(1)
        body.addLayout(plan_row)

        cards_row = QHBoxLayout()
        cards_row.setSpacing(10)
        for name, price, unit, badge, badge_style, features, prop in _PLANS:
            cards_row.addWidget(self._price_card(
                name, price, unit, badge, badge_style, features, prop))
        body.addLayout(cards_row)

        # ---- 购买指引 ----
        guide = QWidget()
        guide.setObjectName("PurchaseGuide")
        guide_lay = QVBoxLayout(guide)
        guide_lay.setContentsMargins(16, 12, 16, 12)
        guide_label = QLabel(_PURCHASE_GUIDE)
        guide_label.setWordWrap(True)
        guide_label.setTextInteractionFlags(
            Qt.TextInteractionFlag.TextSelectableByMouse)
        guide_lay.addWidget(guide_label)
        body.addWidget(guide)

        # ---- footer ----
        btn_later = make_button("稍后再说", "text")
        btn_activate = make_button("立即激活", "accent")
        btn_later.clicked.connect(self.reject)
        btn_activate.clicked.connect(self._on_activate)
        self.add_footer([btn_later, btn_activate], align="between")

    @staticmethod
    def _price_card(name, price, unit, badge, badge_style, features, prop) -> QWidget:
        card = QWidget()
        card.setObjectName("PriceCard")
        if prop:
            card.setProperty(prop, "true")
        lay = QVBoxLayout(card)
        lay.setContentsMargins(14, 12, 14, 12)
        lay.setSpacing(6)

        head = QHBoxLayout()
        n = QLabel(name)
        n.setObjectName("PriceName")
        head.addWidget(n)
        head.addStretch(1)
        if badge:
            b = QLabel(badge)
            b.setObjectName(badge_style)
            head.addWidget(b)
        lay.addLayout(head)

        price_row = QHBoxLayout()
        price_row.setSpacing(4)
        v = QLabel(price)
        v.setObjectName("PriceValue")
        u = QLabel(unit)
        u.setObjectName("PriceUnit")
        price_row.addWidget(v)
        price_row.addWidget(u, alignment=Qt.AlignmentFlag.AlignBottom)
        price_row.addStretch(1)
        lay.addLayout(price_row)

        for f in features:
            fl = QLabel(f"✓ {f}")
            fl.setObjectName("PriceFeature")
            lay.addWidget(fl)
        lay.addStretch(1)
        return card

    # ---------------- 状态与动作 ----------------
    def _refresh_status(self):
        info = lm.current_license()
        if info is not None:
            self.state_icon.setText("👑")
            self.state_icon.setObjectName("LicenseIconActive")
            type_cn = "永久买断" if info.is_buyout else "月度会员"
            self.state_label.setText(f"已激活 · {type_cn}")
            self.detail_label.setText(f"有效期至：{info.expiry_text()}")
        else:
            self.state_icon.setText("🛡")
            self.state_icon.setObjectName("LicenseIcon")
            self.state_label.setText("未激活")
            left = trial.remaining_days()
            if left > 0:
                self.detail_label.setText(
                    f"免费试用剩余：{left} 天")
            else:
                self.detail_label.setText("免费试用已到期，请购买授权")
        # objectName 变更后强制重刷 QSS
        self.state_icon.style().unpolish(self.state_icon)
        self.state_icon.style().polish(self.state_icon)

    def _copy_mid(self, btn):
        QApplication.clipboard().setText(self.mid_code.text())
        btn.setText("已复制")
        QTimer.singleShot(1500, lambda: btn.setText("复制"))

    def _on_activate(self):
        code = self.code_edit.text().strip()
        if not code:
            self.code_edit.setFocus()
            return

        # 包含点的字符串视为离线签名激活码；否则尝试服务器在线激活
        if "." in code:
            info = lm.activate(code)
        else:
            server_url = self.server_edit.text().strip()
            if not server_url:
                self.server_edit.setFocus()
                return
            signed_code = online_activation.activate_online(server_url, code)
            if signed_code is None:
                info = None
            else:
                info = lm.current_license()

        if info is None:
            dlg = ActivationFailedDialog(self)
            if dlg.exec():  # "重新输入"
                self.code_edit.selectAll()
                self.code_edit.setFocus()
            return
        self._refresh_status()
        ActivationSuccessDialog(self, info).exec()
        self.accept()


class ActivationSuccessDialog(ModalDialog):
    """激活成功结果弹窗"""

    def __init__(self, parent=None, info=None):
        super().__init__(parent, width=420)
        self.setWindowTitle("激活成功")
        self.add_header("激活成功", icon="✓", icon_style="green")
        body = self.body_layout()

        icon = QLabel("✓")
        icon.setObjectName("ResultIconSuccess")
        icon.setFixedSize(72, 72)
        icon.setAlignment(Qt.AlignmentFlag.AlignCenter)
        body.addWidget(icon, alignment=Qt.AlignmentFlag.AlignHCenter)

        title = QLabel("激活成功！")
        title.setObjectName("ResultTitleSuccess")
        title.setAlignment(Qt.AlignmentFlag.AlignCenter)
        body.addWidget(title)

        desc = QLabel("感谢支持！你的会员已成功激活，现在可以享受无限次抓取了。")
        desc.setObjectName("ResultDesc")
        desc.setWordWrap(True)
        desc.setAlignment(Qt.AlignmentFlag.AlignCenter)
        body.addWidget(desc)

        if info is not None:
            card = QWidget()
            card.setObjectName("LicenseCard")
            card_lay = QHBoxLayout(card)
            card_lay.setContentsMargins(18, 14, 18, 14)
            crown = QLabel("👑")
            crown.setObjectName("LicenseIconActive")
            crown.setFixedSize(40, 40)
            crown.setAlignment(Qt.AlignmentFlag.AlignCenter)
            card_lay.addWidget(crown)
            col = QVBoxLayout()
            type_cn = "永久买断" if info.is_buyout else "月度会员"
            t = QLabel(type_cn)
            t.setObjectName("LicenseState")
            e = QLabel(f"有效期至：{info.expiry_text()}")
            e.setObjectName("LicenseDetail")
            col.addWidget(t)
            col.addWidget(e)
            card_lay.addLayout(col, stretch=1)
            body.addWidget(card)

        body.addWidget(make_alert(
            "🎉 首发福利已生效，感谢你在第一时间支持 BiliHistory！", "success"))

        btn = make_button("开始使用", "primary", "lg")
        btn.clicked.connect(self.accept)
        self.add_footer([btn], align="center")


class ActivationFailedDialog(ModalDialog):
    """激活失败结果弹窗；accept=重新输入，reject=取消"""

    def __init__(self, parent=None):
        super().__init__(parent, width=420)
        self.setWindowTitle("激活失败")
        self.add_header("激活失败", icon="✕", icon_style="red")
        body = self.body_layout()

        icon = QLabel("✕")
        icon.setObjectName("ResultIconError")
        icon.setFixedSize(72, 72)
        icon.setAlignment(Qt.AlignmentFlag.AlignCenter)
        body.addWidget(icon, alignment=Qt.AlignmentFlag.AlignHCenter)

        title = QLabel("激活失败")
        title.setObjectName("ResultTitleError")
        title.setAlignment(Qt.AlignmentFlag.AlignCenter)
        body.addWidget(title)

        desc = QLabel("激活码无效或已被使用，请检查后重试。")
        desc.setObjectName("ResultDesc")
        desc.setWordWrap(True)
        desc.setAlignment(Qt.AlignmentFlag.AlignCenter)
        body.addWidget(desc)

        body.addWidget(make_alert(
            "错误码：AUTH_CODE_INVALID\n"
            "请确认激活码输入正确（注意大小写），且与本机机器码匹配。\n"
            "如多次失败请联系客服核实。", "error"))

        btn_cancel = make_button("取消", "text")
        btn_retry = make_button("重新输入", "primary")
        btn_cancel.clicked.connect(self.reject)
        btn_retry.clicked.connect(self.accept)
        self.add_footer([btn_cancel, btn_retry])
