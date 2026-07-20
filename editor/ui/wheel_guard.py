"""Prevent accidental value changes when scrolling over SpinBox/ComboBox."""

from __future__ import annotations

from PySide6.QtCore import QObject, QEvent, Qt
from PySide6.QtWidgets import (
    QAbstractSpinBox,
    QAbstractScrollArea,
    QComboBox,
    QApplication,
    QWidget,
)


def _combo_from_object(obj: QObject) -> QComboBox | None:
    if isinstance(obj, QComboBox):
        return obj
    if isinstance(obj, QWidget):
        parent = obj.parentWidget()
        if isinstance(parent, QComboBox):
            return parent
    return None


def _combo_popup_open(combo: QComboBox) -> bool:
    view = combo.view()
    return view is not None and view.isVisible()


class WheelGuardFilter(QObject):
    """Block wheel on spin/combo unless intentionally active; forward for page scroll.

    - SpinBox: allow wheel only when focused.
    - ComboBox (incl. editable line-edit child): allow wheel only while the
      dropdown popup is open. Focus alone must not change the value — otherwise
      scrolling a table of opcode combos silently rewrites scripts.
    """

    def eventFilter(self, obj: QObject, event: QEvent) -> bool:  # noqa: N802
        if event.type() != QEvent.Type.Wheel:
            return False

        combo = _combo_from_object(obj)
        if combo is not None:
            if _combo_popup_open(combo):
                return False
            self._forward_wheel(combo, event)
            return True

        if isinstance(obj, QAbstractSpinBox):
            if obj.hasFocus():
                return False
            self._forward_wheel(obj, event)
            return True

        return False

    @staticmethod
    def _forward_wheel(widget: QWidget, event: QEvent) -> None:
        # Prefer the enclosing scroll area so table/list pages keep scrolling.
        parent = widget.parentWidget()
        while parent is not None:
            if isinstance(parent, QAbstractScrollArea):
                QApplication.sendEvent(parent.viewport(), event)
                return
            parent = parent.parentWidget()
        parent = widget.parentWidget()
        if parent is not None:
            QApplication.sendEvent(parent, event)


def install_wheel_guard(app: QApplication) -> WheelGuardFilter:
    guard = WheelGuardFilter(app)
    app.installEventFilter(guard)
    return guard


def harden_scroll_widgets(root: QWidget) -> None:
    """ClickFocus: wheel only adjusts value after the control is clicked."""
    for w in root.findChildren(QAbstractSpinBox):
        w.setFocusPolicy(Qt.FocusPolicy.ClickFocus)
    for w in root.findChildren(QComboBox):
        w.setFocusPolicy(Qt.FocusPolicy.ClickFocus)
