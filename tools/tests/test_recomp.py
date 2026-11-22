from pathlib import Path

import pytest

from tools.recomp import RecompError, decomp_boundaries, regenerate

BASE = 0x08804000


class FakeRecomp:
    """Stands in for psp_recomp: writes the registry file the real tool writes last."""

    def __init__(self):
        self.calls = []

    def __call__(self, cmd):
        self.calls.append(list(cmd))
        out = Path(cmd[3])
        out.mkdir(parents=True, exist_ok=True)
        (out / "generated_registry.cpp").write_text("// fake\n")


@pytest.fixture
def env(tmp_path: Path):
    elf = tmp_path / "EBOOT.elf"
    elf.write_bytes(b"elf-v1")
    tool = tmp_path / "psp_recomp"
    tool.write_bytes(b"tool-v1")
    return elf, tmp_path / "generated", tool


def test_runs_when_nothing_generated_yet(env):
    elf, out, tool = env
    fake = FakeRecomp()
    assert regenerate(elf, out, tool, BASE, runner=fake) is True
    assert fake.calls == [[str(tool), str(elf), "--auto", str(out), "0x08804000", "0x8000"]]


def test_reruns_when_unit_span_changes(env):
    elf, out, tool = env
    regenerate(elf, out, tool, BASE, runner=FakeRecomp())
    fake = FakeRecomp()
    assert regenerate(elf, out, tool, BASE, unit_span=0x4000, runner=fake) is True
    assert fake.calls[0][-1] == "0x4000"


def test_skips_when_inputs_unchanged(env):
    elf, out, tool = env
    regenerate(elf, out, tool, BASE, runner=FakeRecomp())

    def must_not_run(cmd):
        raise AssertionError(f"ran {cmd}")

    assert regenerate(elf, out, tool, BASE, runner=must_not_run) is False


def test_reruns_when_elf_changes(env):
    elf, out, tool = env
    regenerate(elf, out, tool, BASE, runner=FakeRecomp())
    elf.write_bytes(b"elf-v2")
    fake = FakeRecomp()
    assert regenerate(elf, out, tool, BASE, runner=fake) is True and len(fake.calls) == 1


def test_reruns_when_recompiler_binary_changes(env):
    elf, out, tool = env
    regenerate(elf, out, tool, BASE, runner=FakeRecomp())
    tool.write_bytes(b"tool-v2")
    assert regenerate(elf, out, tool, BASE, runner=FakeRecomp()) is True


def test_reruns_when_registry_missing_despite_stamp(env):
    elf, out, tool = env
    regenerate(elf, out, tool, BASE, runner=FakeRecomp())
    (out / "generated_registry.cpp").unlink()
    assert regenerate(elf, out, tool, BASE, runner=FakeRecomp()) is True


def test_missing_elf_points_to_extract(env):
    elf, out, tool = env
    elf.unlink()
    with pytest.raises(RecompError, match="tools.extract"):
        regenerate(elf, out, tool, BASE, runner=FakeRecomp())


def test_missing_recompiler_points_to_setup(env):
    elf, out, tool = env
    tool.unlink()
    with pytest.raises(RecompError, match="tools/setup.sh"):
        regenerate(elf, out, tool, BASE, runner=FakeRecomp())


def test_failed_run_leaves_no_stamp(env):
    elf, out, tool = env

    def boom(cmd):
        Path(cmd[3]).mkdir(parents=True, exist_ok=True)
        raise RuntimeError("psp_recomp crashed")

    with pytest.raises(RuntimeError):
        regenerate(elf, out, tool, BASE, runner=boom)
    assert not (out / ".inputs.sha1").exists()


def test_passes_the_decompiled_functions_as_boundaries(env):
    elf, out, tool = env
    fake = FakeRecomp()
    assert regenerate(elf, out, tool, BASE, boundaries=[0x08B35840, 0x08A5BAD8], runner=fake) is True
    listing = out / "decomp_boundaries.txt"
    assert fake.calls[0][-1] == str(listing)
    assert listing.read_text() == "0x08a5bad8\n0x08b35840\n"


def test_reruns_when_boundaries_change(env):
    elf, out, tool = env
    regenerate(elf, out, tool, BASE, boundaries=[0x08B35840], runner=FakeRecomp())
    fake = FakeRecomp()
    assert regenerate(elf, out, tool, BASE, boundaries=[0x08B35840, 0x08B35254], runner=fake) is True
    assert len(fake.calls) == 1


def test_skips_when_boundaries_unchanged(env):
    elf, out, tool = env
    regenerate(elf, out, tool, BASE, boundaries=[0x08B35840], runner=FakeRecomp())

    def must_not_run(cmd):
        raise AssertionError(f"ran {cmd}")

    assert regenerate(elf, out, tool, BASE, boundaries=[0x08B35840], runner=must_not_run) is False


def test_decomp_boundaries_are_the_active_registrations(tmp_path: Path):
    unit = tmp_path / "libc" / "heap.cpp"
    unit.parent.mkdir(parents=True)
    unit.write_text(
        "GT_DECOMP(0x08B35840u, gt::libc::memalign, gt::ReturnKind::Word);\n"
        "// Not comparable yet: GT_DECOMP(0x08B35254u, gt::libc::free, gt::ReturnKind::Void);\n"
    )
    assert decomp_boundaries(tmp_path) == [0x08B35840]


def test_seeds_are_passed_as_seventh_argument(env):
    elf, out, tool = env
    fake = FakeRecomp()
    regenerate(elf, out, tool, BASE, boundaries=[0x08810000], seeds=[0x08820000, 0x08820010], runner=fake)
    cmd = fake.calls[0]
    assert cmd[-2] == str(out / "decomp_boundaries.txt")
    assert cmd[-1] == str(out / "extra_seeds.txt")
    assert (out / "extra_seeds.txt").read_text() == "0x08820000\n0x08820010\n"


def test_seeds_without_boundaries_leave_the_boundaries_slot_empty(env):
    elf, out, tool = env
    fake = FakeRecomp()
    regenerate(elf, out, tool, BASE, seeds=[0x08820000], runner=fake)
    assert fake.calls[0][-2:] == ["", str(out / "extra_seeds.txt")]


def test_changing_seeds_reruns(env):
    elf, out, tool = env
    regenerate(elf, out, tool, BASE, seeds=[0x08820000], runner=FakeRecomp())
    fake = FakeRecomp()
    assert regenerate(elf, out, tool, BASE, seeds=[0x08820000, 0x08820010], runner=fake) is True
    assert regenerate(elf, out, tool, BASE, seeds=[0x08820010, 0x08820000], runner=FakeRecomp()) is False


def test_no_seeds_leaves_the_digest_and_command_unchanged(env):
    elf, out, tool = env
    fake = FakeRecomp()
    regenerate(elf, out, tool, BASE, runner=fake)
    stamp = (out / ".inputs.sha1").read_text()
    # pinned: sha1(elf + tool + "0x08804000 0x8000" + boundaries listing); a change here changes every build's stamp
    assert stamp.strip() == "4c2d1f29bc3caaab3838a7ce0e79154cef564d86"
    assert regenerate(elf, out, tool, BASE, seeds=[], runner=FakeRecomp()) is False
    assert (out / ".inputs.sha1").read_text() == stamp
    assert len(fake.calls[0]) == 6


def test_read_seeds_converts_elf_vaddrs_and_skips_comments_and_blank_lines(tmp_path: Path):
    from tools.recomp import read_seeds

    f = tmp_path / "s.txt"
    f.write_text("# header\n0x00010000  # first\n\n00010010\n")
    assert read_seeds(f) == [0x08814000, 0x08814010]


def test_read_seeds_missing_file_is_an_error_not_an_empty_list(tmp_path: Path):
    from tools.recomp import read_seeds

    with pytest.raises(RecompError, match="not found"):
        read_seeds(tmp_path / "none.txt")


def test_read_seeds_names_the_file_and_line_of_a_bad_address(tmp_path: Path):
    from tools.recomp import read_seeds

    f = tmp_path / "s.txt"
    f.write_text("0x10\nzzz\n")
    with pytest.raises(RecompError, match=r"s.txt:2"):
        read_seeds(f)


def test_main_fails_with_a_missing_explicit_seeds_path(tmp_path: Path, capsys):
    from tools.recomp import main

    assert main(["--extra-seeds", str(tmp_path / "none.txt")]) == 1
    assert "not found" in capsys.readouterr().err


def test_the_committed_seed_list_is_the_default_and_exists():
    from tools.recomp import REPO, SEEDS_SOURCE

    assert SEEDS_SOURCE == REPO / "config" / "extra_seeds.txt"
    assert SEEDS_SOURCE.is_file()


def test_main_regenerates_with_the_seeds_and_no_extra_seeds_builds_the_baseline(tmp_path: Path, monkeypatch, capsys):
    import tools.recomp as r

    seen = []
    monkeypatch.setattr(r, "regenerate", lambda *a, **k: seen.append(k["seeds"]) or True)
    seeds = tmp_path / "s.txt"
    seeds.write_text("0x10\n")
    assert r.main(["--extra-seeds", str(seeds)]) == 0
    assert r.main(["--no-extra-seeds"]) == 0
    assert seen == [[0x08804010], []]
    monkeypatch.setattr(r, "regenerate", lambda *a, **k: (_ for _ in ()).throw(r.RecompError("boom")))
    assert r.main(["--no-extra-seeds"]) == 1


def test_check_only_reports_without_running(env):
    # A gate asks first whether the generated code is current: only a real regeneration needs every heavy slot
    # (2026-11-20: the slot taken for a no-op check made every gate wait behind every other).
    elf, out, tool = env

    def must_not_run(cmd):
        raise AssertionError(f"ran {cmd}")

    assert regenerate(elf, out, tool, BASE, runner=must_not_run, check_only=True) is True   # nothing generated yet
    assert not (out / ".inputs.sha1").exists()
    regenerate(elf, out, tool, BASE, runner=FakeRecomp())
    assert regenerate(elf, out, tool, BASE, runner=must_not_run, check_only=True) is False  # up to date
    assert regenerate(elf, out, tool, BASE, unit_span=0x4000, runner=must_not_run, check_only=True) is True
    assert (out / ".inputs.sha1").exists()  # a check never removes the stamp
