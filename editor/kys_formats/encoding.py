"""Text encoding helpers for talk/name and ranger fixed-width strings."""

from __future__ import annotations

from typing import Optional, Sequence, Tuple

# (value, UI label)
TEXT_ENCODING_CHOICES: Tuple[Tuple[str, str], ...] = (
    ("auto", "自动检测 (默认)"),
    ("gbk", "GBK / CP936"),
    ("big5", "Big5 / CP950"),
    ("utf-8", "UTF-8"),
)

_TALK_AUTO_ORDER: Tuple[str, ...] = ("gbk", "cp936", "big5", "cp950", "utf-8")
_RANGER_AUTO_ORDER: Tuple[str, ...] = ("gbk", "big5", "cp950", "latin1")


def normalize_encoding(enc: str) -> str:
    key = (enc or "auto").lower().strip()
    if key in ("gbk", "cp936"):
        return "gbk"
    if key in ("big5", "cp950"):
        return "big5"
    if key in ("utf-8", "utf8"):
        return "utf-8"
    return "auto"


def encoding_label(enc: str) -> str:
    key = normalize_encoding(enc)
    for value, label in TEXT_ENCODING_CHOICES:
        if value == key:
            return label
    return key


def _try_decode(raw: bytes, candidates: Sequence[str]) -> Optional[str]:
    for enc in candidates:
        try:
            return raw.decode(enc)
        except UnicodeDecodeError:
            continue
    return None


def decode_bytes(
    raw: bytes,
    encoding: str = "auto",
    *,
    auto_order: Sequence[str] = _RANGER_AUTO_ORDER,
) -> str:
    if not raw:
        return ""
    key = normalize_encoding(encoding)
    if key == "auto":
        text = _try_decode(raw, auto_order)
        return text if text is not None else raw.decode("latin1", errors="replace")
    explicit = {
        "gbk": ("gbk", "cp936"),
        "big5": ("big5", "cp950"),
        "utf-8": ("utf-8",),
    }[key]
    text = _try_decode(raw, explicit)
    return text if text is not None else raw.decode("latin1", errors="replace")


def encode_text(
    text: str,
    encoding: str = "auto",
    *,
    nbytes: Optional[int] = None,
) -> bytes:
    key = normalize_encoding(encoding)
    write_enc = "gbk" if key == "auto" else key
    candidates = {
        "gbk": ("gbk", "cp936"),
        "big5": ("big5", "cp950"),
        "utf-8": ("utf-8",),
    }.get(write_enc, (write_enc,))
    raw: Optional[bytes] = None
    for enc in candidates:
        try:
            raw = text.encode(enc)
            break
        except UnicodeEncodeError:
            continue
    if raw is None:
        raw = text.encode("latin1", errors="replace")
    if nbytes is not None:
        if len(raw) > nbytes:
            raw = raw[:nbytes]
        raw = raw.ljust(nbytes, b"\x00")
    return raw


def decode_talk_payload(raw: bytes, encoding: str = "auto") -> str:
    """XOR 0xFF payload with NUL-terminated C string semantics."""
    dec = bytes(b ^ 0xFF for b in raw)
    out = bytearray()
    for b in dec:
        if b in (0x00, 0xFF):
            break
        out.append(b)
    return decode_bytes(bytes(out), encoding, auto_order=_TALK_AUTO_ORDER)


def encode_talk_payload(text: str, encoding: str = "auto") -> bytes:
    raw = encode_text(text, encoding) + b"\x00"
    return bytes(b ^ 0xFF for b in raw)
