"""PSP savedata: PARAM.SFO, the KIRK slot keys and the savedata cipher and hash (mode 5), as the firmware does them.

Spec: docs/superpowers/specs/2026-10-23-savedata-routes-design.md. This is the reference the C++ HLE
(host/psp_savedata_crypto.cpp) is tested against, and the CLI that writes the KIRK slot keys into a generated header.
Written from the algorithm (PPSSPP's sceChnnlsv.cpp and libkirk, both GPL, were read as references; nothing copied).

Mode 5 (GT: SDK 5.x, a game key, secureVersion 0):
- cipher: seed = AES-dec(slot 18, (file[0:16] ^ game key) ^ K19DC) ^ K19CC; block i of the keystream is
  AES-dec(slot 100, seed[0:12] || le32(i + 1)) ^ the previous counter block (an all-zero block before the first),
  i.e. AES-CBC-decrypt of the counter blocks; the data after the first 16 bytes is XORed with it.
- hash: a CMAC-style CBC-MAC (slot 16) over the whole file padded with zeroes to 16, then ^ H19BC, ^ game key,
  AES-enc(slot 16) once more. It equals the 16 bytes after the file's name in PARAM.SFO's SAVEDATA_FILE_LIST.

Usage: .venv/bin/python -m tools.savedata keys OUT.hpp   (slot keys from the pinned pspdecrypt checkout)
"""

from __future__ import annotations

import hashlib
import re
import struct
import subprocess
import sys
from pathlib import Path

KIRK_SOURCE = "tools/src/pspdecrypt/libkirk/kirk_engine.c"
KEYS_DIGEST_FILE = Path("config/kirk_slot_keys.sha256")
SLOTS = (16, 18, 100)
_K19CC = bytes.fromhex("7044A3AEEF5DA5F2857FF2D694F5363B")
_K19DC = bytes.fromhex("EC6D29592635A57F972A0DBCA3263300")
_H19BC = bytes.fromhex("CB15F407F96A523C04B9B2EE5C53FA86")
_FILE_ENTRY = 0x20  # SAVEDATA_FILE_LIST: name (13 bytes), hash (16), padding (3)


class SavedataError(Exception):
    """Malformed PARAM.SFO or key table."""


def read_sfo(sfo: bytes) -> dict[str, bytes]:
    if sfo[:4] != b"\0PSF":
        raise SavedataError("not a PARAM.SFO")
    _magic, _version, key_table, data_table, count = struct.unpack_from("<4sIIII", sfo)
    result = {}
    for i in range(count):
        key_offset, _fmt, length, _max, data_offset = struct.unpack_from("<HHIII", sfo, 20 + 16 * i)
        start = key_table + key_offset
        name = sfo[start:sfo.index(b"\0", start)].decode()
        result[name] = sfo[data_table + data_offset:data_table + data_offset + length]
    return result


def encrypted(sfo: bytes) -> bool:
    """SAVEDATA_PARAMS' first byte: 0 for a plain save, the crypt mode flags (0x41 for GT) otherwise."""
    params = read_sfo(sfo).get("SAVEDATA_PARAMS", b"")
    return bool(params) and params[0] != 0


def file_hash(sfo: bytes, name: str) -> bytes:
    files = read_sfo(sfo).get("SAVEDATA_FILE_LIST", b"")
    for offset in range(0, len(files) - _FILE_ENTRY + 1, _FILE_ENTRY):
        entry = files[offset:offset + _FILE_ENTRY]
        if entry[:13].rstrip(b"\0").decode(errors="replace") == name:
            return entry[13:29]
    raise SavedataError(f"{name} is not in SAVEDATA_FILE_LIST")


def kirk_slot_keys(source: str) -> dict[int, bytes]:
    """The KIRK commands 4/7 key table (`keyvault[0x80][0x10]`) of a libkirk source, as data: the slots GT's saves use."""
    start = source.find("keyvault")
    if start < 0:
        raise SavedataError("no keyvault table")
    body = source[source.index("{", source.index("=", start)) + 1:]
    rows = re.findall(r"\{([^{}]*)\}", body[:body.index("};")])
    table = [bytes(int(x, 16) for x in re.findall(r"0x([0-9A-Fa-f]{2})", row)) for row in rows]
    if len(table) < 0x80 or any(len(table[slot]) != 16 for slot in SLOTS):
        raise SavedataError(f"keyvault has {len(table)} rows, expected 0x80 of 16 bytes")
    return {slot: table[slot] for slot in SLOTS}


def keys_digest(keys: dict[int, bytes]) -> str:
    return hashlib.sha256(b"".join(keys[slot] for slot in sorted(keys))).hexdigest()


def _aes(mode: str, key: bytes, data: bytes, decrypt: bool, iv: bytes = bytes(16)) -> bytes:
    args = ["openssl", "enc", f"-aes-128-{mode}", "-K", key.hex(), "-nopad"]
    if mode == "cbc":
        args += ["-iv", iv.hex()]
    if decrypt:
        args.append("-d")
    return subprocess.run(args, input=data, capture_output=True, check=True).stdout


def _xor(a: bytes, b: bytes) -> bytes:
    return bytes(x ^ y for x, y in zip(a, b))


def _padded(data: bytes) -> bytes:
    return data + bytes(-len(data) % 16)


def decrypt(data: bytes, game_key: bytes, keys: dict[int, bytes]) -> bytes:
    """The plaintext of a mode-5 savedata file (its first 16 bytes are the cipher's seed, not data)."""
    if len(data) <= 16:
        raise SavedataError("a savedata file has 16 bytes of seed and some data")
    padded = _padded(data)
    seed = _xor(_aes("ecb", keys[18], _xor(_xor(padded[:16], game_key), _K19DC), True), _K19CC)
    body = padded[16:]
    counters = b"".join(seed[:12] + struct.pack("<I", i + 1) for i in range(len(body) // 16))
    stream = _aes("cbc", keys[100], counters, True)
    return _xor(body, stream)[:len(data) - 16]


def _double(block: bytes) -> bytes:
    value = int.from_bytes(block, "big") << 1
    if block[0] & 0x80:
        value ^= 0x87
    return (value & ((1 << 128) - 1)).to_bytes(16, "big")


def mac(data: bytes, game_key: bytes, keys: dict[int, bytes]) -> bytes:
    """The hash PARAM.SFO keeps for a mode-5 savedata file (of its encrypted bytes)."""
    message = _padded(data)
    subkey = _double(_aes("ecb", keys[16], bytes(16), False))
    state = _aes("cbc", keys[16], message[:-16], False)[-16:] if len(message) > 16 else bytes(16)
    tag = _aes("ecb", keys[16], _xor(_xor(message[-16:], subkey), state), False)
    return _aes("ecb", keys[16], _xor(_xor(tag, _H19BC), game_key), False)


def write_header(keys: dict[int, bytes], out: Path) -> None:
    lines = ["// Generated by tools/savedata.py from the pinned pspdecrypt checkout: never commit.", "#pragma once",
             "#include <array>", "#include <cstdint>", "namespace gt {"]
    for slot in SLOTS:
        values = ", ".join(f"0x{b:02x}" for b in keys[slot])
        lines.append(f"inline constexpr std::array<std::uint8_t, 16> kKirkSlot{slot}{{{values}}};")
    lines.append("} // namespace gt")
    out.write_text("\n".join(lines) + "\n")


def main(argv: list[str]) -> int:
    if len(argv) != 2 or argv[0] != "keys":
        print(__doc__.strip().splitlines()[-1], file=sys.stderr)
        return 2
    keys = kirk_slot_keys(Path(KIRK_SOURCE).read_text())
    expected = next(line.split()[0] for line in KEYS_DIGEST_FILE.read_text().splitlines() if line.strip() and not line.startswith("#"))
    if keys_digest(keys) != expected:
        print(f"savedata: KIRK slot keys do not match {KEYS_DIGEST_FILE}", file=sys.stderr)
        return 1
    write_header(keys, Path(argv[1]))
    return 0


if __name__ == "__main__":
    raise SystemExit(main(sys.argv[1:]))
