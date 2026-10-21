"""tools.pool: the reached-but-undecompiled pool with its static facts (stores, VFPU, imports within a few calls)."""

from pathlib import Path

from tools import pool
from tools.progress import LOAD_BASE

TEXT_END = 0x1000


def jal(target: int) -> int:
    return 0x0C000000 | (target >> 2)


def j(target: int) -> int:
    return 0x08000000 | (target >> 2)


NOP = 0
JR_RA = 0x03E00008
JALR_V0 = 0x0040F809
SW = 0xAC000000  # sw $zero, 0($zero)
SB = 0xA0000000
LWC1 = 0xC4000000
VFPU_LVQ = 0x36 << 26


def test_describe_counts_stores_calls_vfpu_and_float_instructions():
    words = [SW, SB, jal(0x100), LWC1, VFPU_LVQ, JR_RA, NOP]
    assert pool.describe(words) == (2, 1, 1, 1)


def test_done_addresses_reads_definitions_and_commented_registrations():
    source = """
    void func_08B4B864(psprecomp::Runtime &rt) {}
    HOT101(0x08B77BF4u, func_08B77BF4, DoubleWord);
    // HOT101(0x0881AF28u, func_0881AF28, Word);  // unregistered
    call_set(rt, ctx, 0x08C7B0D0u, {});
    """
    done = pool.done_addresses(source)
    assert {0x08B4B864, 0x08B77BF4, 0x0881AF28} <= done
    assert 0x08C7B0D0 not in done  # a call target is not a definition


def test_walk_imports_follows_jal_and_j_and_stops_at_the_depth():
    code = {
        0x10: [jal(0x20), JR_RA, NOP],
        0x20: [j(0x30), NOP],
        0x30: [jal(TEXT_END + 0x40), NOP, JR_RA],
    }
    stubs, indirect = pool.walk_imports(code.get, 0x10, 5, TEXT_END)
    assert stubs == {TEXT_END + 0x40} and not indirect
    shallow, _ = pool.walk_imports(code.get, 0x10, 1, TEXT_END)
    assert shallow == set()  # the import is two calls below the depth-1 walk


def test_walk_imports_marks_an_indirect_call_and_survives_a_cycle_and_an_unknown_callee():
    code = {0x10: [jal(0x10), jal(0x99), JALR_V0, NOP]}
    stubs, indirect = pool.walk_imports(code.get, 0x10, 5, TEXT_END)
    assert stubs == set() and indirect


def test_build_rows_filters_done_unknown_and_long_functions_and_ranks_by_hits():
    code = {
        0x10: [SW, JR_RA, NOP],
        0x20: [jal(TEXT_END + 8), JR_RA, NOP],
        0x30: [SW] * 50,
        0x40: [SW, JR_RA, NOP],
    }
    sizes = {address: 4 * len(words) for address, words in code.items()}
    hits = {0x10: 5, 0x20: 9, 0x30: 100, 0x40: 7, 0x50: 3}  # 0x50 is not a known function
    rows = pool.build_rows(hits, sizes, code.get, {0x40 + LOAD_BASE}, TEXT_END, max_insns=10, depth=5)
    assert [row.address for row in rows] == [0x20 + LOAD_BASE, 0x10 + LOAD_BASE]  # 0x30 too long, 0x40 done, 0x50 unknown
    assert rows[0].imports and not rows[1].imports
    assert rows[0].stubs == (TEXT_END + 8 + LOAD_BASE,)
    assert rows[1].stores == 1 and rows[1].instructions == 3


def test_build_rows_skips_a_function_whose_words_are_missing():
    rows = pool.build_rows({0x10: 1}, {0x10: 8}, lambda address: None, set(), TEXT_END, max_insns=10, depth=5)
    assert rows == []


def test_format_row_prints_the_stubs_only_on_request():
    row = pool.Row(5, 0x08B207A4, 3, 1, 0, True, False, 0, 2, (0x08C7A888,))
    assert pool.format_row(row, False) == "5 0x08B207A4 3 1 0 1 0 vf0 fp2"
    assert pool.format_row(row, True) == "5 0x08B207A4 3 1 0 1 0 vf0 fp2 8C7A888"


def test_main_prints_the_table_for_a_synthetic_image(tmp_path: Path, monkeypatch, capsys):
    functions = tmp_path / "functions.tsv"
    functions.write_text("10\t12\n20\t8\n")
    log = tmp_path / "probe.log"
    log.write_text("[probe] 0x%08X hits=7 first=#0 c\n[probe] 0x%08X hits=2 first=#0 c\n" % (0x10 + LOAD_BASE, 0x20 + LOAD_BASE))
    src = tmp_path / "src"
    src.mkdir()
    (src / "a.cpp").write_text("void func_%08X() {}\n" % (0x20 + LOAD_BASE))
    words = {0x10: [SW, JR_RA, NOP], 0x20: [JR_RA, NOP]}

    class FakeImage:
        base = 0
        text = bytes(0x1000)

        @staticmethod
        def from_elf(_data):
            return FakeImage()

        def function(self, address, size):
            return words[address], {}

    monkeypatch.setattr(pool, "TextImage", FakeImage)
    elf = tmp_path / "elf"
    elf.write_bytes(b"")
    assert pool.main([str(log), "--elf", str(elf), "--functions-tsv", str(functions), "--src", str(src)]) == 0
    out = capsys.readouterr().out.splitlines()
    assert out[0].startswith("hits addr")
    assert out[1] == "7 0x%08X 3 1 0 0 0 vf0 fp0" % (0x10 + LOAD_BASE)
    assert len(out) == 2  # 0x20 is defined in src/


def test_walk_imports_revisits_a_function_reached_again_with_more_depth():
    # E calls A then B; A calls B. Reached first through A (one call deeper), B used to be marked seen and never re-explored
    # from E with more depth left, so the stub two calls under E was missed (review of #247).
    code = {
        0x10: [jal(0x20), jal(0x30), JR_RA, NOP],  # E
        0x20: [jal(0x30), JR_RA, NOP],             # A
        0x30: [jal(0x40), JR_RA, NOP],             # B
        0x40: [jal(TEXT_END + 0x80), JR_RA, NOP],  # C
    }
    stubs, _ = pool.walk_imports(code.get, 0x10, 2, TEXT_END)
    assert stubs == {TEXT_END + 0x80}  # E -> B -> C -> stub: within 2 calls


def test_done_addresses_ignores_a_mention_in_a_comment_or_a_call():
    source = """
    // the caller passes func_08A5BB7C(0x10) its holder
    (void)func_08B4B864(rt, ctx);
    """
    assert pool.done_addresses(source) == set()  # neither is a definition nor a registration


def test_done_addresses_reads_single_argument_registrations():
    """GT_DECOMP_EMPTY(0x..u) / GT_DECOMP_ZERO(0x..u) register shared bodies: batch 105's pool offered 8 of them again."""
    source = "GT_DECOMP_EMPTY(0x08B5380Cu);\nGT_DECOMP_ZERO( 0x08B6E524u );\n// see 0x08B4AC54u in a comment\n"
    assert pool.done_addresses(source) == {0x08B5380C, 0x08B6E524}
