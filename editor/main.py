#!/usr/bin/env python3
"""Kys Promise decoupled data editor — entry point."""

from __future__ import annotations

import sys
from pathlib import Path

# Allow running as `python main.py` from editor/
ROOT = Path(__file__).resolve().parent
if str(ROOT) not in sys.path:
    sys.path.insert(0, str(ROOT))

from PySide6.QtWidgets import (
    QApplication, QMainWindow, QTabWidget, QFileDialog, QMessageBox,
    QStatusBar, QToolBar,
)
from PySide6.QtGui import QAction

from ui.context import EditorContext
from ui.save_editor import SaveEditorWidget
from ui.event_editor import EventEditorWidget
from ui.battle_editor import BattleEditorWidget
from ui.asset_editor import AssetEditorWidget
from ui.crossref import CrossRefWidget
from ui.wheel_guard import install_wheel_guard, harden_scroll_widgets


class MainWindow(QMainWindow):
    def __init__(self) -> None:
        super().__init__()
        self.setWindowTitle("金庸群侠前传 · 解耦制作器")
        self.resize(1200, 800)
        self.ctx = EditorContext()

        tb = QToolBar("主工具栏")
        self.addToolBar(tb)
        act_open = QAction("选择 game_data…", self)
        act_open.triggered.connect(self.choose_data_root)
        tb.addAction(act_open)
        act_reload = QAction("全部重新加载", self)
        act_reload.triggered.connect(self.ctx.reload_all)
        tb.addAction(act_reload)

        self.tabs = QTabWidget()
        self.setCentralWidget(self.tabs)
        self.save_editor = SaveEditorWidget(self.ctx)
        self.event_editor = EventEditorWidget(self.ctx)
        self.battle_editor = BattleEditorWidget(self.ctx)
        self.asset_editor = AssetEditorWidget(self.ctx)
        self.crossref = CrossRefWidget(self.ctx)
        self.tabs.addTab(self.save_editor, "存档数据")
        self.tabs.addTab(self.event_editor, "事件")
        self.tabs.addTab(self.battle_editor, "战斗")
        self.tabs.addTab(self.asset_editor, "贴图")
        self.tabs.addTab(self.crossref, "交叉引用")

        harden_scroll_widgets(self)

        self.status = QStatusBar()
        self.setStatusBar(self.status)
        self.ctx.statusMessage.connect(self.status.showMessage)
        self.ctx.dataRootChanged.connect(self._on_root)

        # Default: ../game_data relative to editor/
        default = ROOT.parent / "game_data"
        if default.is_dir():
            self.ctx.set_data_root(default)
            self._refresh_all()
        else:
            self.status.showMessage("请选择 game_data 目录")

    def _on_root(self, path: str) -> None:
        self.status.showMessage(f"数据根: {path}")
        self._refresh_all()

    def _refresh_all(self) -> None:
        self.save_editor.refresh()
        self.event_editor.refresh()
        self.battle_editor.refresh()

    def choose_data_root(self) -> None:
        path = QFileDialog.getExistingDirectory(self, "选择 game_data 目录")
        if path:
            self.ctx.set_data_root(path)
            self._refresh_all()


def main() -> int:
    app = QApplication(sys.argv)
    install_wheel_guard(app)
    win = MainWindow()
    win.show()
    return app.exec()


if __name__ == "__main__":
    raise SystemExit(main())
