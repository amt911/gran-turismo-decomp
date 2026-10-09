"""Parse the plain-text part of an encrypted PSP executable header (~PSP)."""

from __future__ import annotations

import struct
from dataclasses import dataclass

PSP_MAGIC = b"~PSP"
HEADER_SIZE = 0x150


class PspHeaderError(Exception):
    """The data is not a ~PSP header."""


@dataclass(frozen=True)
class PspHeader:
    module_name: str
    elf_size: int
    psp_size: int
    tag: int


def parse_psp_header(data: bytes) -> PspHeader:
    if len(data) < HEADER_SIZE:
        raise PspHeaderError(f"header too short: {len(data)} bytes, need {HEADER_SIZE:#x}")
    if data[:4] != PSP_MAGIC:
        raise PspHeaderError(
            f"bad magic {data[:4]!r}, expected {PSP_MAGIC!r} (already decrypted, or not an EBOOT?)"
        )
    name = data[0x0A:0x26].split(b"\0", 1)[0].decode("ascii", errors="replace")
    elf_size, psp_size = struct.unpack_from("<II", data, 0x28)
    (tag,) = struct.unpack_from("<I", data, 0xD0)
    return PspHeader(name, elf_size, psp_size, tag)
