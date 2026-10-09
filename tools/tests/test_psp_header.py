import struct

import pytest

from tools.psp_header import HEADER_SIZE, PspHeader, PspHeaderError, parse_psp_header


def make_header(name: bytes = b"PDIAPP", elf_size: int = 7057972, psp_size: int = 7058320,
                tag: int = 0xD9160BF0, magic: bytes = b"~PSP") -> bytes:
    h = bytearray(HEADER_SIZE)
    h[0:4] = magic
    h[0x0A:0x0A + len(name)] = name
    struct.pack_into("<II", h, 0x28, elf_size, psp_size)
    struct.pack_into("<I", h, 0xD0, tag)
    return bytes(h)


def test_parses_fields_of_gt_like_header():
    assert parse_psp_header(make_header()) == PspHeader("PDIAPP", 7057972, 7058320, 0xD9160BF0)


def test_rejects_wrong_magic_and_hints_at_decrypted_file():
    with pytest.raises(PspHeaderError, match="magic"):
        parse_psp_header(make_header(magic=b"\x7fELF"))


def test_rejects_short_input():
    with pytest.raises(PspHeaderError, match="too short"):
        parse_psp_header(b"~PSP" + bytes(16))


def test_module_name_stops_at_nul():
    assert parse_psp_header(make_header(name=b"AB\0CD")).module_name == "AB"
