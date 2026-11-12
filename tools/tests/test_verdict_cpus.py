"""tools/verdict.sh: the gate and synthetic CPU sets come from GT_GATE_CPUS / GT_SYNTH_CPUS (desktop halves by default).

Runs the script with `taskset` and the heavy steps replaced by stubs that record their arguments, in a scratch tree."""
import os
import subprocess
from pathlib import Path

REPO = Path(__file__).resolve().parents[2]


def run_verdict(tmp_path: Path, env_extra: dict[str, str]) -> list[str]:
    tree = tmp_path / "tree"
    bindir = tmp_path / "bin"
    bindir.mkdir()
    calls = tmp_path / "calls.txt"
    # A tree that is a git repo with an origin branch, so the script's fetch/checkout succeed.
    origin = tmp_path / "origin"
    subprocess.run(["git", "init", "-q", "-b", "main", str(origin)], check=True)
    subprocess.run(["git", "-C", str(origin), "-c", "user.email=t@t", "-c", "user.name=t", "commit", "-q", "--allow-empty", "-m", "x"], check=True)
    subprocess.run(["git", "clone", "-q", str(origin), str(tree)], check=True)
    for name in ("taskset", "systemd-run", "ctest", "cmake"):
        stub = bindir / name
        stub.write_text(f'#!/bin/sh\necho "{name} $*" >> {calls}\n')
        stub.chmod(0o755)
    (tree / "tools").mkdir()
    for name in ("setup.sh", "cpu_lock.sh", "pad_route.sh"):
        stub = tree / "tools" / name
        stub.write_text("#!/bin/sh\nexit 0\n")
        stub.chmod(0o755)
    venv = tree / ".venv" / "bin"
    venv.mkdir(parents=True)
    for name in ("python", "pytest"):
        stub = venv / name
        # tools.verdict_scope answers "synthetic" so the synthetic groups run too
        stub.write_text('#!/bin/sh\ncase "$*" in *verdict_scope*) echo synthetic;; esac\nexit 0\n')
        stub.chmod(0o755)
    (tree / "game_files" / "UCES01245-GAMEDAT").mkdir(parents=True)
    # The caller's own GT_GATE_CPUS / GT_SYNTH_CPUS must not leak in (the laptop's verdict exports them: verdict 580).
    base = {k: v for k, v in os.environ.items() if k not in ("GT_GATE_CPUS", "GT_SYNTH_CPUS")}
    env = dict(base, PATH=f"{bindir}:{os.environ['PATH']}", GT_VERIFY_TREE=str(tree), **env_extra)
    subprocess.run(["bash", str(REPO / "tools" / "verdict.sh"), "main", "t"], env=env, cwd=tree, capture_output=True, timeout=60)
    return [line for line in calls.read_text().splitlines() if line.startswith("taskset")] if calls.exists() else []


def test_defaults_split_the_desktop(tmp_path):
    lines = run_verdict(tmp_path, {})
    assert lines[0].startswith("taskset -c 6-11,18-23 ")
    assert all(line.startswith("taskset -c 0-5,12-17 ") for line in lines[1:]) and len(lines) == 3


def test_environment_overrides_both_sets(tmp_path):
    lines = run_verdict(tmp_path, {"GT_GATE_CPUS": "0-11", "GT_SYNTH_CPUS": "0-11"})
    assert len(lines) == 3 and all(line.startswith("taskset -c 0-11 ") for line in lines)
