"""Cross-reference browser: scripts ↔ battles ↔ items ↔ pics."""

from __future__ import annotations

from PySide6.QtWidgets import (
    QWidget, QVBoxLayout, QHBoxLayout, QPushButton, QTextEdit, QLabel, QSpinBox,
)

from ui.context import EditorContext


class CrossRefWidget(QWidget):
    def __init__(self, ctx: EditorContext) -> None:
        super().__init__()
        self.ctx = ctx
        lay = QVBoxLayout(self)
        row = QHBoxLayout()
        row.addWidget(QLabel("BattleNum"))
        self.battle_spin = QSpinBox(); self.battle_spin.setRange(0, 9999)
        row.addWidget(self.battle_spin)
        b1 = QPushButton("查开战脚本")
        b1.clicked.connect(self.find_battle_scripts)
        row.addWidget(b1)
        row.addWidget(QLabel("Item ID"))
        self.item_spin = QSpinBox(); self.item_spin.setRange(0, 9999)
        row.addWidget(self.item_spin)
        b2 = QPushButton("查物品相关脚本(粗扫 opcode2)")
        b2.clicked.connect(self.find_item_scripts)
        row.addWidget(b2)
        row.addStretch()
        lay.addLayout(row)
        self.out = QTextEdit()
        self.out.setReadOnly(True)
        lay.addWidget(self.out)

    def find_battle_scripts(self) -> None:
        if not self.ctx.kdef:
            self.out.setPlainText("未加载 Kdef")
            return
        bid = self.battle_spin.value()
        hits = self.ctx.kdef.find_battle_refs(bid)
        lines = [f"BattleNum {bid} 引用脚本: {hits}"]
        if self.ctx.war:
            rec = self.ctx.war.find_by_num(bid)
            if rec:
                lines.append(f"名称={rec.name} 地图={rec.battle_map} Exp={rec.exp} 敌人数={rec.enemy_count()}")
        self.out.setPlainText("\n".join(lines))

    def find_item_scripts(self) -> None:
        if not self.ctx.kdef:
            self.out.setPlainText("未加载 Kdef")
            return
        iid = self.item_spin.value()
        hits = []
        for sid in range(1, self.ctx.kdef.script_count + 1):
            script = self.ctx.kdef.get_script(sid)
            for ins in script.instructions:
                if ins.opcode == 2 and ins.args and ins.args[0] == iid:
                    hits.append(sid)
                    break
        name = ""
        if self.ctx.ranger and 0 <= iid < self.ctx.ranger.items.count:
            name = self.ctx.ranger.item_name(iid)
        self.out.setPlainText(f"物品 {iid} ({name}) 被 AddItem 脚本: {hits}")
