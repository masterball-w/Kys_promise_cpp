"""Top-down map overview: tile-code → color blocks + hover real-tile tooltip."""

from __future__ import annotations

from dataclasses import dataclass
from typing import Callable, List, Optional, Sequence, Tuple

from PySide6.QtCore import Qt, Signal, QPoint, QRect, QSize
from PySide6.QtGui import QColor, QImage, QPainter, QPen, QPixmap, QFont, QBrush
from PySide6.QtWidgets import (
    QWidget, QVBoxLayout, QHBoxLayout, QLabel, QSpinBox, QPushButton,
    QCheckBox, QScrollArea, QFrame, QToolTip, QSizePolicy,
)

from kys_formats.rle_tile import RleTilePack, code_to_tile_index

try:
    from PIL import Image
except ImportError:  # pragma: no cover
    Image = None  # type: ignore


def pil_to_qpixmap(img) -> QPixmap:
    data = img.convert("RGBA").tobytes("raw", "RGBA")
    qimg = QImage(data, img.width, img.height, QImage.Format_RGBA8888).copy()
    return QPixmap.fromImage(qimg)


def _hash_color(code: int) -> Tuple[int, int, int]:
    """Fallback when no tile pack / empty tile."""
    if code <= 0:
        return (32, 32, 32)
    r = (code * 37) & 0xFF
    g = (code * 17 + 80) & 0xFF
    b = (code * 53 + 40) & 0xFF
    return (40 + r // 3, 40 + g // 3, 40 + b // 3)


@dataclass
class MapMarker:
    """Named point drawn on top of the grid (engine x/y axes)."""

    x: int
    y: int
    label: str
    color: Tuple[int, int, int] = (255, 64, 200)


class MapCanvas(QWidget):
    """Paints a rectangular grid of cell colors; supports click + hover tooltip."""

    cellClicked = Signal(int, int)  # x, y (engine axes)
    cellHovered = Signal(int, int)

    def __init__(self, parent=None) -> None:
        super().__init__(parent)
        self.setMouseTracking(True)
        self.cell = 8
        self.width_cells = 64
        self.height_cells = 64
        # flat list row-major by engine (x,y): colors[x][y]
        self.colors: List[List[Tuple[int, int, int]]] = []
        self.overlay: List[List[Optional[Tuple[int, int, int, int]]]] = []  # RGBA marks
        self.markers: List[MapMarker] = []
        self.show_marker_labels = True
        self.selected: Optional[Tuple[int, int]] = None
        self._tooltip_fn: Optional[Callable[[int, int], str]] = None
        self._pixmap_fn: Optional[Callable[[int, int], Optional[QPixmap]]] = None
        self._hover: Optional[Tuple[int, int]] = None
        self.setSizePolicy(QSizePolicy.Fixed, QSizePolicy.Fixed)

    def set_grid(
        self,
        colors: List[List[Tuple[int, int, int]]],
        overlay: Optional[List[List[Optional[Tuple[int, int, int, int]]]]] = None,
    ) -> None:
        self.colors = colors
        self.width_cells = len(colors)
        self.height_cells = len(colors[0]) if colors else 0
        self.overlay = overlay or []
        self._update_size()
        self.update()

    def set_markers(
        self,
        markers: Optional[Sequence[MapMarker]] = None,
        *,
        show_labels: bool = True,
    ) -> None:
        self.markers = list(markers or [])
        self.show_marker_labels = show_labels
        self.update()

    def set_cell_size(self, px: int) -> None:
        self.cell = max(2, int(px))
        self._update_size()
        self.update()

    def set_tooltip_providers(
        self,
        text_fn: Callable[[int, int], str],
        pixmap_fn: Optional[Callable[[int, int], Optional[QPixmap]]] = None,
    ) -> None:
        self._tooltip_fn = text_fn
        self._pixmap_fn = pixmap_fn

    def _update_size(self) -> None:
        self.setFixedSize(self.width_cells * self.cell, self.height_cells * self.cell)

    def sizeHint(self) -> QSize:
        return QSize(self.width_cells * self.cell, self.height_cells * self.cell)

    def paintEvent(self, _event) -> None:
        p = QPainter(self)
        p.setRenderHint(QPainter.Antialiasing, True)
        p.fillRect(self.rect(), QColor(20, 20, 20))
        cell = self.cell
        for x in range(self.width_cells):
            col = self.colors[x] if x < len(self.colors) else None
            if not col:
                continue
            for y in range(min(self.height_cells, len(col))):
                r, g, b = col[y]
                p.fillRect(x * cell, y * cell, cell, cell, QColor(r, g, b))
                if self.overlay and x < len(self.overlay) and y < len(self.overlay[x]):
                    ov = self.overlay[x][y]
                    if ov is not None:
                        p.fillRect(x * cell, y * cell, cell, cell, QColor(*ov))
        self._paint_markers(p)
        if self.selected is not None:
            sx, sy = self.selected
            p.setPen(QPen(QColor(255, 255, 0), max(1, cell // 4)))
            p.drawRect(sx * cell, sy * cell, cell - 1, cell - 1)

    def _paint_markers(self, p: QPainter) -> None:
        if not self.markers:
            return
        cell = self.cell
        # Keep pin readable even when cell is 2px.
        pin = max(6, cell + 2)
        font = QFont()
        font.setPointSize(max(8, min(11, 6 + cell // 2)))
        p.setFont(font)
        for m in self.markers:
            if not (0 <= m.x < self.width_cells and 0 <= m.y < self.height_cells):
                continue
            cx = m.x * cell + cell // 2
            cy = m.y * cell + cell // 2
            color = QColor(*m.color)
            p.setPen(QPen(QColor(0, 0, 0, 200), 1))
            p.setBrush(QBrush(color))
            p.drawEllipse(cx - pin // 2, cy - pin // 2, pin, pin)
            if not self.show_marker_labels or not m.label:
                continue
            text = m.label
            metrics = p.fontMetrics()
            tw = metrics.horizontalAdvance(text) + 6
            th = metrics.height() + 2
            tx = cx + pin // 2 + 2
            ty = cy - th // 2
            # Prefer keeping label inside canvas when near right/bottom edge.
            if tx + tw > self.width():
                tx = cx - pin // 2 - tw - 2
            if ty < 0:
                ty = 0
            if ty + th > self.height():
                ty = self.height() - th
            p.fillRect(tx, ty, tw, th, QColor(0, 0, 0, 180))
            p.setPen(QColor(255, 255, 255))
            p.drawText(tx + 3, ty + metrics.ascent() + 1, text)

    def _pos_to_cell(self, pos) -> Optional[Tuple[int, int]]:
        x = int(pos.x() // self.cell)
        y = int(pos.y() // self.cell)
        if 0 <= x < self.width_cells and 0 <= y < self.height_cells:
            return x, y
        return None

    def mousePressEvent(self, event) -> None:
        cell = self._pos_to_cell(event.position())
        if cell is None:
            return
        self.selected = cell
        self.update()
        self.cellClicked.emit(cell[0], cell[1])

    def mouseMoveEvent(self, event) -> None:
        cell = self._pos_to_cell(event.position())
        if cell is None:
            QToolTip.hideText()
            return
        if cell != self._hover:
            self._hover = cell
            self.cellHovered.emit(cell[0], cell[1])
        if self._tooltip_fn:
            text = self._tooltip_fn(cell[0], cell[1])
            # Rich tooltip with optional image
            html = f"<div style='white-space:pre'>{text}</div>"
            if self._pixmap_fn:
                pm = self._pixmap_fn(cell[0], cell[1])
                if pm is not None and not pm.isNull():
                    # QToolTip can't embed QPixmap easily; show text + status-style note
                    html += f"<br/><i>贴图 {pm.width()}×{pm.height()}（见右侧预览）</i>"
            QToolTip.showText(event.globalPosition().toPoint(), html, self)


class MapOverviewPanel(QWidget):
    """Reusable overview: color-mapped grid + tile preview + paint value."""

    cellSelected = Signal(int, int)
    cellEdited = Signal(int, int, int)  # x, y, new_code

    def __init__(self, title: str = "俯视图") -> None:
        super().__init__()
        self._get_code: Optional[Callable[[int, int], int]] = None
        self._set_code: Optional[Callable[[int, int, int], None]] = None
        self._ground_code: Optional[Callable[[int, int], int]] = None
        self._event_code: Optional[Callable[[int, int], int]] = None
        self._event_pic_code: Optional[Callable[[int, int], int]] = None
        self._markers: List[MapMarker] = []
        self._show_marker_labels = True
        self._marker_lookup: dict[Tuple[int, int], List[str]] = {}
        self._w = 64
        self._h = 64
        self.tile_pack: Optional[RleTilePack] = None
        self.palette: Optional[List[Tuple[int, int, int]]] = None
        self.adjust_mode = False

        root = QVBoxLayout(self)
        bar = QHBoxLayout()
        bar.addWidget(QLabel(title))
        self.btn_adjust = QPushButton("进入调整模式")
        self.btn_adjust.setCheckable(True)
        self.btn_adjust.toggled.connect(self._on_adjust_toggled)
        bar.addWidget(self.btn_adjust)
        bar.addWidget(QLabel("色块"))
        self.sp_cell = QSpinBox()
        self.sp_cell.setRange(2, 24)
        self.sp_cell.setValue(8)
        self.sp_cell.valueChanged.connect(self._on_cell_size)
        bar.addWidget(self.sp_cell)
        self.chk_events = QCheckBox("叠加事件")
        self.chk_events.setChecked(True)
        self.chk_events.toggled.connect(lambda _: self.rebuild())
        bar.addWidget(self.chk_events)
        self.chk_markers = QCheckBox("标记点")
        self.chk_markers.setChecked(True)
        self.chk_markers.toggled.connect(lambda _: self.rebuild())
        self.chk_markers.setVisible(False)
        bar.addWidget(self.chk_markers)
        self.chk_marker_labels = QCheckBox("标记文字")
        self.chk_marker_labels.setChecked(True)
        self.chk_marker_labels.toggled.connect(lambda _: self.rebuild())
        self.chk_marker_labels.setVisible(False)
        bar.addWidget(self.chk_marker_labels)
        self.chk_true_color = QCheckBox("真砖主色")
        self.chk_true_color.setChecked(True)
        self.chk_true_color.toggled.connect(lambda _: self.rebuild())
        bar.addWidget(self.chk_true_color)
        bar.addStretch()
        root.addLayout(bar)

        body = QHBoxLayout()
        self.scroll = QScrollArea()
        self.scroll.setWidgetResizable(False)
        self.scroll.setAlignment(Qt.AlignCenter)
        self.canvas = MapCanvas()
        self.canvas.cellClicked.connect(self._on_click)
        self.canvas.cellHovered.connect(self._on_hover)
        self.scroll.setWidget(self.canvas)
        body.addWidget(self.scroll, 3)

        side = QVBoxLayout()
        self.lbl_info = QLabel("格: -")
        self.lbl_info.setWordWrap(True)
        side.addWidget(self.lbl_info)
        self.preview = QLabel("贴图预览")
        self.preview.setFixedSize(128, 128)
        self.preview.setAlignment(Qt.AlignCenter)
        self.preview.setStyleSheet("background:#111;color:#888;border:1px solid #333;")
        side.addWidget(self.preview)
        paint_row = QHBoxLayout()
        paint_row.addWidget(QLabel("笔刷值"))
        self.sp_brush = QSpinBox()
        self.sp_brush.setRange(-1, 32767)
        self.sp_brush.setValue(-1)
        paint_row.addWidget(self.sp_brush)
        side.addLayout(paint_row)
        self.lbl_hint = QLabel(
            "调整模式：点击格子写入笔刷值。\n"
            "悬停显示编号；右侧显示真实贴图块。"
        )
        self.lbl_hint.setWordWrap(True)
        side.addWidget(self.lbl_hint)
        side.addStretch()
        body.addLayout(side, 1)
        root.addLayout(body)

        self.canvas.set_tooltip_providers(self._tooltip_text, self._tooltip_pixmap)

    def bind(
        self,
        width: int,
        height: int,
        get_code: Callable[[int, int], int],
        set_code: Optional[Callable[[int, int, int], None]] = None,
        *,
        ground_code: Optional[Callable[[int, int], int]] = None,
        event_code: Optional[Callable[[int, int], int]] = None,
        event_pic_code: Optional[Callable[[int, int], int]] = None,
        markers: Optional[Sequence[MapMarker]] = None,
        tile_pack: Optional[RleTilePack] = None,
        palette: Optional[Sequence[Tuple[int, int, int]]] = None,
    ) -> None:
        self._w, self._h = width, height
        self._get_code = get_code
        self._set_code = set_code
        self._ground_code = ground_code or get_code
        self._event_code = event_code
        self._event_pic_code = event_pic_code
        self._markers = list(markers or [])
        self._marker_lookup = {}
        for m in self._markers:
            self._marker_lookup.setdefault((m.x, m.y), []).append(m.label)
        self.tile_pack = tile_pack
        self.palette = list(palette) if palette else None
        self.chk_events.setVisible(event_code is not None)
        has_markers = bool(self._markers)
        self.chk_markers.setVisible(has_markers)
        self.chk_marker_labels.setVisible(has_markers)
        if has_markers:
            self.chk_markers.setChecked(True)
            self.chk_marker_labels.setChecked(True)
        self.rebuild()

    def set_markers(
        self,
        markers: Optional[Sequence[MapMarker]] = None,
        *,
        show: Optional[bool] = None,
        show_labels: Optional[bool] = None,
    ) -> None:
        self._markers = list(markers or [])
        self._marker_lookup = {}
        for m in self._markers:
            self._marker_lookup.setdefault((m.x, m.y), []).append(m.label)
        has_markers = bool(self._markers)
        self.chk_markers.setVisible(has_markers)
        self.chk_marker_labels.setVisible(has_markers)
        if show is not None:
            self.chk_markers.setChecked(show)
        if show_labels is not None:
            self.chk_marker_labels.setChecked(show_labels)
        self.rebuild()

    def ensure_visible(self, x: int, y: int) -> None:
        """Scroll so that cell (x,y) is roughly centered."""
        cell = self.canvas.cell
        cx = x * cell + cell // 2
        cy = y * cell + cell // 2
        self.scroll.ensureVisible(
            cx,
            cy,
            max(40, self.scroll.viewport().width() // 3),
            max(40, self.scroll.viewport().height() // 3),
        )

    def _on_adjust_toggled(self, on: bool) -> None:
        self.adjust_mode = on
        self.btn_adjust.setText("退出调整模式" if on else "进入调整模式")

    def _on_cell_size(self, v: int) -> None:
        self.canvas.set_cell_size(v)

    def rebuild(self) -> None:
        if not self._get_code or not self._ground_code:
            return
        use_true = self.chk_true_color.isChecked() and self.tile_pack and self.palette
        colors: List[List[Tuple[int, int, int]]] = []
        overlay: List[List[Optional[Tuple[int, int, int, int]]]] = []
        for x in range(self._w):
            crow: List[Tuple[int, int, int]] = []
            orow: List[Optional[Tuple[int, int, int, int]]] = []
            for y in range(self._h):
                gcode = self._ground_code(x, y)
                if use_true:
                    idx = code_to_tile_index(gcode)
                    if idx >= 0:
                        crow.append(self.tile_pack.average_color(idx, self.palette))
                    else:
                        crow.append((28, 28, 28))
                else:
                    crow.append(_hash_color(gcode))
                mark = None
                if self.chk_events.isChecked() and self._event_code is not None:
                    ev = self._event_code(x, y)
                    if ev >= 0:
                        mark = (255, 64, 64, 140)
                orow.append(mark)
            colors.append(crow)
            overlay.append(orow)
        self.canvas.set_grid(colors, overlay)
        if self.chk_markers.isChecked() and self._markers:
            self.canvas.set_markers(
                self._markers,
                show_labels=self.chk_marker_labels.isChecked(),
            )
        else:
            self.canvas.set_markers([])

    def _on_click(self, x: int, y: int) -> None:
        self.cellSelected.emit(x, y)
        self._show_cell(x, y)
        if self.adjust_mode and self._set_code is not None:
            val = self.sp_brush.value()
            self._set_code(x, y, val)
            self.cellEdited.emit(x, y, val)
            self.rebuild()

    def _on_hover(self, x: int, y: int) -> None:
        self._show_cell(x, y)

    def _show_cell(self, x: int, y: int) -> None:
        if not self._get_code:
            return
        code = self._get_code(x, y)
        gcode = self._ground_code(x, y) if self._ground_code else code
        lines = [f"格 ({x}, {y})", f"当前值: {code}", f"地面代码: {gcode}"]
        marks = self._marker_lookup.get((x, y))
        if marks:
            lines.append("入口: " + "；".join(marks))
        eid = -1
        if self._event_code is not None:
            eid = self._event_code(x, y)
            lines.append(f"事件: {eid}")
        g_idx = code_to_tile_index(gcode)
        lines.append(f"地面砖 smp: {g_idx} (=地面/2)")

        preview_idx = g_idx
        epic = 0
        if eid >= 0 and self._event_pic_code is not None:
            epic = int(self._event_pic_code(x, y))
            if epic != 0:
                e_idx = code_to_tile_index(epic)
                lines.append(f"事件贴图代码: {epic}")
                lines.append(f"事件 smp: {e_idx} (=贴图/2)")
                if epic > 0:
                    preview_idx = e_idx

        self.lbl_info.setText("\n".join(lines))
        pm = self._tile_pixmap(preview_idx)
        if pm is None:
            self.preview.setPixmap(QPixmap())
            self.preview.setText("无贴图" if epic >= 0 else "负贴图")
        else:
            self.preview.setPixmap(
                pm.scaled(120, 120, Qt.KeepAspectRatio, Qt.SmoothTransformation)
            )

    def _tile_pixmap(self, idx: int) -> Optional[QPixmap]:
        if idx < 0 or not self.tile_pack or not self.palette:
            return None
        try:
            img = self.tile_pack.decode_tile(idx, self.palette)
        except Exception:
            return None
        if img is None:
            return None
        return pil_to_qpixmap(img)

    def _tooltip_text(self, x: int, y: int) -> str:
        if not self._get_code:
            return ""
        code = self._get_code(x, y)
        gcode = self._ground_code(x, y) if self._ground_code else code
        idx = code_to_tile_index(gcode)
        parts = [f"({x},{y}) 值={code} 地面={gcode} 砖={idx}"]
        marks = self._marker_lookup.get((x, y))
        if marks and self.chk_markers.isChecked():
            parts.append("入口=" + "；".join(marks))
        if self._event_code is not None:
            eid = self._event_code(x, y)
            parts.append(f"事件={eid}")
            if eid >= 0 and self._event_pic_code is not None:
                epic = int(self._event_pic_code(x, y))
                if epic != 0:
                    parts.append(f"贴图={epic} smp={code_to_tile_index(epic)}")
        return "\n".join(parts)

    def _tooltip_pixmap(self, x: int, y: int) -> Optional[QPixmap]:
        if self._event_code is not None and self._event_pic_code is not None:
            eid = self._event_code(x, y)
            if eid >= 0:
                epic = int(self._event_pic_code(x, y))
                if epic > 0:
                    return self._tile_pixmap(code_to_tile_index(epic))
        if not self._ground_code:
            return None
        return self._tile_pixmap(code_to_tile_index(self._ground_code(x, y)))

    def select_cell(self, x: int, y: int) -> None:
        self.canvas.selected = (x, y)
        self.canvas.update()
        self._show_cell(x, y)
