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
    assert all(route in command for route in ("race", "dealer", "garage", "buy")) and "wait" in command
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


def test_route_problem_flags_a_timeout_a_missing_report_and_a_crash():
    assert gate.route_problem(REPORT, "0") is None
    assert "timed out" in gate.route_problem(REPORT, "124")
    assert "no report" in gate.route_problem("Running from entry 0x0880410C\n", "0")
    assert "exit 1" in gate.route_problem(REPORT, "1")
    assert gate.route_problem(REPORT, "2") is None  # 2 = mismatches, reported by the per-function lines


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
