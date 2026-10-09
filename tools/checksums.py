"""Read and verify the project's expected hashes (config/checksums.sha1, sha1sum format)."""

from __future__ import annotations

import hashlib
from pathlib import Path

_HEX = set("0123456789abcdef")


class ChecksumError(Exception):
    """A hash file is malformed or a file does not have the expected hash."""


def load_checksums(path: Path) -> dict[str, str]:
    result: dict[str, str] = {}
    for lineno, raw in enumerate(path.read_text().splitlines(), 1):
        line = raw.strip()
        if not line or line.startswith("#"):
            continue
        parts = line.split(None, 1)
        digest = parts[0].lower()
        if len(parts) != 2 or len(digest) != 40 or not set(digest) <= _HEX:
            raise ChecksumError(f"{path}:{lineno}: expected '<sha1>  <name>', got {raw!r}")
        result[parts[1].strip()] = digest
    return result


def sha1_of(path: Path, chunk: int = 1 << 20) -> str:
    h = hashlib.sha1()
    with path.open("rb") as f:
        while block := f.read(chunk):
            h.update(block)
    return h.hexdigest()


def verify(path: Path, expected: str, label: str) -> None:
    actual = sha1_of(path)
    if actual != expected:
        raise ChecksumError(f"{label}: SHA1 mismatch for {path}: expected {expected}, got {actual}")
