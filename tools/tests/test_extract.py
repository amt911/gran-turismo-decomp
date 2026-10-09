from pathlib import Path

import pytest

from tools.checksums import ChecksumError
from tools.extract import ExtractError, extract, find_iso
from tools.tests.test_psp_header import make_header

SHA1_ABC = "a9993e364706816aba3e25717850c26c9cd0d89d"    # fake ISO content b"abc"
SHA1_EMPTY = "da39a3ee5e6b4b0d3255bfef95601890afd80709"  # fake ELF content b""
SUMS = {"iso": SHA1_ABC, "eboot.elf": SHA1_EMPTY}


class FakeTools:
    """Stands in for 7z and pspdecrypt; records every command."""

    def __init__(self, header: bytes = make_header(), elf_bytes: bytes = b""):
        self.header, self.elf_bytes, self.calls = header, elf_bytes, []

    def __call__(self, cmd):
        self.calls.append(list(cmd))
        if cmd[0] == "7z":
            out = Path(next(a for a in cmd if a.startswith("-o"))[2:])
            eboot = out / "PSP_GAME/SYSDIR/EBOOT.BIN"
            eboot.parent.mkdir(parents=True, exist_ok=True)
            eboot.write_bytes(self.header)
        else:
            Path(cmd[cmd.index("-o") + 1]).write_bytes(self.elf_bytes)


@pytest.fixture
def env(tmp_path: Path):
    iso = tmp_path / "game.iso"
    iso.write_bytes(b"abc")
    tools = tmp_path / "bin"
    tools.mkdir()
    (tools / "pspdecrypt").write_text("")
    return iso, tmp_path / "extracted", tools


def test_extract_happy_path_runs_7z_without_gtvol_then_pspdecrypt(env):
    iso, out, tools = env
    fake = FakeTools()
    elf = extract(iso, out, SUMS, tools, runner=fake)
    assert elf == out / "EBOOT.elf"
    assert fake.calls[0][:2] == ["7z", "x"] and "-x!PSP_GAME/USRDIR/GT.VOL" in fake.calls[0]
    assert fake.calls[1] == [str(tools / "pspdecrypt"), "-o", str(elf),
                             str(out / "iso/PSP_GAME/SYSDIR/EBOOT.BIN")]


def test_extract_skips_everything_when_elf_already_verified(env):
    iso, out, tools = env
    out.mkdir()
    (out / "EBOOT.elf").write_bytes(b"")

    def must_not_run(cmd):
        raise AssertionError(f"ran {cmd}")

    assert extract(iso, out, SUMS, tools, runner=must_not_run) == out / "EBOOT.elf"


def test_extract_regenerates_stale_elf_with_wrong_hash(env):
    iso, out, tools = env
    out.mkdir()
    (out / "EBOOT.elf").write_bytes(b"stale")
    fake = FakeTools()
    extract(iso, out, SUMS, tools, runner=fake)
    assert len(fake.calls) == 2 and (out / "EBOOT.elf").read_bytes() == b""


def test_extract_rejects_wrong_iso_before_running_anything(env):
    iso, out, tools = env
    iso.write_bytes(b"abd")
    fake = FakeTools()
    with pytest.raises(ChecksumError, match=SHA1_ABC):
        extract(iso, out, SUMS, tools, runner=fake)
    assert fake.calls == []


def test_extract_rejects_unexpected_module_before_decrypting(env):
    iso, out, tools = env
    fake = FakeTools(header=make_header(name=b"OTHER"))
    with pytest.raises(ExtractError, match="PDIAPP"):
        extract(iso, out, SUMS, tools, runner=fake)
    assert len(fake.calls) == 1


def test_extract_rejects_already_decrypted_eboot(env):
    iso, out, tools = env
    fake = FakeTools(header=make_header(magic=b"\x7fELF"))
    with pytest.raises(ExtractError, match="magic"):
        extract(iso, out, SUMS, tools, runner=fake)


def test_extract_rejects_wrong_elf_hash(env):
    iso, out, tools = env
    with pytest.raises(ChecksumError, match="eboot.elf"):
        extract(iso, out, SUMS, tools, runner=FakeTools(elf_bytes=b"x"))


def test_extract_requires_pspdecrypt_and_points_to_setup(env):
    iso, out, tools = env
    (tools / "pspdecrypt").unlink()
    with pytest.raises(ExtractError, match="tools/setup.sh"):
        extract(iso, out, SUMS, tools, runner=FakeTools())


def test_extract_names_missing_checksum_entry(env):
    iso, out, tools = env
    with pytest.raises(ExtractError, match="'eboot.elf'"):
        extract(iso, out, {"iso": SHA1_ABC}, tools, runner=FakeTools())


def test_extract_handles_path_with_spaces_and_brackets(env, tmp_path: Path):
    _, out, tools = env
    iso = tmp_path / "Gran Turismo (v2.0) [UCES-01245].iso"
    iso.write_bytes(b"abc")
    fake = FakeTools()
    extract(iso, out, SUMS, tools, runner=fake)
    assert str(iso) in fake.calls[0]


def test_find_iso_picks_single_iso_any_case(tmp_path: Path):
    (tmp_path / "GAME.ISO").write_bytes(b"")
    (tmp_path / "notes.txt").write_text("")
    assert find_iso(tmp_path, None) == tmp_path / "GAME.ISO"


def test_find_iso_refuses_zero_or_several(tmp_path: Path):
    with pytest.raises(ExtractError, match="found 0"):
        find_iso(tmp_path, None)
    (tmp_path / "a.iso").write_bytes(b"")
    (tmp_path / "b.iso").write_bytes(b"")
    with pytest.raises(ExtractError, match="found 2"):
        find_iso(tmp_path, None)


def test_find_iso_explicit_path_must_exist(tmp_path: Path):
    with pytest.raises(ExtractError, match="not found"):
        find_iso(tmp_path, tmp_path / "missing.iso")
