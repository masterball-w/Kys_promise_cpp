"""File backup and atomic write helpers."""

from __future__ import annotations

import os
import shutil
import tempfile
from pathlib import Path


def backup_file(path: str | Path, suffix: str = ".bak") -> Path | None:
    """Copy *path* to *path*+suffix. Returns backup path, or None if source missing."""
    src = Path(path)
    if not src.is_file():
        return None
    dst = Path(str(src) + suffix)
    shutil.copy2(src, dst)
    return dst


def atomic_write(path: str | Path, data: bytes) -> None:
    """Write bytes atomically (temp file + replace)."""
    path = Path(path)
    path.parent.mkdir(parents=True, exist_ok=True)
    fd, tmp_name = tempfile.mkstemp(dir=str(path.parent), prefix=".tmp_", suffix=".part")
    try:
        with os.fdopen(fd, "wb") as f:
            f.write(data)
            f.flush()
            os.fsync(f.fileno())
        os.replace(tmp_name, path)
    except Exception:
        try:
            os.unlink(tmp_name)
        except OSError:
            pass
        raise
