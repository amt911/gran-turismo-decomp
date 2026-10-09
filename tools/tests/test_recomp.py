from pathlib import Path

import pytest

from tools.recomp import RecompError, regenerate

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
    assert fake.calls == [[str(tool), str(elf), "--auto", str(out), "0x08804000"]]


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
