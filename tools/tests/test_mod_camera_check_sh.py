"""The call sequence of tools/mod_camera_check.sh: every run in its own cpu_lock hold, one verdict call over all of them. Stubs for gt_play, cpu_lock, python."""
import os
import shutil
import stat
import subprocess
from pathlib import Path

SCRIPT = Path(__file__).resolve().parents[1] / "mod_camera_check.sh"


def _exe(path, text):
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_text(text)
    path.chmod(path.stat().st_mode | stat.S_IEXEC)


def make_repo(tmp_path):
    repo = tmp_path / "repo"
    (repo / "tools").mkdir(parents=True)
    shutil.copy(SCRIPT, repo / "tools" / "mod_camera_check.sh")
    (repo / "config" / "pad_routes").mkdir(parents=True)
    (repo / "config" / "pad_routes" / "chase.pad").write_text("# route\n100:START\n")
    log = tmp_path / "calls.log"
    _exe(repo / "build" / "gt_play", '#!/bin/sh\necho "gt_play $*" >> "%s"\necho "[pad] script done at vblank 100"\n' % log)
    _exe(repo / "tools" / "cpu_lock.sh", '#!/bin/sh\nwhat=$1; shift\necho "lock $what" >> "%s"\nexec "$@"\n' % log)
    _exe(repo / ".venv" / "bin" / "python", '#!/bin/sh\necho "python $*" >> "%s"\nexit 0\n' % log)
    _exe(tmp_path / "bin" / "systemd-run", '#!/bin/sh\nwhile [ "$1" != "--" ]; do shift; done; shift; exec "$@"\n')
    return repo, log


def run(tmp_path, *args):
    repo, log = make_repo(tmp_path)
    env = dict(os.environ, PATH=f"{tmp_path / 'bin'}:{os.environ['PATH']}")
    env.pop("MOD_CHECK_NO_LOCK", None)
    result = subprocess.run(["bash", str(repo / "tools" / "mod_camera_check.sh"), "--without-mod", *args], cwd=repo, env=env, capture_output=True, text=True, timeout=60)
    return result, log.read_text().splitlines() if log.exists() else []


def test_every_run_has_its_own_lock_hold_and_one_verdict_call(tmp_path):
    result, calls = run(tmp_path)
    assert result.returncode == 0, result.stderr
    locks = [c for c in calls if c.startswith("lock ")]
    assert locks == ["lock mod_camera_check:baseline", "lock mod_camera_check:yaw_0", "lock mod_camera_check:yaw_90", "lock mod_camera_check:yaw_180"]
    plays = [c for c in calls if c.startswith("gt_play")]
    assert len(plays) == 4
    verdicts = [c for c in calls if c.startswith("python")]
    assert len(verdicts) == 1 and verdicts[0].endswith("yaw_0 yaw_90 yaw_180")
    # the order: each lock is followed by its gt_play, the verdict comes last
    assert calls[-1] == verdicts[0]
    assert [calls[i].split()[0] for i in range(0, 8)] == ["lock", "gt_play"] * 4


def test_toggle_is_one_hold_and_a_caller_that_holds_the_lock_gets_none(tmp_path):
    result, calls = run(tmp_path, "--toggle-off")
    assert [c for c in calls if c.startswith("lock ")] == ["lock mod_camera_check:baseline", "lock mod_camera_check:toggle_90"]
    repo, log = make_repo(tmp_path / "held")
    env = dict(os.environ, PATH=f"{tmp_path / 'held' / 'bin'}:{os.environ['PATH']}", MOD_CHECK_NO_LOCK="1")
    subprocess.run(["bash", str(repo / "tools" / "mod_camera_check.sh"), "--without-mod"], cwd=repo, env=env, capture_output=True, text=True, timeout=60)
    assert not any(c.startswith("lock ") for c in log.read_text().splitlines())
