"""Big-map (earth/surface/building .002) overview editor."""

from __future__ import annotations

from pathlib import Path

from PySide6.QtCore import Qt
from PySide6.QtWidgets import (
    QWidget, QVBoxLayout, QHBoxLayout, QLabel, QComboBox, QPushButton,
    QMessageBox, QListWidget, QListWidgetItem, QSplitter, QFileDialog,
    QCheckBox, QSpinBox,
)

from kys_formats.world_map import (
    collect_scene_entrances,
    SceneEntrance,
    export_layer_region_json,
    save_layer_region_json,
    load_layer_region_json,
)
from ui.context import EditorContext
from ui.map_view import MapOverviewPanel, MapMarker


LAYER_HINTS = {
    "earth": "地面贴图码（mmap，偶数）",
    "surface": "地表装饰贴图码",
    "building": "建筑贴图码（mmap）",
    "buildx": "可行走标记 / 场景号旁路（见引擎）",
    "buildy": "建筑附加层（遗留）",
}


class WorldMapEditorWidget(QWidget):
    def __init__(self, ctx: EditorContext) -> None:
        super().__init__()
        self.ctx = ctx
        self._entrances: list[SceneEntrance] = []

        lay = QVBoxLayout(self)
        top = QHBoxLayout()
        top.addWidget(QLabel("编辑层"))
        self.layer_combo = QComboBox()
        self.layer_combo.currentIndexChanged.connect(self._reload)
        top.addWidget(self.layer_combo)
        self.chk_preview_layer = QCheckBox("俯视图用当前层上色")
        self.chk_preview_layer.setChecked(True)
        self.chk_preview_layer.setToolTip(
            "勾选后 earth/surface/building 用当前层贴图码上色；"
            "否则始终用 earth。不影响写入目标层。"
        )
        self.chk_preview_layer.toggled.connect(lambda _: self._reload())
        top.addWidget(self.chk_preview_layer)
        save = QPushButton("保存当前层 .002")
        save.clicked.connect(self._save)
        top.addWidget(save)
        top.addStretch()
        lay.addLayout(top)

        io = QHBoxLayout()
        for text, slot in [
            ("导出选区 JSON", self._export_region),
            ("导入选区 JSON", self._import_region),
            ("导出整层 .002", self._export_layer_file),
            ("导入整层 .002", self._import_layer_file),
            ("导出 mmap 砖…", lambda: self._open_tile_io("mmap")),
            ("导出场景砖…", lambda: self._open_tile_io("smp")),
        ]:
            b = QPushButton(text)
            b.clicked.connect(slot)
            io.addWidget(b)
        io.addStretch()
        lay.addLayout(io)

        brush_row = QHBoxLayout()
        brush_row.addWidget(QLabel("mmap砖号→笔刷"))
        self.sp_tile_idx = QSpinBox()
        self.sp_tile_idx.setRange(0, 99999)
        brush_row.addWidget(self.sp_tile_idx)
        btn_use_tile = QPushButton("设为笔刷码(×2)")
        btn_use_tile.setToolTip("引擎贴图码通常为偶数：code = tile_index * 2")
        btn_use_tile.clicked.connect(self._apply_tile_brush)
        brush_row.addWidget(btn_use_tile)
        self.lbl_layer_hint = QLabel("")
        self.lbl_layer_hint.setStyleSheet("color:#aaa;")
        brush_row.addWidget(self.lbl_layer_hint, 1)
        lay.addLayout(brush_row)

        hint = QLabel(
            "大地图 480×480。入口标记只读（不参与复制粘贴）。"
            "调整模式 + 画笔：左键拖动批量绘制当前层；"
            "框选：拖出矩形后复制/粘贴/填充（Ctrl+C/V），只改 .002 贴图码。"
            "俯视图横=引擎Y、纵=引擎X。"
        )
        hint.setWordWrap(True)
        lay.addWidget(hint)

        split = QSplitter(Qt.Horizontal)
        self.overview = MapOverviewPanel("大地图俯视图")
        self.overview.sp_cell.setValue(2)
        self.overview.chk_events.setVisible(False)
        self.overview.set_help_preset("generic")
        split.addWidget(self.overview)

        right = QWidget()
        right_lay = QVBoxLayout(right)
        right_lay.setContentsMargins(4, 0, 0, 0)
        right_lay.addWidget(QLabel("场景入口（只读）"))
        self.entrance_list = QListWidget()
        self.entrance_list.currentItemChanged.connect(self._on_entrance_selected)
        right_lay.addWidget(self.entrance_list, 1)
        self.lbl_entrance_count = QLabel("入口: 0")
        right_lay.addWidget(self.lbl_entrance_count)
        split.addWidget(right)
        split.setStretchFactor(0, 4)
        split.setStretchFactor(1, 1)
        lay.addWidget(split, 1)

        ctx.dataRootChanged.connect(lambda _: self.refresh())

    def refresh(self) -> None:
        self.layer_combo.blockSignals(True)
        self.layer_combo.clear()
        wm = self.ctx.world_map
        if not wm:
            self.layer_combo.blockSignals(False)
            self._entrances = []
            self._fill_entrance_list()
            return
        preferred = ["earth", "surface", "building", "buildx", "buildy"]
        keys = [k for k in preferred if k in wm.layers]
        keys += [k for k in wm.layers if k not in keys]
        for key in keys:
            label = f"{key} — {LAYER_HINTS.get(key, '')}"
            self.layer_combo.addItem(label, key)
        self.layer_combo.blockSignals(False)
        if self.ctx.mmap_tiles:
            self.sp_tile_idx.setMaximum(max(0, self.ctx.mmap_tiles.count - 1))
        self._reload()

    def _current_layer_key(self) -> str | None:
        return self.layer_combo.currentData()

    def _apply_tile_brush(self) -> None:
        idx = self.sp_tile_idx.value()
        self.overview.sp_brush.setValue(idx * 2)

    def _build_markers(self) -> list[MapMarker]:
        self._entrances = collect_scene_entrances(self.ctx.ranger) if self.ctx.ranger else []
        markers: list[MapMarker] = []
        for e in self._entrances:
            hue = (e.scene_id * 47) % 360
            color = _hsv_rgb(hue, 0.85, 1.0)
            markers.append(MapMarker(e.x, e.y, e.label, color))
        return markers

    def _fill_entrance_list(self) -> None:
        self.entrance_list.blockSignals(True)
        self.entrance_list.clear()
        for e in self._entrances:
            item = QListWidgetItem(e.label)
            item.setData(Qt.UserRole, (e.x, e.y, e.scene_id, e.which))
            self.entrance_list.addItem(item)
        self.entrance_list.blockSignals(False)
        self.lbl_entrance_count.setText(f"入口: {len(self._entrances)}")

    def _reload(self) -> None:
        wm = self.ctx.world_map
        key = self._current_layer_key()
        if not wm or not key or key not in wm.layers:
            self._entrances = []
            self._fill_entrance_list()
            return
        grid = wm.layers[key]
        earth = wm.layers.get("earth", grid)
        markers = self._build_markers()
        self._fill_entrance_list()
        self.lbl_layer_hint.setText(LAYER_HINTS.get(key, key))

        use_layer_preview = self.chk_preview_layer.isChecked() and key in (
            "earth",
            "surface",
            "building",
        )

        def get_code(x: int, y: int) -> int:
            return grid.get(x, y)

        def set_code(x: int, y: int, v: int) -> None:
            grid.set(x, y, v)

        def ground(x: int, y: int) -> int:
            if use_layer_preview:
                code = grid.get(x, y)
                if code > 0:
                    return code
                return earth.get(x, y)
            return earth.get(x, y)

        self.overview.bind(
            grid.size,
            grid.size,
            get_code,
            set_code,
            ground_code=ground,
            event_code=None,
            markers=markers,
            tile_pack=self.ctx.mmap_tiles,
            palette=self.ctx.palette,
        )
        self.overview.chk_markers.setText("场景入口")
        self.overview.chk_marker_labels.setText("入口文字")

    def _on_entrance_selected(self, cur: QListWidgetItem | None, _prev) -> None:
        if cur is None:
            return
        data = cur.data(Qt.UserRole)
        if not data:
            return
        x, y, _sid, _which = data
        self.overview.select_cell(x, y)
        self.overview.ensure_visible(x, y)

    def _save(self) -> None:
        wm = self.ctx.world_map
        key = self._current_layer_key()
        if not wm or not key or key not in wm.layers:
            return
        try:
            wm.layers[key].save(backup=True)
            QMessageBox.information(self, "保存", f"已保存 {wm.layers[key].path}")
        except Exception as e:
            QMessageBox.critical(self, "失败", str(e))

    def _export_region(self) -> None:
        wm = self.ctx.world_map
        key = self._current_layer_key()
        if not wm or not key or key not in wm.layers:
            return
        rect = self.overview.selection_engine_rect()
        if rect is None:
            QMessageBox.information(self, "导出选区", "请先用「框选」工具拖出矩形选区")
            return
        x0, y0, x1, y1 = rect
        path, _ = QFileDialog.getSaveFileName(
            self,
            "导出选区 JSON",
            f"{key}_{x0}_{y0}_{x1}_{y1}.json",
            "JSON (*.json)",
        )
        if not path:
            return
        payload = export_layer_region_json(key, wm.layers[key], x0, y0, x1, y1)
        save_layer_region_json(path, payload)
        self.ctx.statusMessage.emit(f"已导出选区 → {path}")

    def _import_region(self) -> None:
        wm = self.ctx.world_map
        key = self._current_layer_key()
        if not wm or not key or key not in wm.layers:
            return
        path, _ = QFileDialog.getOpenFileName(self, "导入选区 JSON", "", "JSON (*.json)")
        if not path:
            return
        try:
            payload = load_layer_region_json(path)
            data = payload["data"]
            rect = self.overview.selection_engine_rect()
            if rect:
                ox, oy = rect[0], rect[1]
            else:
                ox = int(payload.get("x0", 0))
                oy = int(payload.get("y0", 0))
            n = wm.layers[key].paste_rect(ox, oy, data)
            self._reload()
            QMessageBox.information(
                self,
                "导入选区",
                f"已写入 {n} 格到层 {key} @({ox},{oy})\n（未改入口；记得保存 .002）",
            )
        except Exception as e:
            QMessageBox.critical(self, "导入失败", str(e))

    def _export_layer_file(self) -> None:
        wm = self.ctx.world_map
        key = self._current_layer_key()
        if not wm or not key or key not in wm.layers:
            return
        grid = wm.layers[key]
        path, _ = QFileDialog.getSaveFileName(
            self, "导出整层", f"{key}.002", "Layer (*.002);;All (*.*)"
        )
        if not path:
            return
        Path(path).write_bytes(grid.to_bytes())
        self.ctx.statusMessage.emit(f"已导出 {path}")

    def _import_layer_file(self) -> None:
        wm = self.ctx.world_map
        key = self._current_layer_key()
        if not wm or not key or key not in wm.layers:
            return
        path, _ = QFileDialog.getOpenFileName(
            self, "导入整层 .002", "", "Layer (*.002);;All (*.*)"
        )
        if not path:
            return
        try:
            grid = wm.layers[key]
            dest = grid.path
            grid.load(path)
            grid.path = dest  # 仍保存回资源目录原路径
            self._reload()
            QMessageBox.information(
                self,
                "导入整层",
                f"已加载到内存层 {key}。\n点「保存当前层 .002」才会写回：\n{dest}",
            )
        except Exception as e:
            QMessageBox.critical(self, "导入失败", str(e))

    def _open_tile_io(self, kind: str) -> None:
        """Jump hint: tile import/export lives in 贴图 tab RLE panel."""
        QMessageBox.information(
            self,
            "砖库导入导出",
            "请切换到「贴图」页的 RLE 砖库面板：\n"
            f"• {'mmap（大地图/建筑）' if kind == 'mmap' else 'smp/sdx（场景砖）'}\n"
            "支持单帧/批量 PNG 导出与导入，以及保存 idx+grp。",
        )


def _hsv_rgb(h: float, s: float, v: float) -> tuple[int, int, int]:
    """Simple HSV→RGB (h in degrees) without Qt dependency in markers."""
    h = (h % 360) / 60.0
    c = v * s
    x = c * (1 - abs(h % 2 - 1))
    m = v - c
    if 0 <= h < 1:
        r, g, b = c, x, 0
    elif 1 <= h < 2:
        r, g, b = x, c, 0
    elif 2 <= h < 3:
        r, g, b = 0, c, x
    elif 3 <= h < 4:
        r, g, b = 0, x, c
    elif 4 <= h < 5:
        r, g, b = x, 0, c
    else:
        r, g, b = c, 0, x
    return int((r + m) * 255), int((g + m) * 255), int((b + m) * 255)
