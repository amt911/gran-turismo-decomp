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
