"""Lazy thumbnail list — visible rows only, bounded pixmap cache."""

from __future__ import annotations

from collections import OrderedDict
from typing import Callable, Optional

from PySide6.QtCore import Qt, QSize, QTimer, QPoint
from PySide6.QtGui import QColor, QIcon, QPainter, QPixmap
from PySide6.QtWidgets import QAbstractItemView, QListWidget, QListWidgetItem


ThumbLoader = Callable[[int], Optional[QPixmap]]


def make_placeholder(size: int = 48) -> QPixmap:
    pm = QPixmap(size, size)
    pm.fill(QColor(55, 55, 55))
    p = QPainter(pm)
    p.setPen(QColor(120, 120, 120))
    p.drawRect(0, 0, size - 1, size - 1)
    p.drawText(pm.rect(), Qt.AlignCenter, "…")
    p.end()
    return pm


def _checkerboard(size: int) -> QPixmap:
    """Light board so dark / sparse tiles stay recognizable."""
    pm = QPixmap(size, size)
    pm.fill(QColor(96, 96, 96))
    p = QPainter(pm)
    p.setPen(Qt.NoPen)
    p.setBrush(QColor(120, 120, 120))
    step = max(4, size // 8)
    for y in range(0, size, step):
        for x in range(0, size, step):
            if ((x // step) + (y // step)) & 1:
                p.drawRect(x, y, step, step)
    p.end()
    return pm


def fit_thumb(src: QPixmap, size: int) -> QPixmap:
    """Scale with aspect ratio onto a light checkerboard square."""
    if src.isNull():
        return make_placeholder(size)
    square = _checkerboard(size)
    scaled = src.scaled(size, size, Qt.KeepAspectRatio, Qt.SmoothTransformation)
    p = QPainter(square)
    p.setRenderHint(QPainter.SmoothPixmapTransform, True)
    x = (size - scaled.width()) // 2
    y = (size - scaled.height()) // 2
    p.drawPixmap(x, y, scaled)
    p.end()
    return square


class LazyThumbList(QListWidget):
    """QListWidget that fills icons for visible rows on demand."""

    def __init__(
        self,
        parent=None,
        *,
        thumb_size: int = 48,
        cache_limit: int = 320,
    ) -> None:
        super().__init__(parent)
        self._thumb_size = max(24, int(thumb_size))
        self._cache_limit = max(32, int(cache_limit))
        self._loader: Optional[ThumbLoader] = None
        self._enabled = True
        self._cache: OrderedDict[int, QPixmap] = OrderedDict()
        self._placeholder = make_placeholder(self._thumb_size)
        self._empty_icon = QIcon()
        self.setIconSize(QSize(self._thumb_size, self._thumb_size))
        # Per-item scroll makes visible-range = scrollbar value (reliable for 8k+ rows).
        self.setVerticalScrollMode(QAbstractItemView.ScrollMode.ScrollPerItem)
        self.setUniformItemSizes(True)
        self.setSpacing(1)
        self.verticalScrollBar().valueChanged.connect(self._schedule_fill)
        self.verticalScrollBar().rangeChanged.connect(lambda *_: self._schedule_fill())
        self._timer = QTimer(self)
        self._timer.setSingleShot(True)
        self._timer.setInterval(30)
        self._timer.timeout.connect(self._fill_visible)

    @property
    def thumbs_enabled(self) -> bool:
        return self._enabled

    def set_thumb_loader(self, loader: Optional[ThumbLoader]) -> None:
        self._loader = loader
        self.clear_thumb_cache()
        self._schedule_fill()

    def set_thumbs_enabled(self, enabled: bool) -> None:
        enabled = bool(enabled)
        if enabled == self._enabled:
            return
        self._enabled = enabled
        if not enabled:
            self.clear_thumb_cache()
            for i in range(self.count()):
                item = self.item(i)
                if item is not None:
                    item.setIcon(self._empty_icon)
            self.setIconSize(QSize(0, 0))
        else:
            self.setIconSize(QSize(self._thumb_size, self._thumb_size))
            for i in range(self.count()):
                item = self.item(i)
                if item is not None:
                    item.setIcon(QIcon(self._placeholder.copy()))
            self._schedule_fill()

    def set_thumb_size(self, size: int) -> None:
        self._thumb_size = max(24, int(size))
        self._placeholder = make_placeholder(self._thumb_size)
        self.clear_thumb_cache()
        if self._enabled:
            self.setIconSize(QSize(self._thumb_size, self._thumb_size))
            for i in range(self.count()):
                item = self.item(i)
                if item is not None:
                    item.setIcon(QIcon(self._placeholder.copy()))
            self._schedule_fill()

    def clear_thumb_cache(self) -> None:
        self._cache.clear()

    def invalidate(self, index: Optional[int] = None) -> None:
        if index is None:
            self.clear_thumb_cache()
        else:
            self._cache.pop(index, None)
            item = self.item(index)
            if item is not None and self._enabled:
                item.setIcon(QIcon(self._placeholder.copy()))
        self._schedule_fill()

    def rebuild_items(self, labels: list[str]) -> None:
        """Replace all rows; icons start as placeholders (lazy fill)."""
        self.blockSignals(True)
        self.clear()
        self.clear_thumb_cache()
        for text in labels:
            # Unique QIcon per row — avoid sharing one icon across thousands of items.
            if self._enabled:
                item = QListWidgetItem(QIcon(self._placeholder.copy()), text)
            else:
                item = QListWidgetItem(text)
            # Hint height so ScrollPerItem + UniformItemSizes stay consistent.
            item.setSizeHint(QSize(self._thumb_size + 80, self._thumb_size + 6))
            self.addItem(item)
        self.blockSignals(False)
        # Layout may not be ready in the same tick.
        self._schedule_fill()
        QTimer.singleShot(0, self._fill_visible)
        QTimer.singleShot(100, self._fill_visible)

    def _schedule_fill(self, *_args) -> None:
        if self._enabled and self._loader is not None and self.count() > 0:
            self._timer.start()

    def _visible_rows(self) -> range:
        n = self.count()
        if n <= 0:
            return range(0)

        # Primary: ScrollPerItem → scrollbar value is top row.
        sb = self.verticalScrollBar()
        top = int(sb.value())
        # Estimate rows in viewport from item height.
        sample = self.item(min(top, n - 1))
        item_h = self._thumb_size + 8
        if sample is not None:
            hint = sample.sizeHint()
            if hint.height() > 0:
                item_h = max(item_h, hint.height() + self.spacing())
            rect = self.visualItemRect(sample)
            if rect.height() > 0:
                item_h = max(item_h, rect.height() + self.spacing())
        vh = max(1, self.viewport().height())
        visible = max(1, vh // max(1, item_h) + 1)

        a = max(0, top - 2)
        b = min(n - 1, top + visible + 6)

        # Secondary: indexAt correction (viewport coords).
        first = self.indexAt(QPoint(2, 2))
        last = self.indexAt(QPoint(2, max(0, vh - 2)))
        if first.isValid():
            a = min(a, max(0, first.row() - 2))
        if last.isValid():
            b = max(b, min(n - 1, last.row() + 4))
        if b < a:
            b = min(n - 1, a + visible + 4)
        return range(a, b + 1)

    def _cache_get(self, index: int) -> Optional[QPixmap]:
        pm = self._cache.get(index)
        if pm is not None:
            self._cache.move_to_end(index)
        return pm

    def _cache_put(self, index: int, pm: QPixmap) -> None:
        # Keep an independent copy so LRU eviction cannot blank list icons.
        self._cache[index] = pm.copy()
        self._cache.move_to_end(index)
        while len(self._cache) > self._cache_limit:
            self._cache.popitem(last=False)

    def _fill_visible(self) -> None:
        if not self._enabled or self._loader is None:
            return
        for row in self._visible_rows():
            item = self.item(row)
            if item is None:
                continue
            cached = self._cache_get(row)
            if cached is not None:
                item.setIcon(QIcon(cached))
                continue
            try:
                pm = self._loader(row)
            except Exception:
                pm = None
            if pm is None or pm.isNull():
                item.setIcon(QIcon(self._placeholder.copy()))
                continue
            thumb = fit_thumb(pm, self._thumb_size)
            self._cache_put(row, thumb)
            item.setIcon(QIcon(thumb))

    def resizeEvent(self, event) -> None:
        super().resizeEvent(event)
        self._schedule_fill()

    def showEvent(self, event) -> None:
        super().showEvent(event)
        self._schedule_fill()
