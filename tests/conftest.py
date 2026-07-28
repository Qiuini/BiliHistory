"""pytest 配置"""
import sys
import os

# 添加项目根目录和 src 目录到路径
project_root = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
src_dir = os.path.join(project_root, 'src')
sys.path.insert(0, src_dir)
sys.path.insert(0, project_root)
