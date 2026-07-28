"""
B站历史记录管理工具 - GUI 入口

启动方式:
    python gui_main.py
"""
import os
import sys

# 确保 src 目录在路径中（与 main.py 一致）
current_dir = os.path.dirname(os.path.abspath(__file__))
src_dir = os.path.join(current_dir, 'src')
if src_dir not in sys.path:
    sys.path.insert(0, src_dir)
if current_dir not in sys.path:
    sys.path.insert(0, current_dir)

from PyQt6.QtWidgets import QApplication

from gui import theme
from gui.main_window import MainWindow


def main():
    app = QApplication(sys.argv)
    app.setApplicationName("BiliHistory")
    app.setStyleSheet(theme.GLOBAL_QSS)
    window = MainWindow()
    window.show()
    return app.exec()


if __name__ == '__main__':
    sys.exit(main())
