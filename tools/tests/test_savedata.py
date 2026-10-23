"""Tests for tools.savedata: PARAM.SFO, KIRK slot keys, the PSP savedata cipher and hash (mode 5)."""

from __future__ import annotations

import os
import struct
from pathlib import Path

import pytest

from tools import savedata

# Synthetic slot keys: the real ones are never in the repo (the CLI reads them from the fetched pspdecrypt checkout).
KEYS = {16: bytes(range(16)), 18: bytes(range(16, 32)), 100: bytes(range(100, 116))}
GAME_KEY = bytes.fromhex("00112233445566778899aabbccddeeff")


def make_sfo(entries: list[tuple[str, int, bytes]]) -> bytes:
    """A PARAM.SFO: header, index table, key table, data table (fmt 0x0204 utf8, 0x0404 int32, 0x0004 raw)."""
    keys = b""
    data = b""
    index = b""
    for name, fmt, value in entries:
        index += struct.pack("<HHIII", len(keys), fmt, len(value), len(value), len(data))
        keys += name.encode() + b"\0"
        data += value
    keys += bytes(-len(keys) % 4)
    key_table = 20 + len(index)
    data_table = key_table + len(keys)
    return struct.pack("<4sIIII", b"\0PSF", 0x101, key_table, data_table, len(entries)) + index + keys + data


def file_list(name: str, digest: bytes) -> bytes:
    entry = name.encode().ljust(13, b"\0") + digest + bytes(3)
    return entry + bytes(3168 - len(entry))


def test_read_sfo_gives_each_value():
    sfo = make_sfo([("TITLE", 0x0204, b"GT\0"), ("PARENTAL_LEVEL", 0x0404, struct.pack("<I", 3))])
    assert savedata.read_sfo(sfo) == {"TITLE": b"GT\0", "PARENTAL_LEVEL": struct.pack("<I", 3)}


def test_read_sfo_refuses_other_magic():
    with pytest.raises(savedata.SavedataError):
        savedata.read_sfo(b"XPSF" + bytes(16))


def test_file_hash_and_crypt_mode():
    digest = bytes(range(0xA0, 0xB0))
    sfo = make_sfo([("SAVEDATA_FILE_LIST", 0x0004, file_list("GAME.DAT", digest)),
                    ("SAVEDATA_PARAMS", 0x0004, bytes([0x41]) + bytes(127))])
    assert savedata.file_hash(sfo, "GAME.DAT") == digest
    assert savedata.encrypted(sfo)
    with pytest.raises(savedata.SavedataError):
        savedata.file_hash(sfo, "OTHER.DAT")


def test_plain_params_are_not_encrypted():
    assert not savedata.encrypted(make_sfo([("SAVEDATA_PARAMS", 0x0004, bytes(128))]))


def test_kirk_slot_keys_reads_the_table_by_index():
    rows = ",\n".join("  {" + ", ".join(f"0x{(i * 16 + j) & 0xFF:02X}" for j in range(16)) + "}" for i in range(0x80))
    text = f"/* x */\nstatic const unsigned char keyvault[0x80][0x10] = {{\n{rows}\n}};\nint other[2] = {{1, 2}};\n"
    keys = savedata.kirk_slot_keys(text)
    assert sorted(keys) == [16, 18, 100]
    assert keys[18] == bytes((18 * 16 + j) & 0xFF for j in range(16))


def test_kirk_slot_keys_refuses_a_short_table():
    with pytest.raises(savedata.SavedataError):
        savedata.kirk_slot_keys("keyvault[0x80][0x10] = { {0x00} };")


def test_decrypt_undoes_itself_and_keeps_the_length():
    # The cipher is a keystream XOR (the stream depends on the first 16 bytes and the key only): applying it to the
    # header + plaintext gives the ciphertext body, and decrypting that gives the plaintext back.
    header = bytes(range(0x30, 0x40))
    plain = bytes((i * 7) & 0xFF for i in range(5000))
    body = savedata.decrypt(header + plain, GAME_KEY, KEYS)
    assert len(body) == len(plain) and body != plain
    assert savedata.decrypt(header + body, GAME_KEY, KEYS) == plain


def test_decrypt_depends_on_the_key_and_the_header():
    data = bytes(16) + bytes(64)
    base = savedata.decrypt(data, GAME_KEY, KEYS)
    assert savedata.decrypt(data, bytes(16), KEYS) != base
    assert savedata.decrypt(b"\1" + data[1:], GAME_KEY, KEYS) != base


def test_mac_changes_with_any_byte():
    data = bytes(range(256)) * 3
    digest = savedata.mac(data, GAME_KEY, KEYS)
    assert len(digest) == 16
    assert savedata.mac(data[:-1] + b"\0", GAME_KEY, KEYS) != digest
    assert savedata.mac(data, bytes(16), KEYS) != digest


def test_mac_of_an_unaligned_file_pads_with_zeroes():
    data = bytes(range(40))
    assert savedata.mac(data, GAME_KEY, KEYS) == savedata.mac(data + bytes(8), GAME_KEY, KEYS)


def test_header_cli_writes_the_three_keys(tmp_path):
    out = tmp_path / "kirk_slot_keys.hpp"
    savedata.write_header(KEYS, out)
    text = out.read_text()
    assert "kKirkSlot16" in text and "kKirkSlot18" in text and "kKirkSlot100" in text
    assert "0x64, 0x65" in text  # slot 100's first bytes


def test_digest_of_keys_is_stable():
    assert savedata.keys_digest(KEYS) == savedata.keys_digest(dict(reversed(list(KEYS.items()))))


SAVE = Path("game_files/UCES01245-GAMEDAT")


@pytest.mark.skipif(not (SAVE / "GAME.DAT").exists() or "GT_SAVEDATA_TEST_KEY" not in os.environ
                    or not Path(savedata.KIRK_SOURCE).exists(),
                    reason="the user's save, the game key (GT_SAVEDATA_TEST_KEY) and the pspdecrypt checkout")
def test_real_save_hash_matches_param_sfo():
    keys = savedata.kirk_slot_keys(Path(savedata.KIRK_SOURCE).read_text())
    game_key = bytes.fromhex(os.environ["GT_SAVEDATA_TEST_KEY"])
    sfo = (SAVE / "PARAM.SFO").read_bytes()
    data = (SAVE / "GAME.DAT").read_bytes()
    assert savedata.mac(data, game_key, keys) == savedata.file_hash(sfo, "GAME.DAT")


def _vault_source(tmp_path: Path) -> Path:
    rows = ",\n".join("  {" + ", ".join(f"0x{(i + j) & 0xFF:02X}" for j in range(16)) + "}" for i in range(0x80))
    path = tmp_path / "kirk_engine.c"
    path.write_text(f"static const unsigned char keyvault[0x80][0x10] = {{\n{rows}\n}};\n")
    return path


def test_cli_writes_the_header_when_the_keys_match(tmp_path, monkeypatch):
    source = _vault_source(tmp_path)
    digest = tmp_path / "keys.sha256"
    digest.write_text("# a comment line, as config/kirk_slot_keys.sha256 has\n"
                      + savedata.keys_digest(savedata.kirk_slot_keys(source.read_text())) + "\n")
    monkeypatch.setattr(savedata, "KIRK_SOURCE", str(source))
    monkeypatch.setattr(savedata, "KEYS_DIGEST_FILE", digest)
    out = tmp_path / "keys.hpp"
    assert savedata.main(["keys", str(out)]) == 0 and "kKirkSlot18" in out.read_text()


def test_cli_refuses_other_keys(tmp_path, monkeypatch):
    digest = tmp_path / "keys.sha256"
    digest.write_text("0" * 64 + "\n")
    monkeypatch.setattr(savedata, "KIRK_SOURCE", str(_vault_source(tmp_path)))
    monkeypatch.setattr(savedata, "KEYS_DIGEST_FILE", digest)
    assert savedata.main(["keys", str(tmp_path / "keys.hpp")]) == 1
    assert not (tmp_path / "keys.hpp").exists()


def test_cli_usage():
    assert savedata.main([]) == 2


def test_decrypt_refuses_a_file_without_data():
    with pytest.raises(savedata.SavedataError):
        savedata.decrypt(bytes(16), GAME_KEY, KEYS)


def test_kirk_slot_keys_without_a_table():
    with pytest.raises(savedata.SavedataError):
        savedata.kirk_slot_keys("int x;")
