"""Top-down map overview: tile-code → color blocks + hover real-tile tooltip."""

from __future__ import annotations

from dataclasses import dataclass
from typing import Callable, List, Optional, Sequence, Tuple

from PySide6.QtCore import Qt, Signal, QPoint, QRect, QSize
from PySide6.QtGui import QColor, QImage, QPainter, QPen, QPixmap, QFont, QBrush
from PySide6.QtWidgets import (
    QWidget, QVBoxLayout, QHBoxLayout, QLabel, QSpinBox, QPushButton,
    QCheckBox, QScrollArea, QFrame, QToolTip, QSizePolicy, QComboBox,
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

    cellClicked = Signal(int, int)  # x, y (engine axes), left button
    cellRightClicked = Signal(int, int)
    cellHovered = Signal(int, int)
    # Drag interactions in *visual* cell coordinates
    cellPressed = Signal(int, int, int)  # vx, vy, button
    cellDragged = Signal(int, int, int)  # vx, vy, button
    cellReleased = Signal(int, int, int)

    def __init__(self, parent=None) -> None:
        super().__init__(parent)
        self.setMouseTracking(True)
        self.setFocusPolicy(Qt.StrongFocus)
        self.cell = 8
        self.width_cells = 64
        self.height_cells = 64
        # flat list row-major by engine (x,y): colors[x][y]
        self.colors: List[List[Tuple[int, int, int]]] = []
        self.overlay: List[List[Optional[Tuple[int, int, int, int]]]] = []  # RGBA marks
        self.markers: List[MapMarker] = []
        self.show_marker_labels = True
        self.selected: Optional[Tuple[int, int]] = None
        # Inclusive visual selection rectangle (vx0, vy0, vx1, vy1)
        self.selection: Optional[Tuple[int, int, int, int]] = None
        self._tooltip_fn: Optional[Callable[[int, int], str]] = None
        self._pixmap_fn: Optional[Callable[[int, int], Optional[QPixmap]]] = None
        self._hover: Optional[Tuple[int, int]] = None
        self._last_size: Tuple[int, int] = (0, 0)
        self._drag_button: Optional[int] = None
        self._drag_last: Optional[Tuple[int, int]] = None
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
        w = self.width_cells * self.cell
        h = self.height_cells * self.cell
        if (w, h) == self._last_size:
            return
        self._last_size = (w, h)
        self.setFixedSize(w, h)

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
        if self.selection is not None:
            x0, y0, x1, y1 = self.selection
            xa, xb = sorted((x0, x1))
            ya, yb = sorted((y0, y1))
            p.fillRect(
                xa * cell,
                ya * cell,
                (xb - xa + 1) * cell,
                (yb - ya + 1) * cell,
                QColor(64, 160, 255, 70),
            )
            p.setPen(QPen(QColor(80, 180, 255), max(1, cell // 3)))
            p.drawRect(xa * cell, ya * cell, (xb - xa + 1) * cell - 1, (yb - ya + 1) * cell - 1)
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
        cell = self.cell
        if cell <= 0:
            return None
        x = int(pos.x()) // cell
        y = int(pos.y()) // cell
        if 0 <= x < self.width_cells and 0 <= y < self.height_cells:
            return x, y
        return None

    def leaveEvent(self, event) -> None:
        self._hover = None
        QToolTip.hideText()
        super().leaveEvent(event)

    def mousePressEvent(self, event) -> None:
        cell = self._pos_to_cell(event.position())
        if cell is None:
            return
        self.selected = cell
        self._drag_button = int(event.button().value)
        self._drag_last = cell
        self.update()
        self.cellPressed.emit(cell[0], cell[1], int(event.button().value))
        if event.button() == Qt.RightButton:
            self.cellRightClicked.emit(cell[0], cell[1])
        else:
            self.cellClicked.emit(cell[0], cell[1])

    def mouseMoveEvent(self, event) -> None:
        cell = self._pos_to_cell(event.position())
        if cell is None:
            QToolTip.hideText()
            self._hover = None
            return
        if cell != self._hover:
            self._hover = cell
            self.cellHovered.emit(cell[0], cell[1])
        if self._drag_button is not None and cell != self._drag_last:
            self._drag_last = cell
            self.selected = cell
            self.cellDragged.emit(cell[0], cell[1], self._drag_button)
            self.update()

    def mouseReleaseEvent(self, event) -> None:
        cell = self._pos_to_cell(event.position())
        btn = int(event.button().value)
        if cell is not None:
            self.cellReleased.emit(cell[0], cell[1], btn)
        self._drag_button = None
        self._drag_last = None
        super().mouseReleaseEvent(event)

class MapOverviewPanel(QWidget):
    """Reusable overview: color-mapped grid + tile preview + paint value."""

    cellSelected = Signal(int, int)
    cellEdited = Signal(int, int, int)  # x, y, new_code
    regionEdited = Signal()  # batch edit finished

    def __init__(self, title: str = "俯视图") -> None:
        super().__init__()
        self.setFocusPolicy(Qt.StrongFocus)
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
        self._swap_display_xy = True
        self.tile_pack: Optional[RleTilePack] = None
        self.palette: Optional[List[Tuple[int, int, int]]] = None
        self.adjust_mode = False
        self.tool_mode = "paint"  # paint | select
        self._clipboard: Optional[List[List[int]]] = None  # [dx][dy] engine codes
        self._sel_anchor: Optional[Tuple[int, int]] = None  # visual
        self._batch_dirty = False
        # full: hover updates text+preview; coords: one-line hover (stable); off: click only
        self.hover_inspect_mode = "full"
        self._last_info_cell: Optional[Tuple[int, int]] = None

        root = QVBoxLayout(self)
        bar = QHBoxLayout()
        bar.addWidget(QLabel(title))
        self.btn_adjust = QPushButton("进入调整模式")
        self.btn_adjust.setCheckable(True)
        self.btn_adjust.setToolTip(
            "开启后才能在俯视图上用鼠标改格子的值；关闭时左键只选中、右键吸取笔刷。"
        )
        self.btn_adjust.toggled.connect(self._on_adjust_toggled)
        bar.addWidget(self.btn_adjust)
        bar.addWidget(QLabel("工具"))
        self.tool_combo = QComboBox()
        self.tool_combo.addItem("画笔(拖动画)", "paint")
        self.tool_combo.addItem("框选(复制粘贴)", "select")
        self.tool_combo.currentIndexChanged.connect(self._on_tool_changed)
        bar.addWidget(self.tool_combo)
        self.btn_copy = QPushButton("复制")
        self.btn_copy.setToolTip("复制选区图层值 (Ctrl+C)；不改动入口")
        self.btn_copy.clicked.connect(self.copy_selection)
        bar.addWidget(self.btn_copy)
        self.btn_paste = QPushButton("粘贴")
        self.btn_paste.setToolTip("粘贴到选区左上 / 当前格 (Ctrl+V)")
        self.btn_paste.clicked.connect(self.paste_clipboard)
        bar.addWidget(self.btn_paste)
        self.btn_fill = QPushButton("填充")
        self.btn_fill.setToolTip("用笔刷值填充选区")
        self.btn_fill.clicked.connect(self.fill_selection)
        bar.addWidget(self.btn_fill)
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
        self.scroll.setAlignment(Qt.AlignLeft | Qt.AlignTop)
        self.scroll.setHorizontalScrollBarPolicy(Qt.ScrollBarAlwaysOn)
        self.scroll.setVerticalScrollBarPolicy(Qt.ScrollBarAlwaysOn)
        self.scroll.setFrameShape(QFrame.NoFrame)
        self.canvas = MapCanvas()
        self.canvas.cellClicked.connect(self._on_click)
        self.canvas.cellRightClicked.connect(self._on_right_click)
        self.canvas.cellHovered.connect(self._on_hover)
        self.canvas.cellPressed.connect(self._on_cell_pressed)
        self.canvas.cellDragged.connect(self._on_cell_dragged)
        self.canvas.cellReleased.connect(self._on_cell_released)
        self.scroll.setWidget(self.canvas)
        body.addWidget(self.scroll, 3)

        side = QVBoxLayout()
        self._side_panel = QWidget()
        self._side_panel.setFixedWidth(200)
        side_inner = QVBoxLayout(self._side_panel)
        side_inner.setContentsMargins(0, 0, 0, 0)
        self.lbl_info = QLabel("格: -")
        self.lbl_info.setWordWrap(True)
        self.lbl_info.setFixedHeight(88)
        side_inner.addWidget(self.lbl_info)
        self.preview = QLabel("贴图预览")
        self.preview.setFixedSize(128, 128)
        self.preview.setAlignment(Qt.AlignCenter)
        self.preview.setStyleSheet("background:#111;color:#888;border:1px solid #333;")
        side_inner.addWidget(self.preview)
        paint_row = QHBoxLayout()
        paint_row.addWidget(QLabel("笔刷值"))
        self.sp_brush = QSpinBox()
        self.sp_brush.setRange(-1, 32767)
        self.sp_brush.setValue(-1)
        self.sp_brush.setToolTip("写入格子的事件号/图层值；-1 表示该格无事件。")
        paint_row.addWidget(self.sp_brush)
        side_inner.addLayout(paint_row)
        self.lbl_sel = QLabel("选区: 无")
        self.lbl_sel.setWordWrap(True)
        side_inner.addWidget(self.lbl_sel)
        self.lbl_hint = QLabel()
        self.lbl_hint.setWordWrap(True)
        self.lbl_hint.setTextInteractionFlags(Qt.TextSelectableByMouse)
        self.set_help_preset("generic")
        side_inner.addWidget(self.lbl_hint)
        side_inner.addStretch()
        side.addWidget(self._side_panel)
        body.addLayout(side, 1)
        root.addLayout(body)

        self.canvas.set_tooltip_providers(self._tooltip_text, self._tooltip_pixmap)
        self._help_preset = "generic"
        self._update_tool_buttons()

    def set_help_preset(self, preset: str) -> None:
        """Update side-panel instructions (generic map vs SData event layer)."""
        self._help_preset = preset
        if preset == "sdata_event":
            self.hover_inspect_mode = "coords"
            self.lbl_hint.setText(
                "【SData 事件层】格子里存的是事件号（与 DData 行号一致），-1=空。\n"
                "① 先点「进入调整模式」才能改图。\n"
                "② 左键：用笔刷值画到当前格（写入左侧「编辑层」，默认 3）。\n"
                "③ 右键：未开调整模式→把该格事件号抄到笔刷；"
                "已开调整模式→将该格清为 -1。\n"
                "④ 右侧 64×64 表：行=X、列=Y，可直接改数字；与俯视图联动。\n"
                "⑤ 悬停格子时请看俯视图右侧「格信息」，不要依赖浮动提示。\n"
                "⑥ 改完务必「保存当前槽 SData」。红色半透明=层3有事件。"
            )
            self.btn_adjust.setText("进入调整模式（改格）")
        else:
            self.hover_inspect_mode = "full"
            self.lbl_hint.setText(
                "浏览：左键选中格；悬停看编号，右侧看贴图。\n"
                "改值：先「进入调整模式」。\n"
                "画笔：左键按住拖动连续绘制；右键未开模式吸取、开模式写 -1。\n"
                "框选：拖出矩形后「复制/粘贴/填充」(Ctrl+C/V)；只改图层贴图码，不动入口。"
            )
            self.btn_adjust.setText("进入调整模式")
        self._on_adjust_toggled(self.adjust_mode)

    def _update_tool_buttons(self) -> None:
        sel = self.tool_mode == "select" or self.canvas.selection is not None
        self.btn_copy.setEnabled(True)
        self.btn_paste.setEnabled(self._clipboard is not None)
        self.btn_fill.setEnabled(self.canvas.selection is not None)

    def _on_tool_changed(self, _idx: int = 0) -> None:
        self.tool_mode = self.tool_combo.currentData() or "paint"
        self._update_tool_buttons()

    def selection_engine_rect(self) -> Optional[Tuple[int, int, int, int]]:
        """Return inclusive engine (x0,y0,x1,y1) or None."""
        sel = self.canvas.selection
        if sel is None:
            return None
        vx0, vy0, vx1, vy1 = sel
        e00 = self._visual_to_engine(vx0, vy0)
        e11 = self._visual_to_engine(vx1, vy1)
        x0, x1 = sorted((e00[0], e11[0]))
        y0, y1 = sorted((e00[1], e11[1]))
        return x0, y0, x1, y1

    def _set_visual_selection(self, vx0: int, vy0: int, vx1: int, vy1: int) -> None:
        self.canvas.selection = (vx0, vy0, vx1, vy1)
        rect = self.selection_engine_rect()
        if rect:
            x0, y0, x1, y1 = rect
            self.lbl_sel.setText(
                f"选区引擎 X={x0}..{x1}, Y={y0}..{y1}\n"
                f"({x1 - x0 + 1}×{y1 - y0 + 1})"
            )
        self.canvas.update()
        self._update_tool_buttons()

    def clear_selection(self) -> None:
        self.canvas.selection = None
        self._sel_anchor = None
        self.lbl_sel.setText("选区: 无")
        self.canvas.update()
        self._update_tool_buttons()

    def copy_selection(self) -> bool:
        rect = self.selection_engine_rect()
        if rect is None or not self._get_code:
            return False
        x0, y0, x1, y1 = rect
        data: List[List[int]] = []
        for x in range(x0, x1 + 1):
            col = [int(self._get_code(x, y)) for y in range(y0, y1 + 1)]
            data.append(col)
        self._clipboard = data
        self._update_tool_buttons()
        self.lbl_sel.setText(
            self.lbl_sel.text() + f"\n已复制 {len(data)}×{len(data[0]) if data else 0}"
        )
        return True

    def paste_clipboard(self, origin: Optional[Tuple[int, int]] = None) -> int:
        """Paste clipboard at engine origin (default: selection top-left or selected)."""
        if not self._clipboard or not self._set_code or not self.adjust_mode:
            return 0
        if origin is None:
            rect = self.selection_engine_rect()
            if rect:
                origin = (rect[0], rect[1])
            elif self.canvas.selected is not None:
                origin = self._visual_to_engine(*self.canvas.selected)
            else:
                return 0
        ox, oy = origin
        n = 0
        scroll_pos = self._preserve_scroll()
        for dx, col in enumerate(self._clipboard):
            for dy, val in enumerate(col):
                x, y = ox + dx, oy + dy
                if 0 <= x < self._w and 0 <= y < self._h:
                    self._set_code(x, y, int(val))
                    self.cellEdited.emit(x, y, int(val))
                    n += 1
        if n:
            self.rebuild()
            self.regionEdited.emit()
        self._restore_scroll(scroll_pos)
        return n

    def fill_selection(self) -> int:
        if not self.adjust_mode or not self._set_code:
            return 0
        rect = self.selection_engine_rect()
        if rect is None:
            return 0
        x0, y0, x1, y1 = rect
        val = self.sp_brush.value()
        n = 0
        scroll_pos = self._preserve_scroll()
        for x in range(x0, x1 + 1):
            for y in range(y0, y1 + 1):
                self._set_code(x, y, val)
                self.cellEdited.emit(x, y, val)
                n += 1
        if n:
            self.rebuild()
            self.regionEdited.emit()
        self._restore_scroll(scroll_pos)
        return n

    def keyPressEvent(self, event) -> None:
        key = event.key()
        mods = event.modifiers()
        if mods & Qt.ControlModifier:
            if key == Qt.Key_C:
                self.copy_selection()
                return
            if key == Qt.Key_V:
                self.paste_clipboard()
                return
            if key == Qt.Key_A and self._w and self._h:
                self._set_visual_selection(0, 0, self._w - 1, self._h - 1)
                return
        if key == Qt.Key_Escape:
            self.clear_selection()
            return
        super().keyPressEvent(event)
    def _paint_engine_cell(self, ex: int, ey: int, value: Optional[int] = None) -> None:
        if not self.adjust_mode or not self._set_code:
            return
        if not (0 <= ex < self._w and 0 <= ey < self._h):
            return
        val = self.sp_brush.value() if value is None else value
        self._set_code(ex, ey, val)
        self.cellEdited.emit(ex, ey, val)
        self._batch_dirty = True

    def _on_cell_pressed(self, vx: int, vy: int, button: int) -> None:
        self.setFocus(Qt.MouseFocusReason)
        if button != int(Qt.LeftButton.value):
            return
        if self.tool_mode == "select":
            self._sel_anchor = (vx, vy)
            self._set_visual_selection(vx, vy, vx, vy)
            return
        if self.adjust_mode and self.tool_mode == "paint":
            ex, ey = self._visual_to_engine(vx, vy)
            self._batch_dirty = False
            self._paint_engine_cell(ex, ey)

    def _on_cell_dragged(self, vx: int, vy: int, button: int) -> None:
        if button != int(Qt.LeftButton.value):
            return
        if self.tool_mode == "select" and self._sel_anchor is not None:
            ax, ay = self._sel_anchor
            self._set_visual_selection(ax, ay, vx, vy)
            return
        if self.adjust_mode and self.tool_mode == "paint":
            ex, ey = self._visual_to_engine(vx, vy)
            self._paint_engine_cell(ex, ey)

    def _on_cell_released(self, vx: int, vy: int, button: int) -> None:
        if button != int(Qt.LeftButton.value):
            return
        if self.tool_mode == "select" and self._sel_anchor is not None:
            ax, ay = self._sel_anchor
            self._set_visual_selection(ax, ay, vx, vy)
            self._sel_anchor = None
        if self._batch_dirty:
            scroll_pos = self._preserve_scroll()
            self.rebuild()
            self.regionEdited.emit()
            self._restore_scroll(scroll_pos)
            self._batch_dirty = False

    def _visual_to_engine(self, vx: int, vy: int) -> Tuple[int, int]:
        """Map canvas cell → on-disk / in-game (engine) coordinates."""
        if self._swap_display_xy:
            return vy, vx
        return vx, vy

    def _engine_to_visual(self, ex: int, ey: int) -> Tuple[int, int]:
        """Engine coordinates → canvas cell for painting/selection."""
        if self._swap_display_xy:
            return ey, ex
        return ex, ey

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
        swap_display_xy: bool = True,
    ) -> None:
        self._w, self._h = width, height
        self._swap_display_xy = swap_display_xy
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
        """Scroll so that engine cell (x,y) is roughly centered."""
        vx, vy = self._engine_to_visual(x, y)
        cell = self.canvas.cell
        cx = vx * cell + cell // 2
        cy = vy * cell + cell // 2
        self.scroll.ensureVisible(
            cx,
            cy,
            max(40, self.scroll.viewport().width() // 3),
            max(40, self.scroll.viewport().height() // 3),
        )

    def _on_adjust_toggled(self, on: bool) -> None:
        self.adjust_mode = on
        if self._help_preset == "sdata_event":
            self.btn_adjust.setText("退出调整模式（只浏览）" if on else "进入调整模式（改格）")
        else:
            self.btn_adjust.setText("退出调整模式" if on else "进入调整模式")
        if on:
            self.lbl_hint.setStyleSheet("color: #ffcc66;")
        else:
            self.lbl_hint.setStyleSheet("")

    def _on_cell_size(self, v: int) -> None:
        self.canvas.set_cell_size(v)

    def rebuild(self) -> None:
        if not self._get_code or not self._ground_code:
            return
        use_true = self.chk_true_color.isChecked() and self.tile_pack and self.palette
        colors: List[List[Tuple[int, int, int]]] = []
        overlay: List[List[Optional[Tuple[int, int, int, int]]]] = []
        for vx in range(self._w):
            crow: List[Tuple[int, int, int]] = []
            orow: List[Optional[Tuple[int, int, int, int]]] = []
            for vy in range(self._h):
                ex, ey = self._visual_to_engine(vx, vy)
                gcode = self._ground_code(ex, ey)
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
                    ev = self._event_code(ex, ey)
                    if ev >= 0:
                        mark = (255, 64, 64, 140)
                orow.append(mark)
            colors.append(crow)
            overlay.append(orow)
        self.canvas.set_grid(colors, overlay)
        if self.chk_markers.isChecked() and self._markers:
            vis_markers = [
                MapMarker(
                    *self._engine_to_visual(m.x, m.y),
                    m.label,
                    m.color,
                )
                for m in self._markers
            ]
            self.canvas.set_markers(
                vis_markers,
                show_labels=self.chk_marker_labels.isChecked(),
            )
        else:
            self.canvas.set_markers([])

    def _on_click(self, vx: int, vy: int) -> None:
        ex, ey = self._visual_to_engine(vx, vy)
        scroll_pos = self._preserve_scroll()
        self.cellSelected.emit(ex, ey)
        self._last_info_cell = None
        self._show_cell(ex, ey)
        # Painting is handled by pressed/dragged for paint tool to support drag-batch.
        # Keep single-click paint only if press handler didn't (legacy safety): skip here.
        self._restore_scroll(scroll_pos)

    def _on_right_click(self, vx: int, vy: int) -> None:
        ex, ey = self._visual_to_engine(vx, vy)
        scroll_pos = self._preserve_scroll()
        self.cellSelected.emit(ex, ey)
        self._last_info_cell = None
        self._show_cell(ex, ey)
        if not self._get_code:
            self._restore_scroll(scroll_pos)
            return
        cur = int(self._get_code(ex, ey))
        if self.adjust_mode and self._set_code is not None:
            self._set_code(ex, ey, -1)
            self.cellEdited.emit(ex, ey, -1)
            self.rebuild()
            self._restore_scroll(scroll_pos)
            return
        self.sp_brush.setValue(cur)
        self._restore_scroll(scroll_pos)

    def _preserve_scroll(self) -> Tuple[int, int]:
        return (
            self.scroll.verticalScrollBar().value(),
            self.scroll.horizontalScrollBar().value(),
        )

    def _restore_scroll(self, pos: Tuple[int, int]) -> None:
        vy, hx = pos
        self.scroll.verticalScrollBar().setValue(vy)
        self.scroll.horizontalScrollBar().setValue(hx)

    def _on_hover(self, vx: int, vy: int) -> None:
        ex, ey = self._visual_to_engine(vx, vy)
        if self.hover_inspect_mode == "off":
            return
        scroll_pos = self._preserve_scroll()
        if self.hover_inspect_mode == "coords":
            if self._last_info_cell == (ex, ey):
                self._restore_scroll(scroll_pos)
                return
            self._last_info_cell = (ex, ey)
            if self._get_code:
                code = int(self._get_code(ex, ey))
                eid = ""
                if self._event_code is not None:
                    eid = f"  层3事件={self._event_code(ex, ey)}"
                ax = "（俯视图横=引擎Y，纵=引擎X）" if self._swap_display_xy else ""
                self.lbl_info.setText(f"引擎格 (X={ex}, Y={ey})  值={code}{eid}{ax}")
            self._restore_scroll(scroll_pos)
            return
        if self._last_info_cell == (ex, ey):
            self._restore_scroll(scroll_pos)
            return
        self._last_info_cell = (ex, ey)
        self._show_cell(ex, ey)
        self._restore_scroll(scroll_pos)

    def _show_cell(self, x: int, y: int) -> None:
        if not self._get_code:
            return
        code = self._get_code(x, y)
        gcode = self._ground_code(x, y) if self._ground_code else code
        lines = [f"引擎格 X={x}, Y={y}", f"当前值: {code}", f"地面代码: {gcode}"]
        if self._swap_display_xy:
            vx, vy = self._engine_to_visual(x, y)
            lines.append(f"俯视图格 (列={vx}, 行={vy})")
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
            self.preview.setText("")
            self.preview.setPixmap(
                pm.scaled(120, 120, Qt.KeepAspectRatio, Qt.SmoothTransformation)
            )

    def select_cell(self, x: int, y: int) -> None:
        """Select engine coordinates (x, y)."""
        scroll_pos = self._preserve_scroll()
        vx, vy = self._engine_to_visual(x, y)
        self.canvas.selected = (vx, vy)
        self.canvas.update()
        self._last_info_cell = None
        self._show_cell(x, y)
        self._restore_scroll(scroll_pos)

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
