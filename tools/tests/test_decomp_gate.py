"""Tests for tools/decomp_gate.py: parsing the differential harness report, summing routes, sabotage."""

from __future__ import annotations

import os
import re
import subprocess
import sys
from pathlib import Path

import pytest

from tools import decomp_gate as gate

REPORT = """\
Loaded extracted/EBOOT.elf (243383 relocations)
[decomp] mode=diff functions=3 cases/function=32
[pad] script done at vblank 12345
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


def test_sabotage_bumps_s0_and_a_kernel_ram_word_at_the_entry_of_each_named_function():
    out, missing = gate.sabotage(SOURCE, ["func_088D4C9C", "window_max"])
    assert missing == []
    bump = "ctx.gpr[16] += 1u; {0}.memory().store32(0x08000F00u, {0}.memory().load32(0x08000F00u) + 1u);"
    assert ("void func_088D4C9C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { " + bump.format("rt") +
            " ctx.gpr[2]") in out
    # an unnamed runtime parameter gets a name for the memory bump
    assert ("void window_max(psprecomp::Runtime &sabotage_rt, psprecomp::AllegrexContext &ctx) { " +
            bump.format("sabotage_rt") + "\n") in out
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
    assert all(route in command for route in ("race", "dealer", "garage", "buy", "options")) and "wait" in command
    assert "--max-seconds 200" in command and "--decomp diff" in command
    assert "options.pad" in command and f"--max-seconds {dict(gate.ROUTES['fast'])['options']}" in command
    assert "GT_PAD_EXIT_AFTER" not in command


def test_full_routes_end_after_their_script_not_on_the_wall_clock():
    """A loaded machine runs the 11 parallel routes slower than real time: a wall-clock end cut their scripts short.
    A full route ends a tail of vblanks after its script is done (the race keeps its 600 s of racing as a tail); the
    wall clock, twice the route's length, is only a safety cap below the timeout."""
    full = gate.route_shell(Path("build/gt_native"), "full", "t1", Path("recomp/analysis"))
    race = next(part for part in full.split(") &") if "race.pad" in part)
    buy = next(part for part in full.split(") &") if "buy.pad" in part)
    assert "GT_PAD_EXIT_AFTER=23400" in race and "--max-seconds 1200" in race and "timeout 1320" in race
    assert "GT_PAD_EXIT_AFTER=600" in buy and "--max-seconds 440" in buy and "timeout 560" in buy


def test_full_routes_start_longest_first_and_at_most_max_parallel_at_once():
    """Eleven real-time routes at ~0.5 GB each (measured 2026-10-21: 506 MB) pass the slice's MemoryHigh (5 GB); the
    kernel then throttles them and a real-time route stalls (options timed out three gates running). The full gate
    keeps MAX_PARALLEL_ROUTES running and starts the longest first, so the race still sets its length."""
    full = gate.route_shell(Path("build/gt_native"), "full", "t1", Path("recomp/analysis"))
    started = [route for route in re.findall(r"config/pad_routes/(\w+)\.pad", full)]
    lengths = dict(gate.ROUTES["full"])
    assert started[0] == "race" and [lengths[r] for r in started] == sorted(lengths.values(), reverse=True)
    assert full.count("wait -n") == len(started) and f"-ge {gate.MAX_PARALLEL_ROUTES} ]" in full
    assert gate.MAX_PARALLEL_ROUTES * 0.55 + 0.9 < 5.0  # routes + a clang build in the other slot stay under MemoryHigh


def test_route_cap_leaves_cpu_headroom():
    """A real-time route kept ~3 cores busy before the vblank pacing fix (2.6-3.1 in `top`, 2026-10-22; eight filled all 24
    cores and lost a seek, FINDINGS); since then 0.61-0.67 cores each (four routes of a full gate, load 17, 2026-11-05)."""
    assert gate.MAX_PARALLEL_ROUTES * gate.ROUTE_CORES <= 0.8 * gate.MACHINE_CORES


@pytest.mark.parametrize("cores, routes", [(24, 7), (18, 7), (12, 7), (8, 6), (4, 3), (1, 1)])
def test_route_cap_follows_the_cores_the_gate_may_use(cores, routes):
    """Another session's jobs pinned to CPUs 0-5 left the gate 18 cores, which six routes filled to the last one; two to
    five routes per full gate then lost a seek (2026-10-26). The cap counts the gate's own CPU affinity. Since the
    vblank pacing fix a route uses ~0.65 cores (2026-11-05), so memory (7 x 0.55 GB under MemoryHigh, room for a build in
    the other slot) is the bound on a full machine: with 4 routes the race's 1200 s lane left the rest queued (~35 min)."""
    assert gate.max_parallel_routes(cores) == routes


def test_machine_cores_are_the_cores_this_process_may_run_on():
    assert gate.MACHINE_CORES == len(os.sched_getaffinity(0))
    assert gate.MAX_PARALLEL_ROUTES == gate.max_parallel_routes(gate.MACHINE_CORES)


@pytest.mark.parametrize("kind", ["full", "fast"])
def test_route_shell_never_runs_more_than_the_cap_at_once(tmp_path, kind):
    """Runs the generated line with a stand-in binary that records how many copies are alive. The fast gate too:
    twelve uncapped routes at once passed the slice's MemoryHigh and every one stalled to its timeout (2026-10-23)."""
    fake = tmp_path / "fake"
    fake.write_text("#!/bin/sh\nmkdir -p live; touch live/$$; ls live | wc -l >> peaks; sleep 0.3; rm live/$$\n")
    fake.chmod(0o755)
    command = gate.route_shell(fake, kind, "t1", tmp_path)
    subprocess.run(["bash", "-c", command], cwd=tmp_path, check=True, timeout=30)
    peaks = [int(line) for line in (tmp_path / "peaks").read_text().split()]
    assert len(peaks) == len(gate.ROUTES[kind]) and 1 < max(peaks) <= gate.MAX_PARALLEL_ROUTES
    assert all((tmp_path / f"gt_diff_t1_{route}.exit").read_text().strip() == "0" for route, _ in gate.ROUTES[kind])


def test_heavy_runs_in_the_shared_slice_that_caps_all_heavy_jobs_together():
    wrapped = gate.heavy(["cmake", "--build", "build", "-j8"])
    assert wrapped[:4] == ["systemd-run", "--user", "--scope", "--quiet"]
    assert "--slice=gt-heavy.slice" in wrapped
    assert wrapped[-4:] == ["cmake", "--build", "build", "-j8"]


def test_slice_gives_the_real_time_routes_cpu_priority_over_other_sessions():
    """Another agent session's niced builds and tests (load ~30) still stalled real-time routes (2026-10-22); a high
    CPUWeight on the shared slice makes the kernel serve the gate's routes first, without root."""
    assert "CPUWeight=10000" in gate.slice_limits()


def test_slice_limits_cap_the_total_at_6g_without_swap():
    command = gate.slice_limits()
    assert command[:4] == ["systemctl", "--user", "set-property", "--runtime"]
    assert "gt-heavy.slice" in command
    assert {"MemoryHigh=5G", "MemoryMax=6G", "MemorySwapMax=0"} <= set(command)


def test_routes_outweigh_builds_in_the_heavy_slice(monkeypatch):
    """Real-time routes get CPUWeight=10000 in their scope, builds and the recomp keep the default (100): a clang build in
    another worktree beside a full gate made six routes miss a pad press (exit 97, 2026-11-05)."""
    monkeypatch.delenv("GT_GATE_CPU_LOCK", raising=False)
    routes = gate.route_command(Path("build/gt_native"), "full", "t1", Path("recomp/analysis"))
    assert "CPUWeight=10000" in routes[: routes.index("--")]
    build = gate.heavy(["cmake", "--build", "build"])
    assert not any(part.startswith("CPUWeight") for part in build[: build.index("--")])


def test_routes_take_the_cpu_lock_only_around_the_routes(monkeypatch):
    """GT_GATE_CPU_LOCK=<what>: the routes (the real-time part) run under tools/cpu_lock.sh, the recomp and the build do
    not. Callers wrapped the whole gate, so the lock was held through ~18 min of recomp and build too; with the routes the
    hold passed 30 min and a waiter stopped every route (exit 124, verdict of #482, 2026-11-05)."""
    monkeypatch.delenv("GT_GATE_CPU_LOCK", raising=False)
    plain = gate.route_command(Path("build/gt_native"), "full", "t1", Path("recomp/analysis"))
    assert plain[0] == "systemd-run" and "tools/cpu_lock.sh" not in plain
    monkeypatch.setenv("GT_GATE_CPU_LOCK", "verdict full 482")
    locked = gate.route_command(Path("build/gt_native"), "full", "t1", Path("recomp/analysis"))
    assert locked[:2] == ["tools/cpu_lock.sh", "verdict full 482"] and locked[2:] == plain


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


def test_route_problem_flags_a_timeout_a_missing_report_and_a_crash():
    assert gate.route_problem(REPORT, "0") is None
    assert "timed out" in gate.route_problem(REPORT, "124")
    assert "no report" in gate.route_problem("Running from entry 0x0880410C\n", "0")
    assert "exit 1" in gate.route_problem(REPORT, "1")
    assert gate.route_problem(REPORT, "2") is None  # 2 = mismatches, reported by the per-function lines


def test_a_route_whose_pad_script_did_not_finish_is_a_failure():
    unfinished = REPORT.replace("[pad] script done at vblank 12345\n", "")
    assert "did not finish" in gate.route_problem(unfinished, "0", require_done=True)  # cut short by --max-seconds
    assert gate.route_problem(unfinished, "0") is None  # the fast gate is short on purpose
    assert "timed out" in gate.route_problem(unfinished, "124", require_done=True)  # the more specific problem is first


def test_route_commands_log_the_pad_so_an_unfinished_script_is_seen():
    assert "GT_PAD_LOG=1" in gate.route_shell(Path("build/gt_native"), "fast", "t1", Path("recomp/analysis"))
    assert "GT_PAD_LOG=1" in gate.probe_shell(Path("build/gt_native"), "t1", Path("/s/syms.txt"), Path("recomp/analysis"))


def test_an_exclusive_slot_takes_every_lock(tmp_path):
    locks = [tmp_path / "a.lock", tmp_path / "b.lock"]
    shared = gate.HeavySlot(locks)
    assert shared.acquire(block=False)
    exclusive = gate.HeavySlot(locks, exclusive=True)
    assert not exclusive.acquire(block=False)  # one slot is taken
    shared.release()
    assert exclusive.acquire(block=False)
    other = gate.HeavySlot(locks)
    assert not other.acquire(block=False)  # the exclusive job holds both
    exclusive.release()
    assert other.acquire(block=False)
    other.release()


def test_two_exclusive_waiters_and_a_shared_holder_all_get_through(tmp_path):
    import threading
    locks = [tmp_path / "a.lock", tmp_path / "b.lock"]
    shared = gate.HeavySlot(locks)
    assert shared.acquire(block=False)
    done = []

    def run_exclusive():
        slot = gate.HeavySlot(locks, exclusive=True)
        slot.acquire()  # blocks in lock order: no partial-hold-release-retry, so no livelock or deadlock
        done.append(True)
        slot.release()

    waiters = [threading.Thread(target=run_exclusive) for _ in range(2)]
    for waiter in waiters:
        waiter.start()
    waiters[0].join(0.5)
    assert not done  # a.lock is still held
    shared.release()
    for waiter in waiters:
        waiter.join(5)
    assert done == [True, True]


def test_clear_run_removes_a_tags_old_logs_and_exit_codes(tmp_path):
    for route, _ in gate.ROUTES["full"]:
        (tmp_path / f"gt_diff_t1_{route}.log").write_text("old")
        (tmp_path / f"gt_diff_t1_{route}.exit").write_text("0")
    (tmp_path / "gt_diff_t2_race.log").write_text("other tag")
    gate.clear_run("t1", tmp_path)
    assert [p.name for p in tmp_path.iterdir()] == ["gt_diff_t2_race.log"]


MISMATCH_CASES = """\
[decomp] mismatch case: 0x08900000 nested 0x08900100,0x08900200
[decomp] mismatch case: 0x08900300 nested -
[decomp] mismatch case: 0x08900000 nested 0x08900200
"""


def test_parse_mismatch_cases_reads_each_case_and_what_ran_nested_in_it():
    assert gate.parse_mismatch_cases(MISMATCH_CASES) == {
        (0x08900000, frozenset({0x08900100, 0x08900200})),
        (0x08900300, frozenset()),
        (0x08900000, frozenset({0x08900200})),
    }


def test_a_sabotage_counts_only_where_it_was_the_one_sabotaged_function_in_the_case():
    cases = gate.parse_mismatch_cases(MISMATCH_CASES)
    # parent 0x08900000 and its children sabotaged together: the first case can't tell who caused it, the
    # third can't either; 0x08900300's own case had nothing nested
    assert gate.attributed({0x08900000, 0x08900100, 0x08900200, 0x08900300}, cases) == {0x08900300}
    # children sabotaged without their parent: 0x08900200 alone in the third case; 0x08900100 never alone
    assert gate.attributed({0x08900100, 0x08900200}, cases) == {0x08900200}
    assert gate.attributed({0x08900100}, cases) == {0x08900100}


def test_the_next_sabotage_round_takes_ambiguous_functions_that_never_shared_a_mismatching_case():
    cases = gate.parse_mismatch_cases(MISMATCH_CASES)
    # all four sabotaged: 0x08900300 attributed; the other three only ever mismatched together
    ambiguous = {0x08900000, 0x08900100, 0x08900200}
    first = gate.next_round(ambiguous, cases)
    assert first == {0x08900000}  # lowest address first; its callees shared its cases
    second = gate.next_round(ambiguous - first, cases)
    assert second == {0x08900100}  # 0x08900100 and 0x08900200 shared the first case
    assert gate.next_round(set(), cases) == set()


def test_sabotage_names_an_unnamed_context_parameter():
    source = "void leaf(psprecomp::Runtime &rt, psprecomp::AllegrexContext &) {\n}\n"
    out, missing = gate.sabotage(source, ["leaf"])
    assert missing == []
    assert "psprecomp::AllegrexContext &ctx) { ctx.gpr[16] += 1u;" in out


TEMPLATES = """\
namespace gt::core {
void empty_function(psprecomp::Runtime &, psprecomp::AllegrexContext &) {}
void member_address(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { ctx.gpr[2] = ctx.gpr[4]; }
} // namespace gt::core
#define GT_DECOMP_EMPTY(entry) GT_DECOMP(entry, gt::core::empty_function, gt::ReturnKind::Void)
#define GT_DECOMP_LEAF_(fn, returns, entry, constant)                                                    \\
    static const ::gt::core::LeafRegistrar GT_DECOMP_CAT(gt_leaf_, __LINE__){(entry), (constant)}; \\
    GT_DECOMP(entry, gt::core::fn, returns)
#define GT_DECOMP_MEMBER_ADDRESS(entry, k) GT_DECOMP_LEAF_(member_address, gt::ReturnKind::Word, entry, k)
GT_DECOMP_EMPTY(0x08800010u);
GT_DECOMP_MEMBER_ADDRESS(0x08800020u, 0x3F88u);
GT_DECOMP_MEMBER_ADDRESS(0x08800030u, 0x10u);
// GT_DECOMP_MEMBER_ADDRESS(0x08800040u, 0x14u);
GT_DECOMP(0x08800050u, gt::game::named, gt::ReturnKind::Word);
"""


def test_template_instances_map_to_the_shared_body_their_macro_registers():
    assert gate.template_functions(TEMPLATES) == {0x08800010: "empty_function", 0x08800020: "member_address",
                                                  0x08800030: "member_address"}


def test_sabotage_targets_name_the_shared_body_and_list_every_instance_it_affects():
    names, affected, _ = gate.sabotage_targets(TEMPLATES, {0x08800020, 0x08800050})
    assert names == ["member_address", "named"]
    assert affected == {0x08800020, 0x08800030, 0x08800050}  # 0x...30 shares the sabotaged body


def test_a_named_registration_of_a_shared_body_counts_as_affected_and_unresolvable_targets_are_reported():
    source = TEMPLATES + "GT_DECOMP(0x08800060u, gt::core::member_address, gt::ReturnKind::Word);\n"
    names, affected, unresolved = gate.sabotage_targets(source, {0x08800020, 0x08800099})
    assert names == ["member_address"]
    assert affected == {0x08800020, 0x08800030, 0x08800060, 0x08800099}
    assert unresolved == [0x08800099]


def test_a_case_holding_a_target_and_its_template_sibling_does_not_credit_the_target():
    cases = {(0x08800020, frozenset({0x08800030})), (0x08800030, frozenset())}
    affected = {0x08800020, 0x08800030}
    assert gate.attributed(affected, cases) & {0x08800020} == set()


def test_a_coverage_build_left_configured_is_noticed():
    assert gate.coverage_left_configured("GT_COVERAGE_FILES:STRING=src/game/x.cpp\n")
    assert not gate.coverage_left_configured("GT_COVERAGE_FILES:STRING=\nCMAKE_CXX_COMPILER:STRING=/usr/bin/clang++\n")
    assert not gate.coverage_left_configured("CMAKE_CXX_COMPILER:STRING=/usr/bin/clang++\n")


def test_probe_shell_runs_every_fast_route_uncapped_with_the_decomp_off_and_the_symbols_probed():
    command = gate.probe_shell(Path("build/gt_native"), "t1", Path("/s/syms.txt"), Path("recomp/analysis"))
    assert command.count("--decomp off") == len(gate.ROUTES["fast"]) and command.endswith(" wait")
    assert "GT_PROBE_SYMBOLS=/s/syms.txt" in command and "LCS_UNCAPPED=1" in command
    fast = dict(gate.ROUTES["fast"])
    assert f"--max-seconds {fast['options']}" in command and f"--max-seconds {fast['race']}" in command
    assert "recomp/analysis/probe_t1_options.log" in command and "--decomp diff" not in command


def test_probe_shell_records_each_routes_exit_code_next_to_its_log():
    command = gate.probe_shell(Path("build/gt_native"), "t1", Path("/s/syms.txt"), Path("recomp/analysis"))
    assert command.count("echo $? >") == len(gate.ROUTES["fast"]) and "recomp/analysis/probe_t1_options.exit" in command


def test_a_probe_route_that_times_out_or_exits_non_zero_is_a_failure():
    assert "timed out" in gate.probe_problem("[probe] 0x08804028 hits=1\n", "124")
    assert gate.probe_problem("[probe] 0x08804028 hits=1\n", "139") == "exit 139"
    assert "no exit code" in gate.probe_problem("", "?")
    assert gate.probe_problem("[probe] 0x08804028 hits=1\n[pad] script done at vblank 9\n", "0") is None
    assert "did not finish" in gate.probe_problem("[probe] 0x08804028 hits=1\n", "0", require_done=True)  # partial hit counts
    assert gate.probe_problem("[probe] 0x08804028 hits=1\n", "0") is None


def test_probe_shell_scales_the_route_lengths():
    command = gate.probe_shell(Path("build/gt_native"), "t1", Path("/s/syms.txt"), Path("recomp/analysis"), "full", 1.5)
    # full routes end after their pad script; the scale stretches only their wall-clock cap (2 x length x scale)
    assert "--max-seconds 1800" in command and "--max-seconds 930" in command and "--max-seconds 600" not in command


def test_probe_shell_can_use_the_full_gates_route_lengths():
    """Like the full gate, a full probe route ends a tail after its pad script (dirt stopped short on the wall clock)."""
    command = gate.probe_shell(Path("build/gt_native"), "t1", Path("/s/syms.txt"), Path("recomp/analysis"), "full")
    race = next(part for part in command.split(") &") if "race.pad" in part)
    dirt = next(part for part in command.split(") &") if "dirt.pad" in part)
    assert "GT_PAD_EXIT_AFTER=23400" in race and "--max-seconds 1200" in race and "timeout 1320" in race
    assert "GT_PAD_EXIT_AFTER=600" in dirt and "--max-seconds 840" in dirt


# The compared floor: a full gate whose compared total drops means an installed function stops other code from running
# (FINDINGS: batch 83 and batch 100 lost ~300 compared functions to func_08A9DD14 with 0 mismatches).
def test_floor_allows_run_to_run_noise():
    assert gate.compared_floor_problem(5168, 5208) is None  # the spread seen between full gates on main


def test_floor_fails_a_drop_like_batch_100s():
    problem = gate.compared_floor_problem(4915, 5208)
    assert problem is not None and "4915" in problem and "5208" in problem and "GT_DECOMP_ONLY" in problem


def test_floor_tolerance_is_the_larger_of_50_and_one_percent():
    assert gate.compared_floor_problem(5208 - 52, 5208) is None
    assert gate.compared_floor_problem(5208 - 53, 5208) is not None
    assert gate.compared_floor_problem(950, 1000) is None
    assert gate.compared_floor_problem(949, 1000) is not None


def test_read_floor(tmp_path: Path):
    floor = tmp_path / "floor.txt"
    assert gate.read_floor(floor) is None
    floor.write_text("# the full gate's compared total on main\ncompared 5208\n")
    assert gate.read_floor(floor) == 5208


def test_raised_floor_only_goes_up(tmp_path: Path):
    floor = tmp_path / "floor.txt"
    floor.write_text("compared 5208\n")
    assert gate.raise_floor(floor, 5190) == 5208 and gate.read_floor(floor) == 5208
    assert gate.raise_floor(floor, 5230) == 5230 and gate.read_floor(floor) == 5230
    fresh = tmp_path / "new.txt"
    assert gate.raise_floor(fresh, 100) == 100 and gate.read_floor(fresh) == 100


def test_every_route_has_a_pad_file_and_a_budget_within_the_race():
    for kind in ("fast", "full"):
        names = [route for route, _ in gate.ROUTES[kind]]
        assert len(names) == len(set(names))
        for route, seconds in gate.ROUTES[kind]:
            assert Path(f"config/pad_routes/{route}.pad").exists(), route
            if route != "race":
                assert seconds <= 600, (route, seconds)  # routes run in parallel: none outlasts the race (600 s)


def test_fast_and_full_run_the_same_routes():
    assert [r for r, _ in gate.ROUTES["fast"]] == [r for r, _ in gate.ROUTES["full"]]


def test_sabotage_can_run_the_full_gate(monkeypatch):
    """A function only the full routes reach can't have its sabotage pinned on the fast gate (batch 106 parked 15):
    `sabotage --full` runs the sabotaged build on the full gate instead."""
    calls = []
    monkeypatch.setattr(gate, "_sabotage_run", lambda *a, **k: calls.append((a, k)) or 0)
    assert gate.main(["sabotage", "t1", "--file", "src/x.cpp", "--only", "0x08800000", "--full"]) == 0
    assert calls[0][1].get("kind") == "full"
    assert gate.main(["sabotage", "t1", "--file", "src/x.cpp", "--only", "0x08800000"]) == 0
    assert calls[1][1].get("kind", "fast") == "fast"


def test_probe_shell_never_runs_more_than_the_cap_at_once(tmp_path):
    """The probe starts the same routes as the fast gate, so the same memory cap applies."""
    fake = tmp_path / "fake"
    fake.write_text("#!/bin/sh\nmkdir -p live; touch live/$$; ls live | wc -l >> peaks; sleep 0.3; rm live/$$\n")
    fake.chmod(0o755)
    command = gate.probe_shell(fake, "t1", tmp_path / "symbols.txt", tmp_path)
    subprocess.run(["bash", "-c", command], cwd=tmp_path, check=True, timeout=30)
    peaks = [int(line) for line in (tmp_path / "peaks").read_text().split()]
    assert len(peaks) == len(gate.ROUTES["fast"]) and 1 < max(peaks) <= gate.MAX_PARALLEL_ROUTES


def test_sigterm_still_restores_a_sabotaged_file(tmp_path):
    """`timeout` stops a run with SIGTERM; Python's default dies without `finally`, so a sabotage run killed that way
    left its sabotaged source behind (2026-10-23, mrandom.cpp) and every later build carried it. The CLI turns SIGTERM
    into an exit, so the restore runs."""
    target = tmp_path / "f.cpp"
    target.write_text("original")
    script = (
        "import sys, time, pathlib\n"
        "from tools import decomp_gate as gate\n"
        "gate.exit_on_sigterm()\n"
        f"p = pathlib.Path({str(target)!r})\n"
        "try:\n"
        "    p.write_text('sabotaged'); print('ready', flush=True); time.sleep(30)\n"
        "finally:\n"
        "    p.write_text('original')\n")
    proc = subprocess.Popen([sys.executable, "-c", script], stdout=subprocess.PIPE, text=True,
                            cwd=Path(__file__).resolve().parents[2])
    assert proc.stdout.readline().strip() == "ready"
    proc.terminate()
    proc.wait(timeout=10)
    assert target.read_text() == "original"


def test_clear_run_creates_the_log_directory(tmp_path):
    """A fresh clone has no recomp/analysis/: every route's `> log` failed at once and the gate read no report (the
    PR #291 verdict, 2026-10-24). clear_run prepares the directory the routes write to."""
    logs = tmp_path / "recomp" / "analysis"
    gate.clear_run("t1", logs)
    assert logs.is_dir()


def test_sigterm_restores_pending_files_at_once(tmp_path):
    """A waiter on the CPU lock sends TERM, then KILL after a short grace; unwinding through subprocess.run (which kills
    and waits for its children first) outlasted the grace and left unit_system.cpp sabotaged (2026-10-24). The handler
    writes the pending originals back before anything else."""
    target = tmp_path / "f.cpp"
    target.write_text("original")
    script = (
        "import time, pathlib\n"
        "from tools import decomp_gate as gate\n"
        "gate.exit_on_sigterm()\n"
        f"p = pathlib.Path({str(target)!r})\n"
        "gate.pending_restore(p, 'original')\n"
        "p.write_text('sabotaged'); print('ready', flush=True); time.sleep(30)\n")
    proc = subprocess.Popen([sys.executable, "-c", script], stdout=subprocess.PIPE, text=True,
                            cwd=Path(__file__).resolve().parents[2])
    assert proc.stdout.readline().strip() == "ready"
    proc.terminate()
    proc.wait(timeout=10)
    assert target.read_text() == "original"


SAVE_ROUTES = {"fast": (("race", 200), ("save_tour", 300)), "full": (("race", 600), ("save_tour", 400))}


def test_save_routes_get_their_own_savedata_dir(monkeypatch):
    """A route named save_* starts from a fresh copy of the user's save (spec 2026-10-23-savedata-routes); the others
    keep the default savedata."""
    monkeypatch.setattr(gate, "ROUTES", SAVE_ROUTES)
    for kind in ("fast", "full"):
        command = gate.route_shell(Path("build/gt_native"), kind, "t1", Path("logs"))
        parts = command.split(") &")
        tour = next(p for p in parts if "save_tour.pad" in p)
        race = next(p for p in parts if "race.pad" in p)
        assert "GT_SAVEDATA_DIR=logs/savedata_t1_save_tour" in tour
        assert "GT_SAVEDATA_DIR" not in race
    probe = gate.probe_shell(Path("build/gt_native"), "t1", Path("s.sym"), Path("logs"))
    assert "GT_SAVEDATA_DIR=logs/savedata_probe_t1_save_tour" in next(p for p in probe.split(") &") if "save_tour.pad" in p)


def _save(tmp_path):
    save = tmp_path / "UCES01245-GAMEDAT"
    save.mkdir()
    for name, text in (("GAME.DAT", "data"), ("PARAM.SFO", "sfo")):
        (save / name).write_text(text)
    hashes = tmp_path / "savedata.sha1"
    hashes.write_text("".join(f"{gate.checksums.sha1_of(save / n)}  {n}\n" for n in ("GAME.DAT", "PARAM.SFO")))
    return save, hashes


def test_prepare_saves_copies_a_fresh_save_per_route(tmp_path, monkeypatch):
    monkeypatch.setattr(gate, "ROUTES", SAVE_ROUTES)
    save, hashes = _save(tmp_path)
    logs = tmp_path / "logs"
    stale = logs / "savedata_t1_save_tour" / "UCES01245-GAMEDAT"
    stale.mkdir(parents=True)
    (stale / "GAME.DAT").write_text("written by an earlier run")
    assert gate.prepare_saves("t1", logs, "fast", save, hashes) == ["save_tour"]
    assert (stale / "GAME.DAT").read_text() == "data" and (stale / "PARAM.SFO").read_text() == "sfo"
    assert not (logs / "savedata_t1_race").exists()


def test_prepare_saves_refuses_another_or_missing_save(tmp_path, monkeypatch):
    monkeypatch.setattr(gate, "ROUTES", SAVE_ROUTES)
    save, hashes = _save(tmp_path)
    (save / "GAME.DAT").write_text("another save")
    with pytest.raises(gate.SaveError, match="GAME.DAT"):
        gate.prepare_saves("t1", tmp_path / "logs", "fast", save, hashes)
    with pytest.raises(gate.SaveError, match="game_files"):
        gate.prepare_saves("t1", tmp_path / "logs", "fast", tmp_path / "missing", hashes)


def test_prepare_saves_needs_no_save_without_save_routes(tmp_path, monkeypatch):
    monkeypatch.setattr(gate, "ROUTES", {"fast": (("race", 200),), "full": (("race", 600),)})
    assert gate.prepare_saves("t1", tmp_path / "logs", "fast", tmp_path / "missing", tmp_path / "none.sha1") == []


def test_synth_kind_runs_one_route_then_the_synthetic_cases():
    """docs/superpowers/specs/2026-10-25-synthetic-cases-design.md: one route sets the scene, then GT_SYNTH makes the
    cases. No GT_PAD_EXIT_AFTER: it would end the run before the synthetic phase starts."""
    command = gate.route_shell(Path("build/gt_native"), "synth", "t1", Path("recomp/analysis"))
    assert len(gate.ROUTES["synth"]) == 1
    route = gate.ROUTES["synth"][0][0]
    assert re.findall(r"config/pad_routes/(\w+)\.pad", command) == [route]
    assert f"GT_SYNTH={gate.SYNTH_CASES} " in command and f"GT_SYNTH_SEED={gate.SYNTH_SEED} " in command
    assert "--decomp diff" in command and "GT_PAD_EXIT_AFTER" not in command and "LCS_UNCAPPED=1 " in command
    assert "recomp/analysis/gt_diff_t1_" + route + ".log" in command


def test_route_lines_of_the_other_kinds_never_set_synthetic_cases():
    for kind in ("fast", "full"):
        assert "GT_SYNTH" not in gate.route_shell(Path("build/gt_native"), kind, "t1", Path("recomp/analysis"))


def test_a_synthetic_report_reads_only_its_route():
    """The synth run has one route; reading the full gate's list marked the others missing ("exit ?", 2026-10-25)."""
    assert gate.report_routes("synth") == [route for route, _ in gate.ROUTES["synth"]]
    assert gate.report_routes("fast") == gate.report_routes("full") == [route for route, _ in gate.ROUTES["full"]]


def test_the_synthetic_case_count_is_the_one_route_shell_uses(monkeypatch):
    monkeypatch.setattr(gate, "SYNTH_CASES", 64)
    assert "GT_SYNTH=64 " in gate.route_shell(Path("build/gt_native"), "synth", "t1", Path("recomp/analysis"))


@pytest.mark.parametrize("route, seconds", [(None, 1500), ("dealer", 1500), ("race", 1800)])
def test_the_synthetic_phase_follows_the_route_gt_synth_route_names(route, seconds):
    """Script natives fault in the original after the dealer route: the race objects their holders reach do not exist
    yet. GT_SYNTH_ROUTE picks the route that sets the scene; its run is the full gate's length plus SYNTH_EXTRA."""
    assert gate.synth_routes(route) == ((route or "dealer", seconds),)


def test_an_unknown_synthetic_route_is_refused():
    with pytest.raises(ValueError, match="no full route"):
        gate.synth_routes("nowhere")


def test_a_synthetic_run_after_a_save_route_gets_its_save(monkeypatch):
    # GT_SYNTH_ROUTE=save_garage ran without GT_SAVEDATA_DIR: the route's pad script waited for a load that never
    # came and the run ended with exit 97 (2026-10-27).
    monkeypatch.setitem(gate.ROUTES, "synth", gate.synth_routes("save_garage"))
    line = gate.route_shell(Path("build/gt_native"), "synth", "t1", Path("recomp/analysis"))
    assert "GT_SAVEDATA_DIR=recomp/analysis/savedata_t1_save_garage" in line


def test_several_synthetic_routes_run_side_by_side():
    # The verdict re-checks every function a synthetic route verified: dealer, race and four more (2026-10-27).
    assert gate.synth_routes("dealer,race") == (("dealer", 1500), ("race", 1800))


def test_coverage_can_run_the_synthetic_gate(monkeypatch):
    calls = []
    monkeypatch.setattr(gate, "_coverage_run", lambda *a, **k: calls.append(a) or 0)
    assert gate.main(["coverage", "t1", "--only", "0x08800000", "--synth"]) == 0
    assert gate.main(["coverage", "t1", "--only", "0x08800000"]) == 0
    assert [call[2] for call in calls] == ["synth", "fast"]


def test_coverage_search_passes_its_budget_and_needs_synth(monkeypatch):
    calls = []
    monkeypatch.setattr(gate, "_coverage_run", lambda *a, **k: calls.append((a, k)) or 0)
    assert gate.main(["coverage", "t1", "--only", "0x08800000", "--synth", "--search", "150"]) == 0
    assert calls[0][0][3] == 150
    with pytest.raises(SystemExit):
        gate.main(["coverage", "t1", "--only", "0x08800000", "--search", "150"])


def test_search_environment_sets_the_variables():
    env = gate.search_environment(200, 8)
    assert env == {"GT_SYNTH_SEARCH": "200", "GT_SYNTH_SEARCH_SECONDS": "8"}
    assert gate.search_environment(0, 8) == {}
