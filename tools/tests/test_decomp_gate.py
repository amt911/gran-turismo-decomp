"""Tests for tools/decomp_gate.py: parsing the differential harness report, summing routes, sabotage."""

from __future__ import annotations

from pathlib import Path

import pytest

from tools import decomp_gate as gate

REPORT = """\
Loaded extracted/EBOOT.elf (243383 relocations)
[decomp] mode=diff functions=3 cases/function=32
Runtime stopped: Wall-clock limit reached at 0x08C7AC18
[decomp] gt::game::race_operator::window_max @0x0885BBBC: compared 1, equal 1, mismatched 0, not comparable 0 (calls 3, cases that changed memory 0)
[decomp] gt::game::func_088D4C9C @0x088D4C9C: compared 4, equal 3, mismatched 1, not comparable 2 (calls 9, cases that changed memory 1)
[decomp]   first mismatch: v0: original 0x00000001, decompiled 0x00000000 (a0=0x09F00000 a1=0x00000000 a2=0x00000000)
[decomp] gt::game::spec_db::get_car_price @0x08930E08: compared 0, equal 0, mismatched 0, not comparable 0 (calls 0, cases that changed memory 0, nested in 3 equal / 1 mismatched cases)
[decomp]   warning: never compared in this run
[decomp] gt::game::x @0x08800000: no recompiled function at 0x08800000, not installed
"""


def test_parse_report_reads_every_function_line():
    stats = gate.parse_report(REPORT)
    assert set(stats) == {0x0885BBBC, 0x088D4C9C, 0x08930E08}
    window = stats[0x0885BBBC]
    assert window.name == "gt::game::race_operator::window_max"
    assert (window.compared, window.equal, window.mismatched, window.not_comparable, window.calls) == (1, 1, 0, 0, 3)
    assert stats[0x088D4C9C].mismatched == 1 and stats[0x088D4C9C].not_comparable == 2


def test_parse_report_keeps_the_first_mismatch_and_counts_not_installed():
    stats = gate.parse_report(REPORT)
    assert stats[0x088D4C9C].first_mismatch.startswith("v0: original")
    assert gate.not_installed(REPORT) == 1
    assert gate.stop_reason(REPORT) == "Wall-clock limit reached at 0x08C7AC18"


def test_stop_reason_is_empty_without_a_stop_line():
    assert gate.stop_reason("nothing here") == ""


def test_combine_sums_the_routes_per_address():
    race = gate.parse_report(REPORT)
    dealer = gate.parse_report(REPORT.replace("compared 1, equal 1", "compared 2, equal 2"))
    total = gate.combine([race, dealer])
    assert total[0x0885BBBC].compared == 3 and total[0x0885BBBC].equal == 3
    assert total[0x08930E08].compared == 0
    assert total[0x088D4C9C].mismatched == 2


def test_summary_counts_equal_mismatched_and_never_compared():
    total = gate.combine([gate.parse_report(REPORT)])
    summary = gate.summarize(total)
    assert summary == gate.Summary(functions=3, compared=3, mismatched=2, never_compared=0)


SOURCE = """\
void func_088D4C9C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { ctx.gpr[2] = 1u; }
void window_max(psprecomp::Runtime &, psprecomp::AllegrexContext &ctx) {
    ctx.gpr[2] = 3u;
}
void untouched(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { }
"""


def test_sabotage_bumps_s0_at_the_entry_of_each_named_function():
    out, missing = gate.sabotage(SOURCE, ["func_088D4C9C", "window_max"])
    assert missing == []
    assert "void func_088D4C9C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { ctx.gpr[16] += 1u; ctx.gpr[2]" in out
    assert "void window_max(psprecomp::Runtime &, psprecomp::AllegrexContext &ctx) { ctx.gpr[16] += 1u;\n" in out
    assert "void untouched(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { }" in out


def test_sabotage_reports_names_it_cannot_find():
    out, missing = gate.sabotage(SOURCE, ["nope"])
    assert missing == ["nope"] and out == SOURCE


def test_registered_names_maps_addresses_to_function_names():
    source = (
        "GT_DECOMP(0x088D4C9Cu, gt::game::func_088D4C9C, gt::ReturnKind::Word);\n"
        "// GT_DECOMP(0x08800000u, gt::game::off, gt::ReturnKind::Word);\n"
        "RACE_OPERATOR(0x0885BBBCu, window_max, Word);\n"
        "GT_DECOMP_DEREF(0x0885C2B4u, 4);\n"
    )
    assert gate.registered_names(source) == {0x088D4C9C: "func_088D4C9C", 0x0885BBBC: "window_max"}


def test_parse_only_accepts_hex_lists_and_files(tmp_path: Path):
    assert gate.parse_only("0x088D4C9C, 0885BBBC") == [0x088D4C9C, 0x0885BBBC]
    listing = tmp_path / "only.txt"
    listing.write_text("0x088D4C9C\n0885bbbc\n")
    assert gate.parse_only(f"@{listing}") == [0x088D4C9C, 0x0885BBBC]
    with pytest.raises(ValueError):
        gate.parse_only("0xnothex")


def test_route_commands_are_capped_and_parallel():
    command = gate.route_shell(Path("build/gt_native"), "fast", "t1", Path("recomp/analysis"))
    assert "race" in command and "dealer" in command and "garage" in command and "wait" in command
    assert "--max-seconds 200" in command and "--decomp diff" in command
    full = gate.route_shell(Path("build/gt_native"), "full", "t1", Path("recomp/analysis"))
    assert "--max-seconds 600" in full and "--max-seconds 310" in full


def test_heavy_runs_in_the_shared_slice_that_caps_all_heavy_jobs_together():
    wrapped = gate.heavy(["cmake", "--build", "build", "-j8"])
    assert wrapped[:4] == ["systemd-run", "--user", "--scope", "--quiet"]
    assert "--slice=gt-heavy.slice" in wrapped
    assert wrapped[-4:] == ["cmake", "--build", "build", "-j8"]


def test_slice_limits_cap_the_total_at_6g_without_swap():
    command = gate.slice_limits()
    assert command[:4] == ["systemctl", "--user", "set-property", "--runtime"]
    assert "gt-heavy.slice" in command
    assert {"MemoryHigh=5G", "MemoryMax=6G", "MemorySwapMax=0"} <= set(command)


def test_heavy_slot_lets_two_jobs_in_and_makes_a_third_wait(tmp_path):
    locks = [tmp_path / "a.lock", tmp_path / "b.lock"]
    first = gate.HeavySlot(locks)
    second = gate.HeavySlot(locks)
    third = gate.HeavySlot(locks)
    assert first.acquire(block=False) and second.acquire(block=False)
    assert not third.acquire(block=False)
    first.release()
    assert third.acquire(block=False)
    second.release()
    third.release()


def test_parse_report_reads_the_nested_counts():
    stats = gate.parse_report(REPORT)
    assert (stats[0x08930E08].nested_equal, stats[0x08930E08].nested_mismatched) == (3, 1)
    assert (stats[0x0885BBBC].nested_equal, stats[0x0885BBBC].nested_mismatched) == (0, 0)  # older report format


def test_a_function_compared_only_nested_counts_as_reached_and_its_nested_mismatch_as_detected():
    total = gate.combine([gate.parse_report(REPORT), gate.parse_report(REPORT)])
    assert total[0x08930E08].nested_equal == 6
    assert gate.reached(total[0x08930E08]) and not gate.reached(gate.Stats("x"))
    assert gate.detected(total[0x08930E08]) and not gate.detected(total[0x0885BBBC])
