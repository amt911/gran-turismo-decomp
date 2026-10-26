"""tools.synth_batch: the bookkeeping of a synthetic-cases batch."""

from tools import synth_batch as sb


def test_only_the_named_active_registrations_are_commented():
    source = ("HOT1(0x08800000u, func_08800000, Word);\n"
              "// HOT1(0x08800004u, func_08800004, Word);\n"
              "GT_DECOMP_THUNK(0x08800008u, 0x08900000u);\n"
              "void func_08800000(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { w(rt, ctx, 0x08800008u); }\n")
    out, count = sb.comment_registrations(source, {"0x08800000", "0x08800004", "0x08800008"}, "why")
    assert count == 2
    assert out.splitlines()[0] == "// HOT1(0x08800000u, func_08800000, Word);  // why"
    assert out.splitlines()[1] == "// HOT1(0x08800004u, func_08800004, Word);"  # already commented: left alone
    assert out.splitlines()[2] == "// GT_DECOMP_THUNK(0x08800008u, 0x08900000u);  // why"
    assert out.splitlines()[3].startswith("void func_08800000(")  # a row, not a registration: kept


def test_lines_of_other_addresses_are_never_touched():
    """A cleanup that edited "from the batch marker on" deleted rows it did not own (2026-10-25)."""
    source = "HOT1(0x08800000u, func_08800000, Word);\nHOT1(0x08800010u, func_08800010, Word);\n"
    out, count = sb.comment_registrations(source, {"0x08800010"}, "why")
    assert count == 1 and out.splitlines()[0] == "HOT1(0x08800000u, func_08800000, Word);"


def test_the_never_compared_are_read_from_a_report():
    log = ("[decomp] gt::x::func_08800000 @0x08800000: compared 0, equal 0, mismatched 0, not comparable 8 (calls 8\n"
           "[decomp] gt::x::func_08800004 @0x08800004: compared 3, equal 3, mismatched 0, not comparable 5 (calls 8\n")
    assert sb.never_compared(log) == {"0x08800000"}


def test_the_undetected_are_read_from_a_sabotage_summary():
    out = "sabotage round 1: 3 sabotaged, 1 pinned\nsabotage: 1/3 detected; undetected: ['0x08A65128', '0x08A66AD0']\n"
    assert sb.undetected(out) == {"0x08A65128", "0x08A66AD0"}
    assert sb.undetected("sabotage: 2/2 detected; undetected: []\n") == set()


def test_the_mismatched_are_read_from_a_report_and_never_count_as_verified():
    log = ("[decomp] gt::x::f @0x08C59AE4: compared 9, equal 6, mismatched 3, not comparable 55 (calls 64\n"
           "[decomp] gt::x::g @0x08C59B40: compared 9, equal 9, mismatched 0, not comparable 55 (calls 64\n")
    assert sb.mismatched(log) == {"0x08C59AE4"}


def test_a_run_that_reported_nothing_is_not_a_run_where_everything_compared():
    """A build that failed (duplicate registrations) left an empty report: it was read as "all compared", every
    sabotage then failed to build and the whole batch was commented as undetected (2026-10-26)."""
    assert not sb.report_ok("")
    assert not sb.report_ok("Loaded extracted/EBOOT.elf\ngt_native error: decompiled functions: 0x08A12E38 is registered twice\n")
    assert sb.report_ok("Runtime stopped: Synthetic cases done\n[decomp] gt::x::f @0x08800000: compared 1, equal 1, "
                        "mismatched 0, not comparable 0 (calls 1\n")


def test_a_sabotage_run_that_did_not_build_detects_nothing_and_says_so():
    assert sb.sabotage_ran("sabotage round 1: 2 sabotaged, 2 pinned\nsabotage: 2/2 detected; undetected: []\n")
    assert not sb.sabotage_ran("sabotage: the sabotaged build did not run\n")
