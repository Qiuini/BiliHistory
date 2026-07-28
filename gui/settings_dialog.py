"""
Cookie 设置对话框 - 按设计稿 modal-cookie 复刻:
隐私提示条 + 4步获取教程 + Cookie 多行输入，保存走 config.save_cookie
"""
from PyQt6.QtCore import Qt
from PyQt6.QtWidgets import (
    QLabel, QPlainTextEdit, QMessageBox, QHBoxLayout, QVBoxLayout
)

from config import get_config, save_cookie

from gui.modal import ModalDialog, make_button, make_alert

# 4 步教程（标题, 描述）
_STEPS = [
    ("打开B站网页", "在浏览器中访问 www.bilibili.com 并登录你的账号"),
    ("打开开发者工具", "按 F12 键打开开发者工具，切换到「网络/Network」标签"),
    ("找到Cookie", "刷新页面，点击任意请求，在请求头中找到 Cookie 字段"),
    ("复制并粘贴", "复制完整的 Cookie 值，粘贴到下方输入框中"),
]


class CookieSettingsDialog(ModalDialog):
    """Cookie 输入与保存对话框"""

    def __init__(self, parent=None):
        super().__init__(parent, width=480)
        self.setWindowTitle("Cookie设置")
        self._build_ui()
        self._load_existing()

    def _build_ui(self):
        self.add_header("Cookie设置", "粘贴B站Cookie以启用历史记录抓取",
                        icon="🍪", icon_style="pink")
        body = self.body_layout(scrollable=True, max_height=460)

        # 隐私提示
        body.addWidget(make_alert(
            "🔒 Cookie仅存储在你本地电脑，所有数据不会上传到任何服务器，"
            "隐私安全有保障。", "info"))

        # 4 步教程
        for i, (title, desc) in enumerate(_STEPS, start=1):
            row = QHBoxLayout()
            row.setSpacing(12)
            num = QLabel(str(i))
            num.setObjectName("TutorialNum")
            num.setFixedSize(26, 26)
            num.setAlignment(Qt.AlignmentFlag.AlignCenter)
            row.addWidget(num, alignment=Qt.AlignmentFlag.AlignTop)

            col = QVBoxLayout()
            col.setSpacing(2)
            t = QLabel(title)
            t.setObjectName("TutorialTitle")
            d = QLabel(desc)
            d.setObjectName("TutorialDesc")
            d.setWordWrap(True)
            col.addWidget(t)
            col.addWidget(d)
            row.addLayout(col, stretch=1)
            body.addLayout(row)

        # Cookie 输入
        label = QLabel("Cookie内容")
        label.setObjectName("FormLabel")
        body.addWidget(label)

        self.edit = QPlainTextEdit()
        self.edit.setObjectName("FormTextArea")
        self.edit.setPlaceholderText("粘贴完整的Cookie字符串到这里...")
        self.edit.setMinimumHeight(110)
        body.addWidget(self.edit)

        hint = QLabel("提示：Cookie有效期通常为几天到几周，失效后需要重新设置")
        hint.setObjectName("FormHint")
        hint.setWordWrap(True)
        body.addWidget(hint)

        # 底部按钮
        btn_cancel = make_button("取消", "text")
        btn_save = make_button("保存", "primary")
        btn_cancel.clicked.connect(self.reject)
        btn_save.clicked.connect(self._on_save)
        self.add_footer([btn_cancel, btn_save])

    def _load_existing(self):
        """回填已有 Cookie（若配置可加载）"""
        try:
            cookie = get_config().cookie
            if cookie:
                self.edit.setPlainText(cookie)
        except Exception:
            pass

    def _on_save(self):
        cookie = self.edit.toPlainText().strip()
        if not cookie:
            QMessageBox.warning(self, "提示", "Cookie 不能为空")
            return
        try:
            save_cookie(cookie)
        except Exception as e:
            QMessageBox.critical(self, "保存失败", str(e))
            return
        self.accept()
