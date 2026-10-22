"""tools/cpu_lock.sh: the lock shared with other agent sessions for timing-dependent jobs (real processes, temp dir)."""

from __future__ import annotations

import os
import subprocess
import time
from pathlib import Path

SCRIPT = Path(__file__).resolve().parents[1] / "cpu_lock.sh"


def env(tmp_path: Path, starve_s: int = 2, poll_s: int = 1) -> dict:
    return {**os.environ, "CPU_LOCK_DIR": str(tmp_path), "CPU_LOCK_STARVE_S": str(starve_s), "CPU_LOCK_POLL_S": str(poll_s), "CPU_LOCK_GRACE_S": "1"}


def hold(tmp_path: Path, what: str, seconds: float, marker: Path) -> subprocess.Popen:
    """A holder that writes `marker` once it runs and then sleeps."""
    return subprocess.Popen(["sh", str(SCRIPT), what, "sh", "-c", f"touch {marker}; sleep {seconds}"],
                            env=env(tmp_path), stderr=subprocess.DEVNULL)


def wait_for(path: Path, timeout: float = 10.0) -> None:
    end = time.monotonic() + timeout
    while not path.exists():
        assert time.monotonic() < end, f"{path} never appeared"
        time.sleep(0.05)


def test_runs_the_command_and_clears_the_owner(tmp_path):
    done = tmp_path / "done"
    rc = subprocess.run(["sh", str(SCRIPT), "job", "touch", str(done)], env=env(tmp_path)).returncode
    assert rc == 0 and done.exists()
    assert (tmp_path / "cpu-heavy.owner").read_text() == ""


def test_a_starved_waiter_stops_the_holder_once_and_then_runs(tmp_path):
    first = hold(tmp_path, "long", 60, tmp_path / "first")
    wait_for(tmp_path / "first")
    waiter = subprocess.Popen(["sh", str(SCRIPT), "waiter", "touch", str(tmp_path / "waited")], env=env(tmp_path),
                              stderr=subprocess.DEVNULL)
    assert waiter.wait(timeout=40) == 0
    assert (tmp_path / "waited").exists()
    assert first.wait(timeout=20) != 0  # stopped


def test_a_starved_waiter_does_not_stop_a_holder_that_took_the_lock_after_its_stop(tmp_path):
    """The bug: past 30 min the waiter stopped every new holder (the owner line changed each time) and never got the lock.
    Here a second job queues behind the stale holder, polling every second; it takes the lock while the starved waiter
    (polling every 5 s) is between polls, and must then run to its end."""
    first = hold(tmp_path, "long", 60, tmp_path / "first")
    wait_for(tmp_path / "first")
    second = subprocess.Popen(["sh", str(SCRIPT), "second", "sh", "-c", f"touch {tmp_path / 'second'}; sleep 8"],
                              env=env(tmp_path, starve_s=1000, poll_s=1), stderr=subprocess.DEVNULL)
    waiter = subprocess.Popen(["sh", str(SCRIPT), "waiter", "sleep", "0"], env=env(tmp_path, starve_s=2, poll_s=5),
                              stderr=subprocess.DEVNULL)
    assert first.wait(timeout=30) != 0  # the stale holder is stopped
    assert second.wait(timeout=60) == 0 and (tmp_path / "second").exists()
    assert waiter.wait(timeout=60) == 0
