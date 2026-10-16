from pathlib import Path

from tools import thread_cpu


def write_task(root: Path, pid: int, tid: int, comm: str, utime: int, stime: int) -> None:
    task = root / str(pid) / "task" / str(tid)
    task.mkdir(parents=True, exist_ok=True)
    # Fields after "(comm)": state ppid pgrp session tty tpgid flags minflt cminflt majflt cmajflt utime stime ...
    fields = ["S", "1", "1", "1", "0", "-1", "0", "0", "0", "0", "0", str(utime), str(stime)] + ["0"] * 20
    (task / "stat").write_text(f"{tid} ({comm}) " + " ".join(fields) + "\n")


def test_samples_reads_name_and_ticks_per_thread(tmp_path: Path) -> None:
    write_task(tmp_path, 10, 10, "gt_play", 100, 20)
    write_task(tmp_path, 10, 11, "ge worker", 300, 5)  # a name with a space: the parser must not split on it
    assert thread_cpu.sample(10, proc=tmp_path) == {10: ("gt_play", 120), 11: ("ge worker", 305)}


def test_usage_is_percent_of_one_core_between_samples() -> None:
    before = {10: ("gt_play", 100), 11: ("ge", 300)}
    after = {10: ("gt_play", 150), 11: ("ge", 300), 12: ("new", 40)}
    usage = thread_cpu.usage(before, after, seconds=1.0, ticks_per_second=100)
    assert usage == [("gt_play", 10, 50.0), ("new", 12, 40.0), ("ge", 11, 0.0)]


def test_vanished_threads_are_left_out() -> None:
    assert thread_cpu.usage({10: ("a", 5)}, {}, seconds=1.0, ticks_per_second=100) == []


def test_format_lists_the_busiest_first() -> None:
    text = thread_cpu.format_usage([("gt_play", 10, 50.0), ("ge", 11, 12.5)])
    assert text.splitlines() == ["  50.0%  gt_play (10)", "  12.5%  ge (11)"]


def test_find_pid_matches_comm_and_skips_others(tmp_path: Path) -> None:
    for pid, comm in ((20, "bash"), (21, "gt_play")):
        (tmp_path / str(pid)).mkdir()
        (tmp_path / str(pid) / "comm").write_text(comm + "\n")
    (tmp_path / "self").mkdir()
    assert thread_cpu.find_pid("gt_play", proc=tmp_path) == 21
    assert thread_cpu.find_pid("missing", proc=tmp_path) is None


def test_main_prints_one_window_for_a_live_process(capsys) -> None:
    import subprocess
    import sys

    child = subprocess.Popen([sys.executable, "-c", "import time\nwhile True: pass"])
    try:
        assert thread_cpu.main(["--pid", str(child.pid), "--every", "0.3", "--count", "1"]) == 0
    finally:
        child.kill()
        child.wait()
    import re

    out = capsys.readouterr().out
    assert out.startswith("-- 0 s") and re.search(r"\d+\.\d%  ", out)


def test_main_reports_a_missing_name(capsys) -> None:
    assert thread_cpu.main(["--name", "no-such-process-name-xyz"]) == 2
    assert "no process named" in capsys.readouterr().err


def test_main_reports_a_missing_pid(capsys) -> None:
    assert thread_cpu.main(["--pid", "999999999", "--count", "1", "--every", "0.1"]) == 2
    assert "no such process" in capsys.readouterr().err


def test_main_rejects_a_non_positive_interval(capsys) -> None:
    assert thread_cpu.main(["--pid", "1", "--every", "0"]) == 2


def test_format_says_idle_when_nothing_ran() -> None:
    assert thread_cpu.format_usage([]) == "  (idle)"


def test_samples_parse_a_name_with_parentheses(tmp_path: Path) -> None:
    write_task(tmp_path, 30, 30, "a (b) c", 7, 3)
    assert thread_cpu.sample(30, proc=tmp_path) == {30: ("a (b) c", 10)}
