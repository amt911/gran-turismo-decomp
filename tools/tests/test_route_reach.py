"""Tests for tools/route_reach.py: which functions a route's probe log reached, and what a new route adds."""

from __future__ import annotations

from pathlib import Path

from tools import route_reach as reach

LOG_A = """\
Loaded extracted/EBOOT.elf
[probe] 0x08804100 hits=3 first=#1 c_08804100
[probe] 0x08804200 hits=1 first=#2 c_08804200
[probe] 0x08804300 hits=0 c_08804300
"""
LOG_B = """\
[probe] 0x08804200 hits=7 first=#1 c_08804200
[probe] 0x08804400 hits=2 first=#2 c_08804400
"""
LOG_C = """\
[probe] 0x08804400 hits=1 first=#1 c_08804400
[probe] 0x08804500 hits=1 first=#2 c_08804500
"""


def test_reached_reads_only_functions_with_hits():
    assert reach.reached(LOG_A) == {0x08804100, 0x08804200}


def test_reached_of_a_log_without_probes_is_empty():
    assert reach.reached("Runtime stopped: Wall-clock limit reached\n") == set()


def test_additions_counts_what_each_route_adds_in_order():
    baseline = reach.reached(LOG_A)
    rows = reach.additions(baseline, [("b", reach.reached(LOG_B)), ("c", reach.reached(LOG_C))])
    assert rows == [
        reach.Row("b", reached=2, new_vs_baseline=1, new_vs_previous=1),
        reach.Row("c", reached=2, new_vs_baseline=2, new_vs_previous=1),
    ]


def test_additions_total_is_the_union():
    baseline = reach.reached(LOG_A)
    total = reach.union(baseline, [reach.reached(LOG_B), reach.reached(LOG_C)])
    assert total == {0x08804100, 0x08804200, 0x08804400, 0x08804500}


def test_load_reads_the_probe_logs_of_a_route(tmp_path: Path):
    (tmp_path / "probe_t_a.log").write_text(LOG_A)
    assert reach.load(tmp_path / "probe_t_a.log") == {0x08804100, 0x08804200}


def test_problem_flags_a_log_of_a_run_that_did_not_finish():
    assert reach.problem("0") is None
    assert "timed out" in reach.problem("124")
    assert "exit 97" in reach.problem("97")
    assert reach.problem("?") is not None
