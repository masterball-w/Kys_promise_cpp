"""Big-map (earth/surface/building .002) overview editor."""

from __future__ import annotations

from PySide6.QtCore import Qt
from PySide6.QtWidgets import (
    QWidget, QVBoxLayout, QHBoxLayout, QLabel, QComboBox, QPushButton,
    QMessageBox, QListWidget, QListWidgetItem, QSplitter,
)

from kys_formats.world_map import collect_scene_entrances, SceneEntrance
from ui.context import EditorContext
from ui.map_view import MapOverviewPanel, MapMarker


class WorldMapEditorWidget(QWidget):
    def __init__(self, ctx: EditorContext) -> None:
        super().__init__()
        self.ctx = ctx
        self._entrances: list[SceneEntrance] = []

        lay = QVBoxLayout(self)
        top = QHBoxLayout()
        top.addWidget(QLabel("大地图层"))
        self.layer_combo = QComboBox()
        self.layer_combo.currentIndexChanged.connect(self._reload)
        top.addWidget(self.layer_combo)
        save = QPushButton("保存当前层 .002")
        save.clicked.connect(self._save)
        top.addWidget(save)
        top.addStretch()
        lay.addLayout(top)
        hint = QLabel(
            "大地图 480×480；地面色来自 earth + mmap 砖库主色。"
            "粉色点为场景入口（ranger MainEntranceX/Y）；"
            "右侧列表可定位。默认色块较小以便浏览；进入调整模式后点击写入笔刷值。"
        )
        hint.setWordWrap(True)
        lay.addWidget(hint)

        split = QSplitter(Qt.Horizontal)
        self.overview = MapOverviewPanel("大地图俯视图")
        self.overview.sp_cell.setValue(2)
        self.overview.chk_events.setVisible(False)
        split.addWidget(self.overview)

        right = QWidget()
        right_lay = QVBoxLayout(right)
        right_lay.setContentsMargins(4, 0, 0, 0)
        right_lay.addWidget(QLabel("场景入口（大地图坐标）"))
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
        for key in wm.layers:
            self.layer_combo.addItem(key, key)
        self.layer_combo.blockSignals(False)
        self._reload()

    def _current_layer_key(self) -> str | None:
        return self.layer_combo.currentData()

    def _build_markers(self) -> list[MapMarker]:
        self._entrances = collect_scene_entrances(self.ctx.ranger) if self.ctx.ranger else []
        markers: list[MapMarker] = []
        for e in self._entrances:
            # Distinct hue per scene so dual entrances of same scene match.
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

        def get_code(x: int, y: int) -> int:
            return grid.get(x, y)

        def set_code(x: int, y: int, v: int) -> None:
            grid.set(x, y, v)

        def ground(x: int, y: int) -> int:
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
        # Friendly checkbox titles for this page
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
