#!/usr/bin/env python3
"""KYS-family decoupled data editor — entry point."""

from __future__ import annotations

import sys
from pathlib import Path

# Allow running as `python main.py` from editor/
ROOT = Path(__file__).resolve().parent
if str(ROOT) not in sys.path:
    sys.path.insert(0, str(ROOT))

from PySide6.QtWidgets import (
    QApplication, QMainWindow, QTabWidget, QFileDialog, QMessageBox,
    QStatusBar, QToolBar, QLabel, QComboBox, QWidget, QHBoxLayout,
)
from PySide6.QtGui import QAction

from dataclasses import replace

from kys_formats.encoding import TEXT_ENCODING_CHOICES
from kys_formats.profile import PROFILES, find_data_root_candidates, detect_profile
from ui.context import EditorContext
from ui.save_editor import SaveEditorWidget
from ui.event_editor import EventEditorWidget
from ui.battle_editor import BattleEditorWidget
from ui.asset_editor import AssetEditorWidget
from ui.crossref import CrossRefWidget
from ui.world_map_editor import WorldMapEditorWidget
from ui.wheel_guard import install_wheel_guard, harden_scroll_widgets


class MainWindow(QMainWindow):
    def __init__(self) -> None:
        super().__init__()
        self.setWindowTitle("KYS 通用制作器")
        self.resize(1200, 800)
        self.ctx = EditorContext()

        tb = QToolBar("主工具栏")
        self.addToolBar(tb)
        act_open = QAction("选择数据根目录…", self)
        act_open.triggered.connect(self.choose_data_root)
        tb.addAction(act_open)
        act_reload = QAction("全部重新加载", self)
        act_reload.triggered.connect(self.ctx.reload_all)
        tb.addAction(act_reload)

        tb.addSeparator()
        prof_wrap = QWidget()
        prof_row = QHBoxLayout(prof_wrap)
        prof_row.setContentsMargins(8, 0, 0, 0)
        prof_row.addWidget(QLabel("配置档:"))
        self.profile_combo = QComboBox()
        self.profile_combo.addItem("自动探测", "auto")
        for pid, prof in PROFILES.items():
            self.profile_combo.addItem(prof.display_name, pid)
        self.profile_combo.currentIndexChanged.connect(self._on_profile_combo)
        prof_row.addWidget(self.profile_combo)
        tb.addWidget(prof_wrap)

        tb.addSeparator()
        enc_wrap = QWidget()
        enc_row = QHBoxLayout(enc_wrap)
        enc_row.setContentsMargins(8, 0, 0, 0)
        enc_row.addWidget(QLabel("文本编码:"))
        self.encoding_combo = QComboBox()
        for value, label in TEXT_ENCODING_CHOICES:
            self.encoding_combo.addItem(label, value)
        self.encoding_combo.currentIndexChanged.connect(self._on_encoding_changed)
        enc_row.addWidget(self.encoding_combo)
        tb.addWidget(enc_wrap)

        self.tabs = QTabWidget()
        self.setCentralWidget(self.tabs)
        self.save_editor = SaveEditorWidget(self.ctx)
        self.event_editor = EventEditorWidget(self.ctx)
        self.battle_editor = BattleEditorWidget(self.ctx)
        self.world_map_editor = WorldMapEditorWidget(self.ctx)
        self.asset_editor = AssetEditorWidget(self.ctx)
        self.crossref = CrossRefWidget(self.ctx)
        self.tabs.addTab(self.save_editor, "存档数据")
        self.tabs.addTab(self.event_editor, "事件")
        self.tabs.addTab(self.battle_editor, "战斗")
        self.tabs.addTab(self.world_map_editor, "大地图")
        self.tabs.addTab(self.asset_editor, "贴图")
        self.tabs.addTab(self.crossref, "交叉引用")

        harden_scroll_widgets(self)

        self.status = QStatusBar()
        self.setStatusBar(self.status)
        self.ctx.statusMessage.connect(self.status.showMessage)
        self.ctx.dataRootChanged.connect(self._on_root)
        self.ctx.profileChanged.connect(self._on_profile_name)
        self.ctx.encodingChanged.connect(lambda _: self._refresh_all())

        self._force_profile_id: str | None = None
        default = self._pick_default_root()
        if default is not None:
            self._apply_root(default)
        else:
            self.status.showMessage("请选择游戏数据根目录（含 save/ 与 resource/）")

    def _pick_default_root(self) -> Path | None:
        for cand in find_data_root_candidates(ROOT.parent):
            return cand
        # legacy Promise layout
        legacy = ROOT.parent / "game_data"
        if legacy.is_dir():
            return legacy
        return None

    def _apply_root(self, path: Path) -> None:
        detected = detect_profile(path)
        profile = detected
        if self._force_profile_id and self._force_profile_id in PROFILES:
            forced = PROFILES[self._force_profile_id]
            # Keep disk-detected asset layout; take record widths from preset.
            profile = replace(
                forced,
                assets=detected.assets,
                ranger_team_offset=detected.ranger_team_offset,
                ranger_team_count=detected.ranger_team_count,
                ranger_money_offset=detected.ranger_money_offset,
                ranger_inventory_base=detected.ranger_inventory_base,
                inventory_slots=detected.inventory_slots,
                display_name=f"{forced.display_name}（字宽预设 + 贴图探测）",
            )
        self.ctx.set_data_root(path, profile=profile)
        self._refresh_all()

    def _on_root(self, path: str) -> None:
        self.status.showMessage(f"数据根: {path}")
        self._sync_encoding_combo()
        self._refresh_all()

    def _on_profile_name(self, name: str) -> None:
        self.setWindowTitle(f"KYS 通用制作器 — {name}")
        self.status.showMessage(f"配置档: {name}", 5000)

    def _sync_encoding_combo(self) -> None:
        idx = self.encoding_combo.findData(self.ctx.text_encoding)
        self.encoding_combo.blockSignals(True)
        if idx >= 0:
            self.encoding_combo.setCurrentIndex(idx)
        self.encoding_combo.blockSignals(False)

    def _on_encoding_changed(self) -> None:
        enc = self.encoding_combo.currentData()
        if enc:
            self.ctx.set_text_encoding(str(enc))

    def _on_profile_combo(self) -> None:
        pid = self.profile_combo.currentData()
        self._force_profile_id = None if pid == "auto" else str(pid)
        if self.ctx.data_root:
            self._apply_root(self.ctx.data_root)

    def _refresh_all(self) -> None:
        self.save_editor.refresh()
        self.event_editor.refresh()
        self.battle_editor.refresh()
        self.world_map_editor.refresh()

    def choose_data_root(self) -> None:
        path = QFileDialog.getExistingDirectory(
            self, "选择游戏数据根目录（含 save/ 与 resource/）"
        )
        if path:
            self._apply_root(Path(path))


def main() -> int:
    app = QApplication(sys.argv)
    install_wheel_guard(app)
    win = MainWindow()
    win.show()
    return app.exec()


if __name__ == "__main__":
    raise SystemExit(main())
