from tools import synth_v2 as v2

OFF = """\
[decomp] gt::hot_batch_320::f @0x08810000: compared 8, equal 8, mismatched 0, not comparable 0 (calls 8, cases that changed memory 3, nested in 0 equal / 0 mismatched cases)
[decomp] gt::hot_batch_320::g @0x08810100: compared 0, equal 0, mismatched 0, not comparable 8 (calls 8, cases that changed memory 0, nested in 0 equal / 0 mismatched cases)
[decomp] gt::hot_batch_321::h @0x08810200: compared 0, equal 0, mismatched 0, not comparable 8 (calls 8, cases that changed memory 0, nested in 0 equal / 0 mismatched cases)
"""
ON = """\
[decomp] gt::hot_batch_320::f @0x08810000: compared 16, equal 16, mismatched 0, not comparable 0 (calls 16, cases that changed memory 3, nested in 0 equal / 0 mismatched cases)
[decomp]   v2 cases: compared 8, equal 8
[decomp] gt::hot_batch_320::g @0x08810100: compared 5, equal 5, mismatched 0, not comparable 11 (calls 16, cases that changed memory 2, nested in 0 equal / 0 mismatched cases)
[decomp]   v2 cases: compared 5, equal 5
[decomp] gt::hot_batch_321::h @0x08810200: compared 0, equal 0, mismatched 0, not comparable 16 (calls 16, cases that changed memory 0, nested in 0 equal / 0 mismatched cases)
[decomp]   v2 cases: compared 0, equal 0
"""


def test_parse_v2_reads_the_line_after_each_function():
    assert v2.parse_v2(ON) == {0x08810000: (8, 8), 0x08810100: (5, 5), 0x08810200: (0, 0)}
    assert v2.parse_v2(OFF) == {}


def test_newly_compared_needs_v2_cases_and_no_v1_comparison():
    assert v2.newly_compared(OFF, ON) == {0x08810100}


def test_newly_compared_ignores_mismatching_functions():
    bad = ON.replace("compared 5, equal 5, mismatched 0", "compared 5, equal 4, mismatched 1")
    assert v2.newly_compared(OFF, bad) == set()


def test_table_groups_by_source_file():
    files = {"hot_batch_320": {0x08810000, 0x08810100}, "hot_batch_321": {0x08810200}}
    rows = v2.table(OFF, ON, files)
    assert rows["hot_batch_320"] == v2.Row(installed=2, v1=1, v2_new=1)
    assert rows["hot_batch_321"] == v2.Row(installed=1, v1=0, v2_new=0)


def test_format_table_has_a_total():
    rows = {"a": v2.Row(2, 1, 1), "b": v2.Row(1, 0, 0)}
    text = v2.format_table(rows)
    assert "a" in text and "total" in text.splitlines()[-1] and "3" in text.splitlines()[-1]
