from pathlib import Path

import pytest

from tools.checksums import sha1_of
from tools.extract import REPO, main

ELF_SHA1 = "8746583fa8298f6447132988a01b236a8c5994b0"
HAVE_DUMP = any(p.suffix.lower() == ".iso" for p in (REPO / "game_files").glob("*")) and \
    (REPO / "tools/bin/pspdecrypt").is_file()


@pytest.mark.needs_dump
@pytest.mark.skipif(not HAVE_DUMP, reason="needs the user's ISO in game_files/ and tools/bin/pspdecrypt")
def test_real_pipeline_produces_verified_elf(tmp_path: Path):
    assert main(["--out", str(tmp_path)]) == 0
    assert sha1_of(tmp_path / "EBOOT.elf") == ELF_SHA1
    assert not (tmp_path / "iso/PSP_GAME/USRDIR/GT.VOL").exists()
